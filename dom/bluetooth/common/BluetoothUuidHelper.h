/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_BluetoothUuidHelper_h
#define mozilla_dom_bluetooth_BluetoothUuidHelper_h

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothProfileManagerBase;

class BluetoothUuidHelper
{
public:

  /**
   * Convert a 128-bit uuid string to a value of BluetoothServiceClass
   *
   * @param aUuidStr  128-bit uuid string
   * @return  a value of BluetoothServiceClass
   */
  static BluetoothServiceClass
  GetBluetoothServiceClass(const nsAString& aUuidStr);

  static BluetoothServiceClass
  GetBluetoothServiceClass(uint16_t aServiceUuid);

  static BluetoothProfileManagerBase*
  GetBluetoothProfileManager(uint16_t aServiceUuid);
};

// TODO/qdot: Move these back into gonk and make the service handler deal with
// it there.
//
// Gotten from reading the "u8" values in B2G/external/bluez/src/adapter.c
// These were hardcoded into android
enum BluetoothReservedChannels {
  CHANNEL_DIALUP_NETWORK = 1,
  CHANNEL_HANDSFREE_AG   = 10,
  CHANNEL_HEADSET_AG     = 11,
  CHANNEL_OPUSH          = 12,
  CHANNEL_SIM_ACCESS     = 15,
  CHANNEL_PBAP_PSE       = 19,
  CHANNEL_FTP            = 20,
  CHANNEL_OPUSH_L2CAP    = 5255
};

END_BLUETOOTH_NAMESPACE

#endif // mozilla_dom_bluetooth_BluetoothUuidHelper_h
