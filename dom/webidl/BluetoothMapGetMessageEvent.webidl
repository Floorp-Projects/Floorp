/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth",
 Constructor(DOMString type,
             optional BluetoothMapGetMessageEventInit eventInitDict)]
interface BluetoothMapGetMessageEvent : Event
{
  readonly attribute boolean          hasAttachment;
  readonly attribute FilterCharset    charset;

  readonly attribute BluetoothMapRequestHandle? handle;
};

dictionary BluetoothMapGetMessageEventInit : EventInit
{
  boolean                   hasAttachment = false;
  FilterCharset             charset = "utf-8";

  BluetoothMapRequestHandle? handle = null;
};
