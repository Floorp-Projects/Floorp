/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// |BluetoothAddress|
//

const BluetoothAddress BluetoothAddress::ANY(0x00, 0x00, 0x00,
                                             0x00, 0x00, 0x00);

const BluetoothAddress BluetoothAddress::ALL(0xff, 0xff, 0xff,
                                             0xff, 0xff, 0xff);

const BluetoothAddress BluetoothAddress::LOCAL(0x00, 0x00, 0x00,
                                               0xff, 0xff, 0xff);

END_BLUETOOTH_NAMESPACE
