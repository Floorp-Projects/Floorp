/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothUuid.h"

USING_BLUETOOTH_NAMESPACE

void
BluetoothUuidHelper::GetString(BluetoothServiceClass aServiceClassUuid,
                               nsAString& aRetUuidStr)
{
  aRetUuidStr.Truncate();

  aRetUuidStr.AppendLiteral("0000");
  aRetUuidStr.AppendInt(aServiceClassUuid, 16);
  aRetUuidStr.AppendLiteral("-0000-1000-8000-00805F9B34FB");
}

BluetoothServiceClass
BluetoothUuidHelper::GetBluetoothServiceClass(const nsAString& aUuidStr)
{
  // An example of input UUID string: 0000110D-0000-1000-8000-00805F9B34FB
  MOZ_ASSERT(aUuidStr.Length() == 36);

  /**
   * Extract uuid16 from input UUID string and return a value of enum
   * BluetoothServiceClass. If we failed to recognize the value,
   * BluetoothServiceClass::UNKNOWN is returned.
   */
  BluetoothServiceClass retValue = BluetoothServiceClass::UNKNOWN;
  nsString uuid(Substring(aUuidStr, 4, 4));

  nsresult rv;
  int32_t integer = uuid.ToInteger(&rv, 16);
  NS_ENSURE_SUCCESS(rv, retValue);

  switch (integer) {
    case BluetoothServiceClass::A2DP:
    case BluetoothServiceClass::HANDSFREE:
    case BluetoothServiceClass::HANDSFREE_AG:
    case BluetoothServiceClass::HEADSET:
    case BluetoothServiceClass::HEADSET_AG:
    case BluetoothServiceClass::HID:
    case BluetoothServiceClass::OBJECT_PUSH:
      retValue = (BluetoothServiceClass)integer;
  }
  return retValue;
}
