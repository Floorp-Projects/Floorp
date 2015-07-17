/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth",
 Constructor(DOMString type,
             optional BluetoothAttributeEventInit eventInitDict)]
interface BluetoothAttributeEvent : Event
{
  [Cached, Constant]
  readonly attribute sequence<DOMString> attrs;
};

dictionary BluetoothAttributeEventInit : EventInit
{
  sequence<DOMString> attrs = [];
};
