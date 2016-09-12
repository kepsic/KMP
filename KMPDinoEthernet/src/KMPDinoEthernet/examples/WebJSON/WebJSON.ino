// This is fork from WebTemperature.ino by Andres Kepler
// Company: KMP Electronics Ltd, Bulgaria
// Web: http://kmpelectronics.eu/
// License: See the GNU General Public License for more details at http://www.gnu.org/copyleft/gpl.html
// Supported boards: 
//		KMP DiNo II NETBOARD V1.0. Web: http://kmpelectronics.eu/en-us/products/dinoii.aspx
//		ProDiNo NetBoard V2.1. Web: http://kmpelectronics.eu/en-us/products/prodinoethernet.aspx
// Description:
//		Web JSON server to check One Wire temperature sensors DS18B20 or DS18S20, monitor/contorl relays  and opto inputs. Sensors (one or many) must connect to board. 
//      example output: 
//      curl http://192.168.1.199/?r3=On
//{"sensors":[{"name":"28C0A61604000081","value":52.50},{"name":"28ECF606040000E7","value":52.00},{"name":"28FF42B810140032","value":25.00},{"name":"28FF136B1414007D","value":24.50}],"relays":[{"name":"relay1","status":"On"},{"name":"relay2","status":"Off"},{"name":"relay3","status":"On"},{"name":"relay4","status":"Off"}],"inputs":[{"name":"in1","status":"Off"},{"name":"in2","status":"Off"},{"name":"in3","status":"Off"},{"name":"in4","status":"Off"}]}
//     To turn relays on/off just use http://DEV.IP.ADDR/?r3=On or http://DEV.IP.ADDR/?r3=Off where r1..r4 are
relay numbers 
//Depends from ArduinoJSON https://github.com/bblanchon/ArduinoJson
//        
// Example link: http://kmpelectronics.eu/en-us/examples/dinoii/webtemperaturecheck.aspx
// Version: 1.1.1
// Date: 11.09.2016
// Author: Plamen Kovandjiev <p.kovandiev@kmpelectronics.eu>, Andres Kepler <andres@kepler.ee>



// Ethernet headers
#include <SPI.h>

// To trick compiler just empty file 
#include <KMP.h>
#include "Ethernet.h"
#include "KmpDinoEthernet.h"
#include "KMPCommon.h"
#include <ArduinoJson.h>

// One Wire headers
#include <OneWire.h>
#include <DallasTemperature.h>

// If in debug mode - print debug information in Serial. Comment in production code, this bring performance.
// This method is good for development and verification of results. But increases the amount of code and decreases productivity.

//#define DEBUG

// Thermometer Resolution in bits. http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf page 8. 
// Bits - CONVERSION TIME. 9 - 93.75ms, 10 - 187.5ms, 11 - 375ms, 12 - 750ms. 
#define TEMPERATURE_PRECISION 9


// Enter a MAC address and IP address for your controller below.
byte     _mac[] = { 0x80, 0x9B, 0x20, 0xF1, 0x46, 0x3B };
// The IP address will be dependent on your local network.
byte      _ip[] = { 192, 168, 1, 199 };                  
// Gateway.
byte _gateway[] = { 192, 168, 1, 1 };
// Sub net mask
byte  _subnet[] = { 255, 255, 255, 0 };

// Buffer to Hex bytes.
char _buffer[10];

// Local port.
// Port 80 is default for HTTP
const uint16_t LOCAL_PORT = 80;



// Initialize the Ethernet server library.
// with the IP address and port you want to use. 
EthernetServer _server(LOCAL_PORT);

// Client.
EthernetClient _client;

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire _oneWire(OneWirePin);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature _sensors(&_oneWire);

// Number of one wire temperature devices found.
int _oneWireDeviceCount; 

// Temp device address.
DeviceAddress _tempDeviceAddress; 


