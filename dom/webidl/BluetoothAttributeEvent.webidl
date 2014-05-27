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
  readonly attribute unsigned short   attr;
  readonly attribute any              value;
  // We don't support sequences in unions yet (Bug 767924)
  /* readonly attribute (BluetoothAdapter or BluetoothAdapterState or
   *                     BluetoothClassOfDevice or boolean or
   *                     DOMString or sequence<DOMString>) value;
   */
};

dictionary BluetoothAttributeEventInit : EventInit
{
  unsigned short attr  = 0;
  any            value = null;
};
