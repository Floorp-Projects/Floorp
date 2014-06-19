/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckPermissions="bluetooth",
 Constructor(DOMString type,
             optional BluetoothAttributeEventInit eventInitDict)]
interface BluetoothAttributeEvent : Event
{
  readonly attribute any attrs;
  // We don't support sequence in event codegen yet (Bug 1023762)
  // Bug 1015796:
  // readonly attribute sequence<DOMString> attrs;
};

dictionary BluetoothAttributeEventInit : EventInit
{
  any attrs = null;
};
