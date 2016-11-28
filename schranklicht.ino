
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
// over-the-air update ...
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// wifi/mqtt configuration (fixme: store in fs?!)
const char* ssid = "*******";
const char* password = "************************";
const char* mqtt_server = "192.168.1.70";
const char *mqtt_pass =  "************************";

// rgb strip configuration
const uint16_t PixelCount = 50; // kueche
const uint8_t PixelPin = D8;    // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 220

const uint8_t ButtonPin = D6;
int ledState = LOW;
int buttonState;
int lastButtonState = HIGH;
long lastDebounceTime = 0;
long debounceDelay = 70;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
//int value = 0;

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount+10);


RgbColor red(colorSaturation, 0, 0);
RgbColor green(0, colorSaturation, 0);
RgbColor blue(0, 0, colorSaturation);
RgbColor white(colorSaturation);
RgbColor whiteish(210, 210, 200);
RgbColor black(0);


void setup()
{
    Serial.begin(115200);
    while (!Serial); // wait for serial attach

    Serial.println();
    Serial.println("Initializing WIFI ...");
    Serial.flush();

    setupWifi();

    client.setServer(mqtt_server, 5482);
    client.setCallback(callback);

    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();

    pinMode(ButtonPin, INPUT);
    Serial.print("button pin: ");
    Serial.println(digitalRead(ButtonPin));

    Serial.println("OTA Setup ...");
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);
  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");
  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready ...");

}

void setupWifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void switchLedStrip(const RgbColor& color) {
  RgbColor off(0);

  // apply the color to the strip
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
    strip.SetPixelColor(pixel, color);
  }
  strip.Show();
  for (uint16_t pixel = 0; pixel < PixelCount; pixel++) {
    strip.SetPixelColor(pixel, color);
  }
  strip.Show();
  delay(50);

  if (off == color)
    client.publish("/kueche/schranklicht/status", "OFF");
  else
    client.publish("/kueche/schranklicht/status", "ON");

  //String rgb = String(color.R, HEX) + String(color.G, HEX) + String(color.B, HEX);
  char rgb_c[7];
  sprintf(rgb_c, "%02X%02X%02X", color.R, color.G, color.B);
  client.publish("/kueche/schranklicht/rgb", rgb_c);
}


void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  if (0 == strcmp("/kueche/schranklicht/setrgb", topic)) {
    Serial.println("SETRGB");
    if (6 == length) {
      long r, g, b;
      long number = strtol((char*)&payload[0], NULL, 16);
      r = number >> 16;
      g = number >> 8 & 0xFF;
      b = number & 0xFF;

      RgbColor newColor(r, g, b);
      switchLedStrip(newColor);
    }
  }

  if (0 == strcmp("/kueche/schranklicht/setcolor", topic)) {
    Serial.println("SETCOLOR");

    // if (((char)payload[0] == '#') && (7 == length)) {
    //   // here payload shall be #rrggbb ... now extract the three decimal values
    //   // Message arrived [/test/setcolor] #11eeA7
    //   long r, g, b;
    //   long number = strtol((char*)&payload[1], NULL, 16);
    //   r = number >> 16;
    //   g = number >> 8 & 0xFF;
    //   b = number & 0xFF;

    //   RgbColor newColor(r, g, b);
    //   // Serial.print("r="); Serial.println(r);
    //   // Serial.print("g="); Serial.println(g);
    //   // Serial.print("b="); Serial.println(b);

    //   switchLedStrip(newColor);
    // }

    if (0 == memcmp("blue", payload, 4)) {
      switchLedStrip(blue);
    }
    if (0 == memcmp("green", payload, 5)) {
      switchLedStrip(green);
    }
    if (0 == memcmp("red", payload, 3)) {
      switchLedStrip(red);
    }

  }

  // Switch on the LED if an 1 was received as first character
  if (0 == strcmp("/kueche/schranklicht/set", topic)) {
    if ((length > 1) &&
        ((char)payload[0] == 'O') &&
        ((char)payload[1] == 'N')) {
      ledState = HIGH;
      client.publish("outTopic", "> LED ON");

      // apply the color to the strip
      switchLedStrip(whiteish);
    } else {
      ledState = LOW;
      client.publish("outTopic", "> LED OFF");
      switchLedStrip(black);
    }
  }
}

void mqttConnect() {
  Serial.print("Attempting MQTT connection...");
  // Attempt to connect
  if (client.connect("wemosD1", "nodemcu1", MQTT_PASS,
                     "/kueche/schranklicht/status", 1, 1, "OFFLINE")) {
    Serial.println("connected");
    // Once connected, publish an announcement...
    client.publish("/kueche/schranklicht/status", "CONNECTED", true);
    // ... and resubscribe
    client.subscribe("/kueche/schranklicht/setcolor");
    client.subscribe("/kueche/schranklicht/setrgb");
    client.subscribe("/kueche/schranklicht/set");
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println(" try again in 1 seconds");
    // Wait 1 (was 5) seconds before retrying
    delay(1000);
  }
}

void handleButton() {
    int reading = digitalRead(ButtonPin);
    if (reading != lastButtonState) {
      lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (reading != buttonState) {
        buttonState = reading;
        // only toggle the LED if the new button state is LOW (pull-up!)
        if (buttonState == LOW) {
          ledState = !ledState;
          switchLedStrip(ledState ? whiteish : black);
          client.publish("outTopic", "> LED toggle");
        }
      }
    }
    lastButtonState = reading;
}

void loop()
{
    while (!client.connected()) {
      mqttConnect();
      handleButton();
    }

    client.loop();

    ArduinoOTA.handle();

    handleButton();
}
