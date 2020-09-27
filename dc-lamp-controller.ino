#include <SPI.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>

// wifi
const char SSID[] = "ssid";
const char PASS[] = "password";

int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;

// mqtt
const IPAddress MQTT_SERVER(192, 168, 16, 11);
const int MQTT_PORT = 1883;
const char* DEVICE_ID = "living_room_lamp_1";
const char* MODE_TOPIC = "Home/Living Room Lamp/light_mode";
const char* LEVEL_TOPIC = "Home/Living Room Lamp/light_level";
const int MQTT_UPDATE_FREQ = 100;
WiFiClient client2;
PubSubClient mqtt_client(client2);
int mqtt_tics = 0;

// pin connections
const int CONTROL_PIN   = 3;
const int BUTTON_PIN    = 4;
const int INDICATOR_PIN = 13;

// constants
const int OFF_BRIGHTNESS   = 0;
const int MAX_BRIGHTNESS   = 255;
const int LIGHT_LEVELS     = 10;
const int TIC_LENGTH       = 100;

// light modes
const char* const LEVEL_NAMES[] = { "STATIC", "FLASH", "BREATH", "PULSE" };
const int STATIC_MODE = 0;
const int FLASH_MODE  = 1;
const int BREATH_MODE = 2;
const int PULSE_MODE  = 3;

// light state
int light_level = OFF_BRIGHTNESS;
int light_mode = STATIC_MODE;
int mode_data = OFF_BRIGHTNESS;
int tics = 0;
bool button_state = false;

void setup()
{
  // init serial
  Serial.begin(9600);
  Serial.println("Serial initialized");

  // init pins
  pinMode(CONTROL_PIN,   OUTPUT);
  pinMode(BUTTON_PIN,    INPUT);
  pinMode(INDICATOR_PIN, OUTPUT);

  analogWrite(CONTROL_PIN, OFF_BRIGHTNESS);
  digitalWrite(INDICATOR_PIN, LOW);

  // check for the presence of NIC
  if (WiFi.status() == WL_NO_SHIELD)
  {
    Serial.println("WiFi shield not present");
    //while (true);
  }

  if (WiFi.firmwareVersion() != "1.1.0")
  {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED)
  {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(SSID); 

    // Connect to WPA/WPA2 network
    status = WiFi.begin(SSID, PASS);
    delay(10000);
  }

  mqtt_client.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt_client.setCallback(onUpdate);
  server.begin();
  printWifiStatus();
  
  // boot up sequence
  for (int i = 0; i < 3; ++i)
  {
    digitalWrite(INDICATOR_PIN, HIGH);
    delay(500);
    digitalWrite(INDICATOR_PIN, LOW);
    delay(500);
  }
}

