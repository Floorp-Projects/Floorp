/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetoothuuid_h__
#define mozilla_dom_bluetooth_bluetoothuuid_h__

namespace mozilla {
namespace dom {
namespace bluetooth {

namespace BluetoothServiceUuid {
  static unsigned long long Headset      = 0x0000110800000000;
  static unsigned long long HeadsetAG    = 0x0000111200000000;
  static unsigned long long Handsfree    = 0x0000111E00000000;
  static unsigned long long HandsfreeAG  = 0x0000111F00000000;
  static unsigned long long ObjectPush   = 0x0000110500000000;

  static unsigned long long BaseMSB     = 0x0000000000001000;
  static unsigned long long BaseLSB     = 0x800000805F9B34FB;
}

namespace BluetoothServiceUuidStr {
  static const char* Headset       = "00001108-0000-1000-8000-00805F9B34FB";
  static const char* HeadsetAG     = "00001112-0000-1000-8000-00805F9B34FB";
  static const char* Handsfree     = "0000111E-0000-1000-8000-00805F9B34FB";
  static const char* HandsfreeAG   = "0000111F-0000-1000-8000-00805F9B34FB";
  static const char* ObjectPush    = "00001105-0000-1000-8000-00805F9B34FB";
}

// TODO/qdot: Move these back into gonk and make the service handler deal with
// it there.
//
// Gotten from reading the "u8" values in B2G/external/bluez/src/adapter.c
// These were hardcoded into android
enum BluetoothReservedChannels {
  DIALUP_NETWORK = 1,
  HANDSFREE_AG = 10,
  HEADSET_AG = 11,
  OPUSH = 12,
  SIM_ACCESS = 15,
  PBAP_PSE = 19,
  FTP = 20,
};

}
}
}

#endif

