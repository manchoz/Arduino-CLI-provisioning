#include "bootloader.h"
#include <FlashIAP.h>
#include <Ticker.h>

#ifndef CORE_CM7
#error Update the bootloader by uploading the sketch to the M7 core instead of the M4 core.
#endif

#define BOOTLOADER_ADDR (0x8000000)
mbed::FlashIAP flash;

uint32_t bootloader_data_offset = 0x1F000;
uint8_t* bootloader_data = (uint8_t*)(BOOTLOADER_ADDR + bootloader_data_offset);

#include <chrono>
using namespace std::chrono_literals;

mbed::Ticker blinker;
volatile auto ledStatus { true };

void blink()
{
    digitalWrite(LED_BUILTIN, ledStatus ? LOW : HIGH);
    ledStatus = !ledStatus;
}

void setup()
{
    Serial.begin(115200);
    for (const auto timeout = millis() + 2500; !Serial && timeout < millis(); delay(500))
        ;

    delay(2500);

    uint8_t currentBootloaderVersion = bootloader_data[1];
    uint8_t availableBootloaderVersion = (envie_bootloader_mbed_bin + bootloader_data_offset)[1];

    Serial.println("Magic Number (validation): " + String(bootloader_data[0], HEX));
    Serial.println("Bootloader version: " + String(currentBootloaderVersion));

    if (availableBootloaderVersion >= currentBootloaderVersion) {
        pinMode(LED_BUILTIN, OUTPUT);
        blinker.attach(blink, 25ms);
        applyUpdate(BOOTLOADER_ADDR);
        blinker.detach();
        digitalWrite(LED_BUILTIN, HIGH);
    }
}

void applyUpdate(uint32_t address) {
  long len = envie_bootloader_mbed_bin_len;

  flash.init();

  const uint32_t page_size = flash.get_page_size();
  char *page_buffer = new char[page_size];
  uint32_t addr = address;
  uint32_t next_sector = addr + flash.get_sector_size(addr);
  bool sector_erased = false;
  size_t pages_flashed = 0;
  uint32_t percent_done = 0;

  while (true) {

    if (page_size * pages_flashed > len) {
      break;
    }

    // Erase this page if it hasn't been erased
    if (!sector_erased) {
      flash.erase(addr, flash.get_sector_size(addr));
      sector_erased = true;
    }

    // Program page
    flash.program(&envie_bootloader_mbed_bin[page_size * pages_flashed], addr, page_size);

    addr += page_size;
    if (addr >= next_sector) {
      next_sector = addr + flash.get_sector_size(addr);
      sector_erased = false;
    }

    if (++pages_flashed % 3 == 0) {
      uint32_t percent_done_new = page_size * pages_flashed * 100 / len;
      if (percent_done != percent_done_new) {
        percent_done = percent_done_new;
        Serial.println("Flashed " + String(percent_done) + "%");
      }
    }
  }
  Serial.println("Flashed 100%");

  delete[] page_buffer;

  flash.deinit();
  Serial.println("Bootloader update complete. You may now disconnect the board.");
  Serial.println("BOOTLOADER UPDATE DONE");
}

void loop()
{
    delay(1000);
}