/**
* \brief Setup void. Arduino executed first. Initialize DiNo board and prepare Ethernet connection.
*
*
* \return void
*/
void setup()
{
#ifdef DEBUG
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
    //while (!Serial) {
    //	; // wait for serial port to connect. Needed for Leonardo only. If need debug setup() void.
    //}
#endif

    // Init Dino board. Set pins, start W5200.
    DinoInit();

    // Start the Ethernet connection and the server.
    Ethernet.begin(_mac, _ip, _gateway, _subnet);
    _server.begin();

#ifdef DEBUG
    Serial.println("The server is starting.");
    Serial.println(Ethernet.localIP());
    Serial.println(Ethernet.gatewayIP());
    Serial.println(Ethernet.subnetMask());
#endif
    
    // Start the One Wire library.
    _sensors.begin();

    // Set precision.
    _sensors.setResolution(TEMPERATURE_PRECISION);

    // Select available connected to board One Wire devices.
    GethOneWireDevices();
}

/**
* \brief Loop void. Arduino executed second.
*
*
* \return void
*/
void loop(){
    // listen for incoming clients
    _client = _server.available();

    if(!_client.connected())
    {
        return;
    }

#ifdef DEBUG
    Serial.println("Client connected.");
#endif

    // If client connected On status led.
    OnStatusLed();

    // Read client request to prevent overload input buffer.
    ReadClientRequest();

    // Write client response - html page.
    WriteClientResponse();

    // Close the connection.
    _client.stop();

    // If client disconnected Off status led.
    OffStatusLed();

#ifdef DEBUG
    Serial.println("#Client disconnected.");
    Serial.println("---");
#endif
}

/**
 * \brief ReadClientRequest void. Read client request. Not need any operation.
 *        First row of client request is similar to:
 *        GET / HTTP/1.1
 *
 *        You can check communication client-server get program Smart Sniffer: http://www.nirsoft.net/utils/smsniff.html
 *        or read debug information from Serial Monitor (Tools > Serial Monitor).
 *
 * \return void
 */
bool ReadClientRequest()
{
#ifdef DEBUG
    Serial.println("Read client request.");
#endif



    int requestLen = 0;
    bool result = false;
    bool isGet = false;
    char c;
    int relay = -1;
    char status[4];
    int statusPos = 0;

    // Loop while read all request.
    while (_client.available())
    {
        ++requestLen;
        c = _client.read();

        #ifdef DEBUG
        Serial.write(c);
        #endif
        // Check request is GET.
        if(requestLen == 1 && c != 'G')
        break;
        
        // Check for GET / HTTP/1.1
        
        if(requestLen == 6)
        result = c == CH_SPACE;

        // Not other valid data to read. Read to end.
        if(requestLen > 12)
        break;

 


        // Read only needed chars.
        // Read relay.
        if (requestLen == 8)
        {
            // convert char to int.
            relay = CharToInt(c) - 1;
        }


        // Read max 3 chars for On or Off.
        if (requestLen >= 10 && requestLen <= 12)
        {
            status[statusPos] = c;
            statusPos++;
            // End of string.
            status[statusPos] = 0;

            // If On.
            if(strcmp(status, W_ON) == 0)
            {
                SetRelayStatus(relay, true);
                result = true;
                break;
            }

            // If Off.
            if(strcmp(status, W_OFF) == 0)
            {
                SetRelayStatus(relay, false);
                result = true;
                break;
            }

            
        }
    }

    // Fast flush request.
    while (_client.available())
    {
        c = _client.read();
        #ifdef DEBUG
        Serial.write(c);
        #endif
    }

#ifdef DEBUG
    Serial.println("#End client request.");
#endif

    return result;
}


/**
 * \brief WriteJsonResponse void. Write json response.
 * 
 * 
 * \return void
 */
