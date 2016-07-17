/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type,
             optional BluetoothGattAttributeEventInit eventInitDict),
 ChromeOnly]
interface BluetoothGattAttributeEvent : Event
{
  readonly attribute DOMString                    address;
  readonly attribute long                         requestId;
  readonly attribute BluetoothGattCharacteristic? characteristic;
  readonly attribute BluetoothGattDescriptor?     descriptor;
  [Throws]
  readonly attribute ArrayBuffer?                 value;
  readonly attribute boolean                      needResponse;
};

dictionary BluetoothGattAttributeEventInit : EventInit
{
  DOMString address = "";
  long requestId = 0;
  BluetoothGattCharacteristic? characteristic = null;
  BluetoothGattDescriptor? descriptor = null;
  /**
   * Note that the passed-in value will be copied by the event constructor
   * here instead of storing the reference.
   */
  ArrayBuffer? value = null;
  boolean needResponse = true;
};
