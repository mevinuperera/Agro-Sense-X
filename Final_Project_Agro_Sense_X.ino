#include "DHT.h"
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === Pin Definitions ===
#define DHTPIN 4
#define DHTTYPE DHT11
#define FAN_PIN 26
#define SOIL_PIN 14
#define TDS_PIN 34
#define MOTOR_PIN 25  // Motor pin for watering

// === WiFi Credentials ===
const char* SSID = "Test";
const char* PASSWORD = "123456789";

// === Firebase Credentials (WORKING one) ===
#define FIREBASE_HOST "project-agro-sense-x-516a9-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_API_KEY "#######################################"

// === Sensor Objects ===
DHT dht(DHTPIN, DHTTYPE);

// === Firebase Objects ===
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// === DS18B20 Sensor Setup ===
#define ONE_WIRE_BUS 5
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// === Variables ===
float greenhouseTemp = 0.0;
float greenhouseHumidity = 0.0;
float ds18b20Temp = 0.0;
String soilStatus = "";
float tdsValue = 0.0;
unsigned long sendDataPrevMillis = 0;
unsigned long previousMillis = 0;
const long interval = 86400000; // 24 hours in milliseconds
unsigned long lastWateringTime = 0;

void setup() {
  Serial.begin(115200);
  dht.begin();
  sensors.begin();

  // Set pin modes
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, HIGH); // FAN off by default
  pinMode(SOIL_PIN, INPUT);
  pinMode(MOTOR_PIN, OUTPUT); // Motor pin for watering

  // === Connect to WiFi ===
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ Connected to WiFi");
  Serial.println(WiFi.localIP());

  // === Firebase Setup ===
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_HOST;
  config.cert.data = nullptr;
  config.signer.test_mode = true;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // Wait for Firebase readiness
  unsigned long timeout = millis();
  while (!Firebase.ready() && millis() - timeout < 10000) {
    Serial.print(".");
    delay(500);
  }
  if (Firebase.ready()) {
    Serial.println("\n✅ Firebase is ready!");
  } else {
    Serial.println("\n❌ Firebase not ready. Check config or internet.");
  }
}

void loop() {
  if (!Firebase.ready()) {
    Serial.println("⚠️ Firebase not ready. Skipping upload...");
    delay(5000);
    return;
  }

  // === Read Sensors ===
  greenhouseHumidity = dht.readHumidity();
  greenhouseTemp = dht.readTemperature();

  // Read DS18B20 temperature
  sensors.requestTemperatures();
  ds18b20Temp = sensors.getTempCByIndex(0);

  int soilValue = digitalRead(SOIL_PIN);
  soilStatus = (soilValue == LOW) ? "WET" : "DRY";

  int rawTDS = analogRead(TDS_PIN);
  float tdsVoltage = rawTDS * (3.3 / 4095.0);
  tdsValue = rawTDS * 0.376; // Calibration factor from fish tank code

  // === Fan Control ===
  if (greenhouseTemp > 25.0) {
    digitalWrite(FAN_PIN, LOW); // Turn on FAN
  } else {
    digitalWrite(FAN_PIN, HIGH); // Turn off FAN
  }

  // === Motor Control (Watering the plant) ===
  // Check if soil is dry
  if (soilStatus == "DRY") {
    digitalWrite(MOTOR_PIN, LOW); // Turn on the motor
    lastWateringTime = millis();
  } else {
    digitalWrite(MOTOR_PIN, HIGH); // Turn off the motor if the soil is wet
  }

  // === Automatic Watering at 8 AM ===
  unsigned long currentMillis = millis();
  if (currentMillis - lastWateringTime >= interval) { // Water every 24 hours
    int hour = (currentMillis / 3600000) % 24; // Get the current hour

    if (hour == 8 && soilStatus == "DRY") {
      digitalWrite(MOTOR_PIN, HIGH); // Turn on the motor at 8 AM
      Serial.println("🌱 Automatic watering at 8 AM...");
      lastWateringTime = millis();
    }
  }

  // === Send data every 5 seconds ===
  if (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0) {
    sendDataPrevMillis = millis();
    Serial.println("📤 Uploading to Firebase...");
    Firebase.RTDB.setFloat(&fbdo, "/greenHouse/temperature", greenhouseTemp);
    Firebase.RTDB.setFloat(&fbdo, "/greenHouse/humidity", greenhouseHumidity);
    Firebase.RTDB.setFloat(&fbdo, "/watertemp/temp_ds18b20", ds18b20Temp); // Upload DS18B20 temp
    Firebase.RTDB.setString(&fbdo, "/greenHouse/soil_status", soilStatus);
    Firebase.RTDB.setFloat(&fbdo, "/watertemp/tds", tdsValue);

    // === Debug Output ===
    Serial.print("DHT11 Temp: "); Serial.println(greenhouseTemp);
    Serial.print("Humidity: "); Serial.println(greenhouseHumidity);
    Serial.print("DS18B20 Temp: "); Serial.println(ds18b20Temp);
    Serial.print("Soil: "); Serial.println(soilStatus);
    Serial.print("TDS (ppm): "); Serial.println(tdsValue);
  }
}
