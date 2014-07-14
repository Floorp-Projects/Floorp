/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Set of attributes that might be changed and reported by attributechanged
 * event.
 * Address is not included since it should not be changed once BluetoothDevice
 * is created.
 */
enum BluetoothDeviceAttribute
{
  "unknown",
  "cod",
  "name",
  "paired",
  "uuids"
};

[CheckPermissions="bluetooth"]
interface BluetoothDevice : EventTarget
{
  readonly attribute DOMString              address;
  readonly attribute BluetoothClassOfDevice cod;
  readonly attribute DOMString              name;
  readonly attribute boolean                paired;

  [Cached, Pure]
  readonly attribute sequence<DOMString>    uuids;

           attribute EventHandler           onattributechanged;

  /**
   * Fetch the up-to-date UUID list of each bluetooth service that the device
   * provides and refresh the cache value of attribute uuids if it is updated.
   *
   * If the operation succeeds, the promise will be resolved with up-to-date
   * UUID list which is identical to attribute uuids.
   */
  // Promise<sequence<DOMString>>
  [NewObject, Throws]
  Promise                                   fetchUuids();
};
