/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Constructor(DOMString type,
             optional BluetoothPhonebookPullingEventInit eventInitDict)]
interface BluetoothPhonebookPullingEvent : Event
{
  readonly attribute DOMString                  name;
  readonly attribute vCardVersion               format;
  [Cached, Constant]
  readonly attribute sequence<vCardProperties>  propSelector;
  readonly attribute unsigned long              maxListCount;
  readonly attribute unsigned long              listStartOffset;
  [Cached, Constant]
  readonly attribute sequence<vCardProperties>  vcardSelector;
  readonly attribute vCardSelectorOp            vcardSelectorOperator;

  readonly attribute BluetoothPbapRequestHandle? handle;
};

dictionary BluetoothPhonebookPullingEventInit : EventInit
{
  DOMString                 name = "";
  vCardVersion              format = "vCard21";
  sequence<vCardProperties> propSelector = [];
  unsigned long             maxListCount = 0;
  unsigned long             listStartOffset = 0;
  sequence<vCardProperties> vcardSelector = [];
  vCardSelectorOp           vcardSelectorOperator = "OR";

  BluetoothPbapRequestHandle? handle = null;
};
