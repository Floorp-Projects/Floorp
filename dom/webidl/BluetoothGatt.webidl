/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[CheckPermissions="bluetooth"]
interface BluetoothGatt : EventTarget
{
  readonly attribute BluetoothConnectionState       connectionState;

  // Fired when attribute connectionState changed
           attribute EventHandler                   onconnectionstatechanged;

  /**
   * Connect/Disconnect to the remote BLE device if the connectionState is
   * disconnected/connected. Otherwise, the Promise will be rejected directly.
   *
   * If current connectionState is disconnected/connected,
   *   1) connectionState change to connecting/disconnecting along with a
   *      connectionstatechanged event.
   *   2) connectionState change to connected/disconnected if the operation
   *      succeeds. Otherwise, change to disconnected/connected.
   *   3) Promise is resolved or rejected according to the operation result.
   */
  [NewObject]
  Promise<void>                                     connect();
  [NewObject]
  Promise<void>                                     disconnect();
};

enum BluetoothConnectionState
{
  "disconnected",
  "disconnecting",
  "connected",
  "connecting"
};
