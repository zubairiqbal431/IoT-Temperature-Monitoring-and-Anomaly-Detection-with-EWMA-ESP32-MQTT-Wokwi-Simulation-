#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

// ---------------- WIFI ----------------
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ---------------- MQTT ----------------
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

// ---------------- SENSOR ----------------
const int DHT_PIN = 32;
DHTesp dht;

// ---------------- INTELLIGENCE ----------------
float ewmaTemp = 0;
const float alpha = 0.2;
const float anomaly_margin = 2.5;

// ---------------- TIMING ----------------
unsigned long lastMsg = 0;
unsigned long lastMqttRetry = 0;
const int interval = 3000;
const int retryInterval = 5000;

// ---------------- STATE CONTROL ----------------
bool wifiStarted = false;

// ---------------- WIFI SETUP (RUN ONCE) ----------------
void setup_wifi() {
  if (wifiStarted) return;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  wifiStarted = true;
  Serial.println("WiFi starting...");
}

// ---------------- MQTT RECONNECT ----------------
void reconnectMQTT() {

  if (WiFi.status() != WL_CONNECTED) return;

  if (client.connected()) return;

  if (millis() - lastMqttRetry < retryInterval) return;

  lastMqttRetry = millis();

  Serial.print("MQTT connecting...");

  if (client.connect("ESP32_IOT_Node_001")) {
    Serial.println("connected");
  } else {
    Serial.print("failed, state=");
    Serial.println(client.state());
  }
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);

  dht.setup(DHT_PIN, DHTesp::DHT22);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

// ---------------- LOOP ----------------
void loop() {

  // ---------------- WIFI CHECK ----------------
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connecting...");
    delay(1000);
    return;
  }

  // ---------------- MQTT CHECK ----------------
  reconnectMQTT();
  client.loop();

  // ---------------- SENSOR TIMING ----------------
  if (millis() - lastMsg < interval) return;
  lastMsg = millis();

  TempAndHumidity data = dht.getTempAndHumidity();

  float temp = data.temperature;
  float hum = data.humidity;

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Sensor error");
    return;
  }

  // ---------------- EWMA FILTER ----------------
  if (ewmaTemp == 0) ewmaTemp = temp;
  else ewmaTemp = (alpha * temp) + (1 - alpha) * ewmaTemp;

  float threshold = ewmaTemp + anomaly_margin;
  float deviation = abs(temp - ewmaTemp);

  // ---------------- PRINT ----------------
  Serial.println("Temp: " + String(temp));
  Serial.println("EWMA: " + String(ewmaTemp));
  Serial.println("Threshold: " + String(threshold));

  // ---------------- MQTT PUBLISH ----------------
  if (client.connected()) {
    client.publish("zubair/iot/temp", String(temp).c_str());
    client.publish("zubair/iot/hum", String(hum).c_str());
    client.publish("zubair/iot/ewma", String(ewmaTemp).c_str());
  }

  // ---------------- ANOMALY DETECTION ----------------
  if (deviation > anomaly_margin) {
    Serial.println("⚠ ANOMALY DETECTED");
    client.publish("zubair/iot/alert", "ANOMALY");
  }

  Serial.println("----------------------");
}