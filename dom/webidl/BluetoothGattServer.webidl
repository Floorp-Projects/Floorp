/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary BluetoothAdvertisingData
{
  /**
   * Uuid value of Appearance characteristic of the GAP service which can be
   * mapped to an icon or string that describes the physical representation of
   * the device during the device discovery procedure.
   */
  unsigned short appearance = 0;

  /**
   * Whether to broadcast with device name or not.
   */
  boolean includeDevName = false;

  /**
   * Whether to broadcast with TX power or not.
   */
  boolean includeTxPower = false;

  /**
   * Company Identifier Code for manufacturer data.
   *
   * This ID will be combined with |manufacturerData| byte array specified
   * below as the broadcasting manufacturer data. Please see Core Specification
   * Supplement (CSS) v6 1.4 for more details.
   */
  unsigned short manufacturerId = 0;

  /**
   * Byte array of custom manufacturer specific data.
   *
   * These bytes will be appended to |manufacturerId| specified above as the
   * broadcasting manufacturer data. Please see Core Specification Supplement
   * (CSS) v6 1.4 for more details.
   */
  ArrayBuffer? manufacturerData = null;

  /**
   * 128-bit Service UUID for service data.
   *
   * This UUID will be combinded with |serviceData| specified below as the
   * broadcasting service data. Please see Core Specification Supplement (CSS)
   * v6 1.11 for more details.
   */
  DOMString serviceUuid = "";

  /**
   * Data associated with |serviceUuid|.
   *
   * These bytes will be appended to |serviceUuid| specified above as the
   * broadcasting manufacturer data. Please see Core Specification Supplement
   * (CSS) v6 1.11 for more details.
   */
  ArrayBuffer? serviceData = null;

  /**
   * A list of Service or Service Class UUIDs.
   * Please see Core Specification Supplement (CSS) v6 1.1 for more details.
   */
  sequence<DOMString> serviceUuids = [];
};

[ChromeOnly]
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
   * Start or stop advertising data to nearby devices.
   *
   * Application may customize the advertising data by passing advData to
   * |startAdvertising|. By performing |startAdvertising|, remote central
   * devices can then discover and initiate a connection with our local device.
   * A 'connectionstatechanged' event will be fired when a remote central
   * device connects to the local device.
   */
  [NewObject]
  Promise<void> startAdvertising(optional BluetoothAdvertisingData advData);
  [NewObject]
  Promise<void> stopAdvertising();

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
