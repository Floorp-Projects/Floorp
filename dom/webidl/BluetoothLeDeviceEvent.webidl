/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * [B2G only GATT client API]
 * BluetoothLeDeviceEvent interface is exposed only if
 * "dom.bluetooth.webbluetooth.enabled" preference is false.
 */
[ChromeOnly,
 Func="mozilla::dom::bluetooth::BluetoothManager::B2GGattClientEnabled",
 Constructor(DOMString type, optional BluetoothLeDeviceEventInit eventInitDict)]
interface BluetoothLeDeviceEvent : Event
{
  readonly attribute BluetoothDevice? device;
  readonly attribute short rssi;
  [Throws]
  readonly attribute ArrayBuffer? scanRecord;
};

dictionary BluetoothLeDeviceEventInit : EventInit
{
  BluetoothDevice? device = null;
  short rssi = 0;
  ArrayBuffer? scanRecord = null;
};
