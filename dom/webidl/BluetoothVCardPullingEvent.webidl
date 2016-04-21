/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Constructor(DOMString type,
             optional BluetoothVCardPullingEventInit eventInitDict)]
interface BluetoothVCardPullingEvent : Event
{
  readonly attribute DOMString                 name;
  readonly attribute vCardVersion              format;
  [Cached, Constant]
  readonly attribute sequence<vCardProperties> propSelector;

  readonly attribute BluetoothPbapRequestHandle? handle;
};

dictionary BluetoothVCardPullingEventInit : EventInit
{
  DOMString                 name = "";
  vCardVersion              format = "vCard21";
  sequence<vCardProperties> propSelector = [];

  BluetoothPbapRequestHandle? handle = null;
};