void loop()
{
  // check for new button press
  bool new_button_state = digitalRead(BUTTON_PIN);
  if (new_button_state == LOW && button_state == HIGH)
  {
    // toggle light
    if (light_mode == STATIC_MODE && light_level == OFF_BRIGHTNESS)
    {
      light_mode = STATIC_MODE;
      light_level = MAX_BRIGHTNESS;
    }
    else
    {
      light_mode = STATIC_MODE;
      light_level = OFF_BRIGHTNESS;
    }
  }
  button_state = new_button_state;

  // check for incoming clients
  client = server.available();
  if (client)
  {
    String currentLine = "";
    while (client.connected())
    {
      // parse request
      // format: command:data
      String path = "";
      String command = "";
      String data = "";
      boolean post = false;
      while (client.available())
      {
        char c = client.read();
        if (c == '\n')
        {
          if (currentLine.startsWith("POST") || currentLine.startsWith("GET"))
          {
            path = currentLine.substring(currentLine.indexOf(" ")+1);
            path = path.substring(0, path.indexOf(" "));
          }
          if (currentLine.startsWith("POST"))
          {
            post = true;
          }
          currentLine = "";
        }
        else if (c != '\r')
        {
          currentLine += c;
        }
      }
      if (post && currentLine.length() > 0)
      {
        int idx = currentLine.indexOf(":");
        command = currentLine.substring(0, idx);
        data = currentLine.substring(idx+1);
      }
      
      if (command.length() > 0)
      {
        Serial.print("Path: ");
        Serial.println(path);
        Serial.print("Command: ");
        Serial.println(command);
        Serial.print("Data: ");
        Serial.println(data);

        // Check the client request
        String req = currentLine.substring(currentLine.lastIndexOf('/')+1);
        command.toUpperCase();
        if (data.length() > 0)
        {
          mode_data = data.toInt();
        }
        else
        {
          mode_data = MAX_BRIGHTNESS;
        }
        if (command == "ON")
        {
          light_mode = STATIC_MODE;
        }
        else if (command == "OFF")
        {
          light_mode = STATIC_MODE;
          mode_data = OFF_BRIGHTNESS;
        }
        else if (command == "UP")
        {
          light_mode = STATIC_MODE;
          if (data.length() > 0)
          {
            mode_data = light_level + data.toInt();
          }
          else
          {
            mode_data = light_level + MAX_BRIGHTNESS / LIGHT_LEVELS;
          }
        }
        else if (command == "DOWN")
        {
          light_mode = STATIC_MODE;
          if (data.length() > 0)
          {
            mode_data = light_level - data.toInt();
          }
          else
          {
            mode_data = light_level - MAX_BRIGHTNESS / LIGHT_LEVELS;
          }
        }
        else if (command == "FLASH")
        {
          light_mode = FLASH_MODE;
        }
        else if (command == "BREATH")
        {
          light_mode = BREATH_MODE;
        }
        else if (command == "PULSE")
        {
          light_mode = PULSE_MODE;
        }
        tics = 0;
      
        // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
        // and a content-type so the client knows what's coming, then a blank line:
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();

        // the content of the HTTP response follows the header:
        client.print("SSID: ");
        client.print(WiFi.SSID());
        client.print("<br>");
        client.print("IP Address: ");
        client.print(WiFi.localIP());
        client.print("<br>");
        client.print("Signal Strength (RSSI): ");
        client.print(WiFi.RSSI());
        client.print(" dBm");
        client.print("<br>");
        client.print("Brightness: ");
        client.print(100 * (light_level) / MAX_BRIGHTNESS);
        client.print("% (");
        client.print(light_level);
        client.print(")");
        client.println();
      }
      else if (path == "/get_mode")
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println(light_mode);
      }
      else if (path == "/get_brightness")
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println(light_level);
      }
      else if (path == "/get_brightness_pct")
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.print(100 * light_level / 255);
        client.println("%");
      }
      else if (path == "/get_json")
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:application/json");
        client.println();
        client.print("{ \"light_mode\": ");
        client.print(light_mode);
        client.print(", \"light_level\": ");
        client.print(light_level);
        client.print(", \"light_pct\": ");
        client.print(100 * light_level / 255);
        client.println(" }");
      }
      else if (post)
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println("Invalid command");
      }
      else
      {
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/html");
        client.println();
        client.println("Light");
      }
      
      client.stop();
    }
  }

  int tics_per_side = 0;
  int delta_per_side = 0;
    
  // set light value
  switch (light_mode)
  {
  case STATIC_MODE:
    light_level = mode_data;
    break;
    
  case FLASH_MODE:
    if (tics >= 2000 / TIC_LENGTH)
    {
      tics = 0;
    }
    if (tics >= 1000 / TIC_LENGTH)
    {
      light_level = mode_data;
    }
    else
    {
      light_level = OFF_BRIGHTNESS;
    }
    break;
    
  case BREATH_MODE:
    tics_per_side = 1000 / TIC_LENGTH;
    delta_per_side = mode_data / tics_per_side;
    if (tics >= 4000 / TIC_LENGTH)
    {
      tics = 0;
    }
    else if (tics >= 3000 / TIC_LENGTH)
    {
      // decreasing
      light_level -= delta_per_side;
    }
    else if (tics >= 2000 / TIC_LENGTH)
    {
      // on
      light_level = mode_data;
    }
    else if (tics >= 1000 / TIC_LENGTH)
    {
      // increasing
      light_level += delta_per_side;
    }
    else
    {
      // min
      light_level = OFF_BRIGHTNESS;
    }
    break;
    
  case PULSE_MODE:
    tics_per_side = 2000 / TIC_LENGTH;
    delta_per_side = mode_data / tics_per_side;
    if (tics > 2 * tics_per_side)
    {
      tics = 0;
    }
    else if (tics > tics_per_side)
    {
      // decreasing
      light_level -= delta_per_side;
    }
    else
    {
      // increasing
      light_level += delta_per_side;
    }
    break;
  }

  // keep brightness within bounds
  if (light_level > MAX_BRIGHTNESS)
  {
    light_level = MAX_BRIGHTNESS;
  }
  else if (light_level < OFF_BRIGHTNESS)
  {
    light_level = OFF_BRIGHTNESS;
  }

  // indicator light is inverse of actual light
  if (light_mode == STATIC_MODE && light_level == OFF_BRIGHTNESS)
  {
    digitalWrite(INDICATOR_PIN, HIGH);
  }
  else
  {
    digitalWrite(INDICATOR_PIN, LOW);
  }

  // output
  analogWrite(CONTROL_PIN, light_level);
  //Serial.println(light_level);

  // mqtt
  if (mqtt_tics == 0)
  {
    if (!mqtt_client.connected())
    {
      mqtt_client.connect(DEVICE_ID);
      mqtt_client.subscribe(MODE_TOPIC);
      mqtt_client.subscribe(LEVEL_TOPIC);
    }
    mqtt_client.loop();
    mqtt_client.publish(MODE_TOPIC, LEVEL_NAMES[light_mode]);
    char buff[3];
    mqtt_client.publish(LEVEL_TOPIC, itoa(mode_data, buff, 10));
  }
  ++mqtt_tics;
  if (mqtt_tics == MQTT_UPDATE_FREQ)
  {
    mqtt_tics = 0;
  }

  // tic
  delay(TIC_LENGTH);
  ++tics;
}

