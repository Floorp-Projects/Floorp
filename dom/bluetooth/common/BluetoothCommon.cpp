/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothCommon.h"

// FIXME: Bug 1224171: This variable needs to be cleaned up and
//                     remove from global namespace.
bool gBluetoothDebugFlag = false;

BEGIN_BLUETOOTH_NAMESPACE

//
// |BluetoothAddress|
//

const BluetoothAddress& BluetoothAddress::ANY()
{
  static const BluetoothAddress sAddress(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
  return sAddress;
}

const BluetoothAddress& BluetoothAddress::ALL()
{
  static const BluetoothAddress sAddress(0xff, 0xff, 0xff, 0xff, 0xff, 0xff);
  return sAddress;
}

const BluetoothAddress& BluetoothAddress::LOCAL()
{
  static const BluetoothAddress sAddress(0x00, 0x00, 0x00, 0xff, 0xff, 0xff);
  return sAddress;
}

//
// |BluetoothUuid|
//

const BluetoothUuid& BluetoothUuid::ZERO()
{
  static const BluetoothUuid sUuid(0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x00, 0x00);
  return sUuid;
}

/*
 * [Bluetooth Specification Version 4.2, Volume 3, Part B, Section 2.5.1]
 *
 * To reduce the burden of storing and transferring 128-bit UUID values, a
 * range of UUID values has been pre-allocated for assignment to often-used,
 * registered purposes. The first UUID in this pre-allocated range is known as
 * the Bluetooth Base UUID and has the value 00000000-0000-1000-8000-
 * 00805F9B34FB, from the Bluetooth Assigned Numbers document.
 */
const BluetoothUuid& BluetoothUuid::BASE()
{
  static const BluetoothUuid sUuid(0x00, 0x00, 0x00, 0x00,
                                   0x00, 0x00, 0x10, 0x00,
                                   0x80, 0x00, 0x00, 0x80,
                                   0x5f, 0x9b, 0x34, 0xfb);
  return sUuid;
}

END_BLUETOOTH_NAMESPACE
