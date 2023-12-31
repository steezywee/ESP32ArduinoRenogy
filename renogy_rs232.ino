#include <ModbusMaster.h>
#include <SoftwareSerial.h>

// Adjust these pins according to your ESP8266 board's layout
#define HC12_TX_PIN D2  // Connect to HC12 TX
#define HC12_RX_PIN D3  // Connect to HC12 RX

// ESP8266 only has one hardware serial, so we'll use it for Modbus
#define MODBUS_RX D5
#define MODBUS_TX D6

// Create a SoftwareSerial object for the HC12
SoftwareSerial hc12(HC12_TX_PIN, HC12_RX_PIN); 
SoftwareSerial modbusSerial(MODBUS_RX, MODBUS_TX); // D5 (RX), D6 (TX)
ModbusMaster node;

// Number of registers to check
const uint32_t num_data_registers = 35;
const uint32_t num_info_registers = 17;

bool simulator_mode = false;

// Controller data structure
struct Controller_data { 
  uint8_t battery_soc;               // percent
  float battery_voltage;             // volts
  float battery_charging_amps;       // amps
  uint8_t battery_temperature;       // celcius
  uint8_t controller_temperature;    // celcius
  float load_voltage;                // volts
  float load_amps;                   // amps
  uint8_t load_watts;                // watts
  float solar_panel_voltage;         // volts
  float solar_panel_amps;            // amps
  uint8_t solar_panel_watts;         // watts
  float min_battery_voltage_today;   // volts
  float max_battery_voltage_today;   // volts
  float max_charging_amps_today;     // amps
  float max_discharging_amps_today;  // amps
  uint8_t max_charge_watts_today;    // watts
  uint8_t max_discharge_watts_today; // watts
  uint8_t charge_amphours_today;     // amp hours
  uint8_t discharge_amphours_today;  // amp hours
  uint8_t charge_watthours_today;    // watt hours
  uint8_t discharge_watthours_today; // watt hours
  uint8_t controller_uptime_days;    // days
  uint8_t total_battery_overcharges; // count
  uint8_t total_battery_fullcharges; // count
    // convenience values
  float battery_temperatureF;        // fahrenheit
  float controller_temperatureF;     // fahrenheit
  float battery_charging_watts;      // watts. necessary? Does it ever differ from solar_panel_watts?
  long last_update_time;             // millis() of last update time
  bool controller_connected;         // bool if we successfully read data from the controller
};
Controller_data renogy_data;

// Controller info structure
struct Controller_info {
  uint8_t voltage_rating;            // volts
  uint8_t amp_rating;                // amps
  uint8_t discharge_amp_rating;      // amps
  uint8_t type;
  uint8_t controller_name;
  char software_version[40];
  char hardware_version[40];
  char serial_number[40];
  uint8_t modbus_address;  

  float wattage_rating;
  long last_update_time;           // millis() of last update time
};
Controller_info renogy_info;

// Function prototypes
void renogy_read_data_registers();
void renogy_read_info_registers();
void renogy_control_load(bool state);

void setup() {
  Serial.begin(9600);
  Serial.println("Started!");

  modbusSerial.begin(9600); // Modbus uses SoftwareSerial
  node.begin(255, modbusSerial); // Modbus on SoftwareSerial

  // Initialize HC12 on SoftwareSerial
  hc12.begin(9600); 
  Serial.println("HC12 initialized");
}