void onUpdate(char* topic, byte* payload, unsigned int len)
{
  // TODO updates come in super delayed and appear to get worse over time
  // TODO look into preventing updates from Arduino coming through
  Serial.print("Got update on ");
  Serial.print(topic);
  Serial.print(" of length ");
  Serial.println(len);
  char value[len];
  for (int i = 0; i < len; ++i)
  {
    value[i] = (char) payload[i];
  }
  if (strcmp(topic, MODE_TOPIC) == 0)
  {
    // TODO seems to have ! after externally pushed strings, but length doesn't include it
    Serial.println(value);
    for (int i = 0; i < 4; ++i)
    {
      if (strcmp(LEVEL_NAMES[i], value) == 0)
      {
        if (light_mode != i)
        {
          light_mode = i;
          Serial.print("Changed light mode to: ");
          Serial.println(i);
        }
        break;
      }
    }
  }
  else if (strcmp(topic, LEVEL_TOPIC) == 0)
  {
    int level = String(value).toInt();
    if (level != mode_data)
    {
      Serial.print("Changed light level to: ");
      Serial.println(value);
      mode_data = level;
    }
  }
}

void printWifiStatus()
{
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  long rssi = WiFi.RSSI();
  Serial.print("Signal Strength (RSSI): ");
  Serial.print(rssi);
  Serial.println(" dBm");
  
  Serial.print("Brightness: ");
  Serial.print(100 * light_level / 255);
  Serial.println("%");
}
