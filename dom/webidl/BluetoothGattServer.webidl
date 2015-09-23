/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth"]
interface BluetoothGattServer : EventTarget
{
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattService> services;

  // Fired when a remote device has been connected/disconnected
  attribute EventHandler  onconnectionstatechanged;

  // Fired when a remote BLE client send a read/write request
  attribute EventHandler  onattributereadreq;
  attribute EventHandler  onattributewritereq;

  /**
   * Connect/Disconnect to the remote BLE device with the target address.
   *
   * Promise will be rejected if the local GATT server is busy connecting or
   * disconnecting to other devices.
   */
  [NewObject]
  Promise<void> connect(DOMString address);
  [NewObject]
  Promise<void> disconnect(DOMString address);

  /**
   * Add a BLE service to the local GATT server.
   *
   * This API will be rejected if this service has been added to the GATT
   * server.
   */
  [NewObject]
  Promise<void> addService(BluetoothGattService service);

  /**
   * Remove a BLE service to the local GATT server.
   *
   * This API will be rejected if this service does not exist in the GATT
   * server.
   */
  [NewObject]
  Promise<void> removeService(BluetoothGattService service);

  /**
   * Notify the remote BLE device that the value of a characteristic has been
   * changed.
   */
  [NewObject]
  Promise<void> notifyCharacteristicChanged(
    DOMString address,
    BluetoothGattCharacteristic characteristic,
    boolean confirm);

  /**
   * Send a read/write response to a remote BLE client
   */
  [NewObject]
  Promise<void> sendResponse(
    DOMString address, unsigned short status, long requestId);
};