void loop() {
  renogy_read_data_registers();
  renogy_read_info_registers();
     // Send each data field over HC12 and Serial as separate strings
    String dataMessage;

    dataMessage = "home/exterior/barn/stat/solar/battperc:" + String(renogy_data.battery_soc);
    hc12.println(dataMessage);
    Serial.println(dataMessage);

    dataMessage = "home/exterior/barn/stat/solar/battvolt:" + String(renogy_data.battery_voltage);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
      
    dataMessage = "home/exterior/barn/stat/solar/chrgamps:" + String(renogy_data.battery_charging_amps);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500);
     
    dataMessage = "home/exterior/barn/stat/solar/loadamps:" + String(renogy_data.load_amps);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/pnlvolt:" + String(renogy_data.solar_panel_voltage);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/pnlamps:" + String(renogy_data.solar_panel_amps);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/minbv:" + String(renogy_data.min_battery_voltage_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/maxbv:" + String(renogy_data.max_battery_voltage_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/maxchrg:" + String(renogy_data.max_charging_amps_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/maxdchrg:" + String(renogy_data.max_discharging_amps_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/chrgah:" + String(renogy_data.charge_amphours_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/dchrgah:" + String(renogy_data.discharge_amphours_today);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
    dataMessage = "home/exterior/barn/stat/solar/connect:" + String(renogy_data.controller_connected);
    hc12.println(dataMessage);
    Serial.println(dataMessage);
    delay(500); 
    
  delay(10000); 
}

void renogy_read_data_registers() {
 uint8_t j, result;
  uint16_t data_registers[num_data_registers];
  char buffer1[40], buffer2[40];
  uint8_t raw_data;

  // prints data about each read to the console
  bool print_data=0; 
  
  result = node.readHoldingRegisters(0x100, num_data_registers);
  if (result == node.ku8MBSuccess)
  {
    if (print_data) Serial.println("Successfully read the data registers!");
    renogy_data.controller_connected = true;
    for (j = 0; j < num_data_registers; j++)
    {
      data_registers[j] = node.getResponseBuffer(j);
      if (print_data) Serial.println(data_registers[j]);
    }

    renogy_data.battery_soc = data_registers[0]; 
    renogy_data.battery_voltage = data_registers[1] * .1; // will it crash if data_registers[1] doesn't exist?
    renogy_data.battery_charging_amps = data_registers[2] * .1;

    renogy_data.battery_charging_watts = renogy_data.battery_voltage * renogy_data.battery_charging_amps;
    
    //0x103 returns two bytes, one for battery and one for controller temp in c
    uint16_t raw_data = data_registers[3]; // eg 5913
    renogy_data.controller_temperature = raw_data/256;
    renogy_data.battery_temperature = raw_data%256; 
    // for convenience, fahrenheit versions of the temperatures
    renogy_data.controller_temperatureF = (renogy_data.controller_temperature * 1.8)+32;
    renogy_data.battery_temperatureF = (renogy_data.battery_temperature * 1.8)+32;

    renogy_data.load_voltage = data_registers[4] * .1;
    renogy_data.load_amps = data_registers[5] * .01;
    renogy_data.load_watts = data_registers[6];
    renogy_data.solar_panel_voltage = data_registers[7] * .1;
    renogy_data.solar_panel_amps = data_registers[8] * .01;
    renogy_data.solar_panel_watts = data_registers[9];
     //Register 0x10A - Turn on load, write register, unsupported in wanderer - 10
    renogy_data.min_battery_voltage_today = data_registers[11] * .1;
    renogy_data.max_battery_voltage_today = data_registers[12] * .1; 
    renogy_data.max_charging_amps_today = data_registers[13] * .01;
    renogy_data.max_discharging_amps_today = data_registers[14] * .1;
    renogy_data.max_charge_watts_today = data_registers[15];
    renogy_data.max_discharge_watts_today = data_registers[16];
    renogy_data.charge_amphours_today = data_registers[17];
    renogy_data.discharge_amphours_today = data_registers[18];
    renogy_data.charge_watthours_today = data_registers[19];
    renogy_data.discharge_watthours_today = data_registers[20];
    renogy_data.controller_uptime_days = data_registers[21];
    renogy_data.total_battery_overcharges = data_registers[22];
    renogy_data.total_battery_fullcharges = data_registers[23];
    renogy_data.last_update_time = millis();

    // Add these registers:
    //Registers 0x118 to 0x119- Total Charging Amp-Hours - 24/25    
    //Registers 0x11A to 0x11B- Total Discharging Amp-Hours - 26/27    
    //Registers 0x11C to 0x11D- Total Cumulative power generation (kWH) - 28/29    
    //Registers 0x11E to 0x11F- Total Cumulative power consumption (kWH) - 30/31    
    //Register 0x120 - Load Status, Load Brightness, Charging State - 32    
    //Registers 0x121 to 0x122 - Controller fault codes - 33/34

    if (print_data) Serial.println("---");
  }
  else 
  {
    if (result == 0xE2) 
    {
    Serial.println("Timed out reading the data registers!");
    }
    else 
    {
      Serial.print("Failed to read the data registers... ");
      Serial.println(result, HEX); // E2 is timeout
    }
    // Reset some values if we don't get a reading
    renogy_data.controller_connected = false;
    renogy_data.battery_voltage = 0; 
    renogy_data.battery_charging_amps = 0;
    renogy_data.battery_soc = 0;
    renogy_data.battery_charging_amps = 0;
    renogy_data.controller_temperature = 0;
    renogy_data.battery_temperature = 0;    
    renogy_data.solar_panel_amps = 0;
    renogy_data.solar_panel_watts = 0;
    renogy_data.battery_charging_watts = 0;
    if (simulator_mode) {
      renogy_data.battery_voltage = 13.99;    
      renogy_data.battery_soc = 55; 
    }
  }


}

void renogy_read_info_registers() {
uint8_t j, result;
  uint16_t info_registers[num_info_registers];
  char buffer1[40], buffer2[40];
  uint8_t raw_data;

  // prints data about the read to the console
  bool print_data=0;
  
  result = node.readHoldingRegisters(0x00A, num_info_registers);
  if (result == node.ku8MBSuccess)
  {
    if (print_data) Serial.println("Successfully read the info registers!");
    for (j = 0; j < num_info_registers; j++)
    {
      info_registers[j] = node.getResponseBuffer(j);
      if (print_data) Serial.println(info_registers[j]);
    }

    // read and process each value
    //Register 0x0A - Controller voltage and Current Rating - 0
    // Not sure if this is correct. I get the correct amp rating for my Wanderer 30 (30 amps), but I get a voltage rating of 0 (should be 12v)
    raw_data = info_registers[0]; 
    renogy_info.voltage_rating = raw_data/256; 
    renogy_info.amp_rating = raw_data%256;
    renogy_info.wattage_rating = renogy_info.voltage_rating * renogy_info.amp_rating;
    //Serial.println("raw ratings = " + String(raw_data));
    //Serial.println("Voltage rating: " + String(renogy_info.voltage_rating));
    //Serial.println("amp rating: " + String(renogy_info.amp_rating));


    //Register 0x0B - Controller discharge current and type - 1
    raw_data = info_registers[1]; 
    renogy_info.discharge_amp_rating = raw_data/256; // not sure if this should be /256 or /100
    renogy_info.type = raw_data%256; // not sure if this should be /256 or /100

    //Registers 0x0C to 0x13 - Product Model String - 2-9
    // Here's how the nodeJS project handled this:
    /*
    let modelString = '';
    for (let i = 0; i <= 7; i++) {  
        rawData[i+2].toString(16).match(/.{1,2}/g).forEach( x => {
            modelString += String.fromCharCode(parseInt(x, 16));
        });
    }
    this.controllerModel = modelString.replace(' ','');
    */

    //Registers 0x014 to 0x015 - Software Version - 10-11
    itoa(info_registers[10],buffer1,10); 
    itoa(info_registers[11],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.software_version, buffer1); 
    //Serial.println("Software version: " + String(renogy_info.software_version));

    //Registers 0x016 to 0x017 - Hardware Version - 12-13
    itoa(info_registers[12],buffer1,10); 
    itoa(info_registers[13],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.hardware_version, buffer1);
    //Serial.println("Hardware version: " + String(renogy_info.hardware_version));

    //Registers 0x018 to 0x019 - Product Serial Number - 14-15
    // I don't think this is correct... Doesn't match serial number printed on my controller
    itoa(info_registers[14],buffer1,10); 
    itoa(info_registers[15],buffer2,10);
    strcat(buffer1, buffer2); // should put a divider between the two strings?
    strcpy(renogy_info.serial_number, buffer1);
    //Serial.println("Serial number: " + String(renogy_info.serial_number)); // (I don't think this is correct)

    renogy_info.modbus_address = info_registers[16];
    renogy_info.last_update_time = millis();
  
    if (print_data) Serial.println("---");
  }
  else
  {
    if (result == 0xE2) 
    {
      Serial.println("Timed out reading the info registers!");
    }
    else 
    {
      Serial.print("Failed to read the info registers... ");
      Serial.println(result, HEX); // E2 is timeout
    }
    // anything else to do if we fail to read the info reisters?
  }
}

void renogy_control_load(bool state) {
  if (state==1) node.writeSingleRegister(0x010A, 1);  // turn on load
  else node.writeSingleRegister(0x010A, 0);  // turn off load
}
