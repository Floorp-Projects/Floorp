/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary GattCharacteristicProperties
{
  boolean broadcast = false;
  boolean read = false;
  boolean writeNoResponse = false;
  boolean write = false;
  boolean notify = false;
  boolean indicate = false;
  boolean signedWrite = false;
  boolean extendedProps = false;
};

dictionary GattPermissions
{
  boolean read = false;
  boolean readEncrypted = false;
  boolean readEncryptedMITM = false;
  boolean write = false;
  boolean writeEncrypted = false;
  boolean writeEncryptedMITM = false;
  boolean writeSigned = false;
  boolean writeSignedMITM = false;
};

/**
 * BluetoothGattCharacteristic could be in the server role as a characteristic
 * provided by a local GATT server, or in the client role as a characteristic
 * provided by a remote GATT server.
 */
[CheckAnyPermissions="bluetooth"]
interface BluetoothGattCharacteristic
{
  readonly attribute BluetoothGattService                   service;
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattDescriptor>      descriptors;

  readonly attribute DOMString                              uuid;
  readonly attribute unsigned short                         instanceId;
  readonly attribute ArrayBuffer?                           value;
  [Cached, Constant]
  readonly attribute GattPermissions                        permissions;
  [Cached, Constant]
  readonly attribute GattCharacteristicProperties           properties;

  /**
   * Read or write the value of this characteristic.
   *
   * If this charactersitic is in the client role, the value will be
   * read from or written to the remote GATT server. Otherwise, the local value
   * will be read/written.
   */
  [NewObject]
  Promise<ArrayBuffer>  readValue();
  [NewObject]
  Promise<void>         writeValue(ArrayBuffer value);

  /**
   * Start or stop subscribing notifications of this characteristic from the
   * remote GATT server.
   *
   * This API will be rejected if this characteristic is in the server role.
   */
  [NewObject]
  Promise<void> startNotifications();
  [NewObject]
  Promise<void> stopNotifications();

  /**
   * Add a BLE descriptor to the local GATT characteristic.
   *
   * The promise will be rejected if this characteristic is in the client role
   * since a GATT client is not allowed to manipulate the descriptor list in a
   * remote GATT server.
   */
  [NewObject]
  Promise<BluetoothGattDescriptor> addDescriptor(DOMString uuid,
                                                 GattPermissions permissions,
                                                 ArrayBuffer value);
};
