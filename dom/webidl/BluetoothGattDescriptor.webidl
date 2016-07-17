/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * BluetoothGattDescriptor could be in the server role as a descriptor provided
 * by a local GATT server, or in the client role as a descriptor provided by a
 * remote GATT server.
 */
[ChromeOnly]
interface BluetoothGattDescriptor
{
  readonly attribute BluetoothGattCharacteristic            characteristic;
  readonly attribute DOMString                              uuid;
  readonly attribute ArrayBuffer?                           value;
  [Cached, Constant]
  readonly attribute GattPermissions                        permissions;

  /**
   * Read or write the value of this descriptor.
   *
   * If this descriptor is in the client role, the value will be read from or
   * written to the remote GATT server. Otherwise, the local value will be
   * read/written.
   */
  [NewObject]
  Promise<ArrayBuffer>  readValue();
  [NewObject]
  Promise<void>         writeValue(ArrayBuffer value);
};
