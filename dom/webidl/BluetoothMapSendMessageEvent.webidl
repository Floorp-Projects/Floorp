/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth",
 Constructor(DOMString type,
             optional BluetoothMapSendMessageEventInit eventInitDict)]
interface BluetoothMapSendMessageEvent : Event
{
  readonly attribute DOMString        recipient;
  readonly attribute DOMString        messageBody;
  readonly attribute unsigned long    retry;

  readonly attribute BluetoothMapRequestHandle? handle;
};

dictionary BluetoothMapSendMessageEventInit : EventInit
{
  DOMString                 recipient = "";
  DOMString                 messageBody = "";
  unsigned long             retry = 0;

  BluetoothMapRequestHandle? handle = null;
};
