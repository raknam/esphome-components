/*
  Based on: https://github.com/weetmuts/wmbusmeters/blob/master/src/driver_izar.cc
  Copyright (C) 2019 Jacek Tomasiak (gpl-3.0-or-later)
  Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)
  Copyright (C) 2021 Vincent Privat (gpl-3.0-or-later)
*/

#pragma once

#include "driver.h"

#define GET_BIT(var, pos) ((var >> pos) & 0x01)

struct Izar: Driver
{
  Izar() : Driver(std::string("izar")) {};
  virtual esphome::optional<std::map<std::string, float>> get_values(std::vector<unsigned char> &telegram) override {
    std::map<std::string, float> ret_val{};

    add_to_map(ret_val, "total_water_m3", this->get_total_water_m3(telegram));
    add_to_map(ret_val, "last_month_total_water_m3", this->get_last_month_total_water_m3(telegram));
    add_to_map(ret_val, "transmit_period_s", this->get_transmit_period_s(telegram));
    add_to_map(ret_val, "remaining_battery_life_y", this->get_remaining_battery_life_y(telegram));
    add_to_map(ret_val, "current_alarms", this->get_current_alarms(telegram));
    add_to_map(ret_val, "previous_alarms", this->get_previous_alarms(telegram));

    if (ret_val.size() > 0) {
      return ret_val;
    }
    else {
      return {};
    }
  };

private:
  esphome::optional<float> get_current_alarms(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val = 0;
    // ret_val |= (telegram[11] >> 7)       << 1; // general_alarm
    // ret_val |= (telegram[12] >> 7)       << 2; // leakage_currently
    // ret_val |= (telegram[12] >> 5 & 0x1) << 3; // meter_blocked
    // ret_val |= (telegram[13] >> 7)       << 4; // back_flow
    // ret_val |= (telegram[13] >> 6 & 0x1) << 5; // underflow
    // ret_val |= (telegram[13] >> 5 & 0x1) << 6; // overflow
    // ret_val |= (telegram[13] >> 4 & 0x1) << 7; // submarine
    // ret_val |= (telegram[13] >> 3 & 0x1) << 8; // sensor_fraud_currently
    // ret_val |= (telegram[13] >> 1 & 0x1) << 9; // mechanical_fraud_currently
    return ret_val;
  };

  esphome::optional<float> get_previous_alarms(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val = 0;
    // ret_val |= (telegram[12] >> 6 & 0x1) << 2; // leakage_previously
    // ret_val |= (telegram[13] >> 2 & 0x1) << 8; // sensor_fraud_previously
    // ret_val |= (telegram[13] & 0x1)      << 9; // mechanical_fraud_previously
    return ret_val;
  };

  esphome::optional<float> get_remaining_battery_life_y(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val{};
    ret_val = (telegram[12] & 0x1F) / 2.0;
    return ret_val;
  };

  esphome::optional<float> get_transmit_period_s(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val{};
    ret_val = 1 << ((telegram[11] & 0x0F) + 2);
    return ret_val;
  };

  esphome::optional<float> get_total_water_m3(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val{};
    uint8_t *decoded = reinterpret_cast<uint8_t*>(telegram.data());
    uint8_t decoded_len = telegram.size();
    uint8_t decrypted[64] = {0};
    if ((this->decrypt(decoded, decoded_len, decrypted)) > 0) {
      ret_val = (this->uintFromBytesLittleEndian(decrypted + 1)) / 1000.0;
    }
    return ret_val;
  };

  esphome::optional<float> get_last_month_total_water_m3(std::vector<unsigned char> &telegram) {
    esphome::optional<float> ret_val{};
    uint8_t *decoded = reinterpret_cast<uint8_t*>(telegram.data());
    uint8_t decoded_len = telegram.size();
    uint8_t decrypted[64] = {0};
    if ((this->decrypt(decoded, decoded_len, decrypted)) > 0) {
      ret_val = (this->uintFromBytesLittleEndian(decrypted + 5)) / 1000.0;
    }
    return ret_val;
  };

  const uint8_t decoder[64] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0x03,0xFF,0x01,0x02,0xFF,
    0xFF,0xFF,0xFF,0x07,0xFF,0xFF,0x00,0xFF,
    0xFF,0x05,0x06,0xFF,0x04,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0x0B,0xFF,0x09,0x0A,0xFF,
    0xFF,0x0F,0xFF,0xFF,0x08,0xFF,0xFF,0xFF,
    0xFF,0x0D,0x0E,0xFF,0x0C,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
  };

  uint32_t uintFromBytes(uint8_t* data) {
    uint32_t result = data[0] << 24;
    result += data[1] << 16;
    result += data[2] << 8;
    result += data[3];
    return result;
  }

  uint32_t uintFromBytesLittleEndian(uint8_t* data) {
    uint32_t result = data[3] << 24;
    result += data[2] << 16;
    result += data[1] << 8;
    result += data[0];
    return result;
  }

  uint32_t hashShiftKey(uint32_t key) {
    for (uint8_t i = 0; i < 8; i++) {
      uint8_t bit = GET_BIT(key, 1) ^ GET_BIT(key, 2) ^
                    GET_BIT(key, 11) ^ GET_BIT(key, 31);
      key <<= 1;
      key |= bit;
    }
    return key;
  }

  uint8_t decrypt(uint8_t* encoded, uint8_t len, uint8_t* decoded) {
    if (len < 15) {
      return 0;
    }

    uint32_t key = 0xdfd109e8;
    key ^= uintFromBytes(encoded + 2);
    key ^= uintFromBytes(encoded + 6);
    key ^= uintFromBytes(encoded + 10);

    const uint8_t size = len - 15;
    for (uint8_t i = 0; i < size; i++) {
      key = hashShiftKey(key);
      decoded[i] = encoded[i + 15] ^ (key & 0xFF);
    }

    if (decoded[0] != 0x4B) {
      return 0;
    }

    return size;
  }
};