/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[ChromeOnly]
interface BluetoothManager: EventTarget
{
  readonly attribute BluetoothAdapter? defaultAdapter;

  // Fired when attribute(s) of BluetoothManager changed
           attribute EventHandler onattributechanged;

  // Fired when a new adapter is plugged in
           attribute EventHandler onadapteradded;

  // Fired when an existing adapter is unplugged
           attribute EventHandler onadapterremoved;

  sequence<BluetoothAdapter> getAdapters();
};

enum BluetoothManagerAttribute
{
  "unknown",
  "defaultAdapter"
};
