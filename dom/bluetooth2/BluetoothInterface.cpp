/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothInterface.h"
#ifdef MOZ_B2G_BT_BLUEDROID
#include "BluetoothHALInterface.h"
#endif

BEGIN_BLUETOOTH_NAMESPACE

//
// Socket Interface
//

BluetoothSocketInterface::~BluetoothSocketInterface()
{ }

//
// Handsfree Interface
//

// Notification handling
//

BluetoothHandsfreeNotificationHandler::
  ~BluetoothHandsfreeNotificationHandler()
{ }

// Interface
//

BluetoothHandsfreeInterface::BluetoothHandsfreeInterface()
{ }

BluetoothHandsfreeInterface::~BluetoothHandsfreeInterface()
{ }

//
// Bluetooth Advanced Audio Interface
//

// Notification handling
//

BluetoothA2dpNotificationHandler::~BluetoothA2dpNotificationHandler()
{ }

// Interface
//

BluetoothA2dpInterface::BluetoothA2dpInterface()
{ }

BluetoothA2dpInterface::~BluetoothA2dpInterface()
{ }

//
// Bluetooth AVRCP Interface
//

// Notification handling
//

BluetoothAvrcpNotificationHandler::~BluetoothAvrcpNotificationHandler()
{ }

// Interface
//

BluetoothAvrcpInterface::BluetoothAvrcpInterface()
{ }

BluetoothAvrcpInterface::~BluetoothAvrcpInterface()
{ }

// Notification handling
//

BluetoothNotificationHandler::~BluetoothNotificationHandler()
{ }

// Interface
//

BluetoothInterface*
BluetoothInterface::GetInstance()
{
  /* Here's where we decide which implementation to use. Currently
   * there is only Bluedroid, but others are possible. Having multiple
   * interfaces built-in and selecting the correct one at runtime could
   * also be an option.
   */
#ifdef MOZ_B2G_BT_BLUEDROID
  return BluetoothHALInterface::GetInstance();
#else
  return nullptr;
#endif
}

BluetoothInterface::BluetoothInterface()
{ }

BluetoothInterface::~BluetoothInterface()
{ }

END_BLUETOOTH_NAMESPACE