void WriteJsonResponse() {

      if (!_client.connected())
        return;

  

  
  // Inside the brackets, 200 is the size of the pool in bytes.
  // If the JSON object is more complex, you need to increase that value.

  //StaticJsonBuffer<1024> jsonBuffer;
  
  // StaticJsonBuffer allocates memory on the stack, it can be
  // replaced by DynamicJsonBuffer which allocates in the heap.
  // It's simpler but less efficient.
  //
  //char* content = malloc(_oneWireDeviceCount*sizeof(int16_t));
  DynamicJsonBuffer  jsonBuffer;

  // Create the root of the object tree.
  //
  // It's a reference to the JsonObject, the actual bytes are inside the
  // JsonBuffer with all the other nodes of the object tree.
  // Memory is freed when jsonBuffer goes out of scope.
  JsonObject& root = jsonBuffer.createObject();

  JsonArray& sensors = root.createNestedArray("sensors");
  JsonArray& relays = root.createNestedArray("relays");
  JsonArray& inputs = root.createNestedArray("inputs");
  
    // Send the command to get temperatures.
    _sensors.requestTemperatures(); 

    // Response write.
    // Send a standard http header.
    _client.writeln("HTTP/1.1 200 OK");
    _client.writeln("Content-Type: application/json;charset=utf-8");
    _client.writeln("Server: Arduino");
    _client.writeln("Connection: close");
    _client.writeln("");

//Inputs
   for (int i = 0; i < OPTOIN_COUNT; i++)
    {
        char* inputStatus;
        if(GetOptoInStatus(i) == true)
        {
            inputStatus = (char*)W_ON;
        }
        else
        {
            inputStatus = (char*)W_OFF;
        }

        JsonObject& input = inputs.createNestedObject();
        String inputname = "in"+(String)(i+1);
        input["name"] = inputname;
        input["status"] = (String)inputStatus;
  
    }

//Relays
     for (int i = 0; i < RELAY_COUNT; i++)
    {
        
        char* relayStatus;
     
        if(GetRelayStatus(i) == true)
        {
            relayStatus = (char*)W_ON;
        }
        else
        {
            relayStatus = (char*)W_OFF;
        }

        JsonObject& relay = relays.createNestedObject();
        String relayname = "relay"+(String)(i+1);
        relay["name"] = relayname;
        relay["status"] = (String)relayStatus;

    }
//Sensors
    for (int i = 0; i < _oneWireDeviceCount; i++)
    {

        float temp;
        bool sensorAvaible = _sensors.getAddress(_tempDeviceAddress, i);
        if(sensorAvaible)
        {
         // Get temperature in Celsius.
         temp = _sensors.getTempC(_tempDeviceAddress);

         JsonObject& sensor = sensors.createNestedObject();
         sensor["name"] = DeviceStringAddress(_tempDeviceAddress);
         sensor["value"] = temp;      

         }

    }


    int len = root.measureLength();
    char buffer[len+1];
    root.printTo(buffer, sizeof(buffer));
    _client.write(buffer);
    #ifdef DEBUG
    root.printTo(Serial);
    #endif

    


}

/**
 * \brief WriteClientResponse void. Write json response.
 * 
 * 
 * \return void
 */
void WriteClientResponse()
{

    if (!_client.connected())
        return;
    WriteJsonResponse();


}
/**
 * \brief Get all available One Wire devices.
 * 
 * 
 * \return void
 */

void GethOneWireDevices()
{
    // Grab a count of devices on the wire.
    _oneWireDeviceCount = _sensors.getDeviceCount();

#ifdef DEBUG
    Serial.print("Number of One Wire devices: ");
    Serial.println(_oneWireDeviceCount);

    // Loop through each device, print out address
    for(int i = 0; i < _oneWireDeviceCount; i++)
    {
        // Search the wire for address
        if(_sensors.getAddress(_tempDeviceAddress, i))
        {
            Serial.print("Device ");
            Serial.print(i);
            Serial.print(" with address: ");
            PrintAddress(_tempDeviceAddress);
        }
        else
        {
            Serial.print("Found ghost device at ");
            Serial.print(i);
            Serial.print(" but could not detect address. Check power and cabling");
        }
    }
#endif
}

#ifdef DEBUG
/**
 * \brief Print device address to Serial.
 * 
 * \param deviceAddress Device address.
 * 
 * \return void
 */
void PrintAddress(DeviceAddress deviceAddress)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        ByteToHexStr(deviceAddress[i], _buffer);
        Serial.print(_buffer);
    }
    Serial.println();
}
#endif

/**
 * \brief convert device address to String.
 * 
 * \param deviceAddress Device address.
 * 
 * \return String
 */
String DeviceStringAddress(DeviceAddress deviceAddress)
{
    String s;
    for (uint8_t i = 0; i < 8; i++)
    {
        ByteToHexStr(deviceAddress[i], _buffer);
        s = String(s+(String)_buffer);
    }
    
    return s;
}
