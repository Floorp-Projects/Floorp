/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[CheckPermissions="bluetooth"]
interface BluetoothGattCharacteristic
{
  readonly attribute BluetoothGattService                   service;
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattDescriptor>      descriptors;

  readonly attribute DOMString                              uuid;
  readonly attribute unsigned short                         instanceId;

  /**
   * Start or stop subscribing notifications of this characteristic from the
   * remote GATT server.
   */
  [NewObject]
  Promise<void> startNotifications();
  [NewObject]
  Promise<void> stopNotifications();
};
