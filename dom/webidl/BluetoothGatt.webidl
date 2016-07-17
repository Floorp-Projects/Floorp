/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * [B2G only GATT client API]
 * BluetoothGatt interface is exposed only if
 * "dom.bluetooth.webbluetooth.enabled" preference is false.
 */
[ChromeOnly,
 Func="mozilla::dom::bluetooth::BluetoothManager::B2GGattClientEnabled"]
interface BluetoothGatt : EventTarget
{
  [Cached, Pure]
  readonly attribute sequence<BluetoothGattService> services;
  readonly attribute BluetoothConnectionState       connectionState;

  // Fired when the value of any characteristic changed
           attribute EventHandler                   oncharacteristicchanged;
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

  /**
   * Discover services, characteristics, descriptors offered by the remote GATT
   * server. The promise will be rejected if the connState is not connected or
   * operation fails.
   */
  [NewObject]
  Promise<void>                                     discoverServices();

  /**
   * Read RSSI for the remote BLE device if the connectState is connected.
   * Otherwise, the Promise will be rejected directly.
   */
  [NewObject]
  Promise<short>                                    readRemoteRssi();
};

enum BluetoothConnectionState
{
  "disconnected",
  "disconnecting",
  "connected",
  "connecting"
};
