/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * BluetoothGattService could be in the server role as a service provided by a
 * local GATT server, or in the client role as a service provided by a remote
 * GATT server.
 */
[ChromeOnly, Constructor(BluetoothGattServiceInit init)]
interface BluetoothGattService
{
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattCharacteristic>  characteristics;
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattService>         includedServices;

  readonly attribute boolean                                isPrimary;
  readonly attribute DOMString                              uuid;
  readonly attribute unsigned short                         instanceId;

  /**
   * Add a BLE characteristic to the local GATT service.
   *
   * This API will be rejected if this service is in the client role since a
   * GATT client is not allowed to manipulate the characteristic list in a
   * remote GATT server.
   */
  [NewObject]
  Promise<BluetoothGattCharacteristic> addCharacteristic(
    DOMString uuid,
    GattPermissions permissions,
    GattCharacteristicProperties properties,
    ArrayBuffer value);

  /**
   * Add a BLE included service to the local GATT service.
   *
   * This API will be rejected if this service is in the client role since a
   * GATT client is not allowed to manipulate the included service list in a
   * remote GATT server. The included service to be added should be an existing
   * service of the same GATT server. Otherwise this API will be rejected.
   */
  [NewObject]
  Promise<void> addIncludedService(BluetoothGattService service);
};

dictionary BluetoothGattServiceInit
{
  boolean isPrimary = false;
  DOMString uuid = "";
};
