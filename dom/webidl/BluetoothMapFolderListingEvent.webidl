/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Constructor(DOMString type,
             optional BluetoothMapFolderListingEventInit eventInitDict)]
interface BluetoothMapFolderListingEvent : Event
{
  readonly attribute unsigned long              maxListCount;
  readonly attribute unsigned long              listStartOffset;

  readonly attribute BluetoothMapRequestHandle? handle;
};

dictionary BluetoothMapFolderListingEventInit : EventInit
{
  unsigned long             maxListCount = 0;
  unsigned long             listStartOffset = 0;

  BluetoothMapRequestHandle? handle = null;
};
