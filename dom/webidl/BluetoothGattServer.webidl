/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth"]
interface BluetoothGattServer : EventTarget
{
  // Fired when a remote device has been connected/disconnected
  attribute EventHandler  onconnectionstatechanged;

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
};
