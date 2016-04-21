/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface BluetoothDevice : EventTarget
{
  readonly attribute DOMString              address;
  readonly attribute BluetoothClassOfDevice cod;
  readonly attribute DOMString              name;
  readonly attribute boolean                paired;
  readonly attribute BluetoothDeviceType    type;

  /**
   * Retrieve the BluetoothGatt interface to interact with remote BLE devices.
   * This attribute is null if the device type is not dual or le.
   *
   * [B2G only GATT client API]
   * gatt attribute is exposed only if "dom.bluetooth.webbluetooth.enabled"
   * preference is false.
   */
  [Func="mozilla::dom::bluetooth::BluetoothManager::B2GGattClientEnabled"]
  readonly attribute BluetoothGatt?         gatt;

  [Cached, Pure]
  readonly attribute sequence<DOMString>    uuids;

  // Fired when attribute(s) of BluetoothDevice changed
           attribute EventHandler           onattributechanged;

  /**
   * Fetch the up-to-date UUID list of each bluetooth service that the device
   * provides and refresh the cache value of attribute uuids if it is updated.
   *
   * If the operation succeeds, the promise will be resolved with up-to-date
   * UUID list which is identical to attribute uuids.
   */
  [NewObject]
  Promise<sequence<DOMString>>              fetchUuids();
};

enum BluetoothDeviceType
{
  "unknown",
  "classic",
  "le",
  "dual"
};

/*
 * Possible device attributes that attributechanged event reports.
 * Note "address" and "type" are excluded since they never change once
 * BluetoothDevice is created.
 */
enum BluetoothDeviceAttribute
{
  "unknown",
  "cod",
  "name",
  "paired",
  "uuids"
};

