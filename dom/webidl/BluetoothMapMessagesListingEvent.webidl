/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[CheckAnyPermissions="bluetooth",
 Constructor(DOMString type,
             optional BluetoothMapMessagesListingEventInit eventInitDict)]
interface BluetoothMapMessagesListingEvent : Event
{
  readonly attribute unsigned long              maxListCount;
  readonly attribute unsigned long              listStartOffset;
  readonly attribute unsigned long              subjectLength;
  [Cached, Constant]
  readonly attribute sequence<ParameterMask>    parameterMask;
  readonly attribute MessageType                filterMessageType;
  readonly attribute DOMString                  filterPeriodBegin;
  readonly attribute DOMString                  filterPeriodEnd;
  readonly attribute ReadStatus                 filterReadStatus;
  readonly attribute DOMString                  filterRecipient;
  readonly attribute DOMString                  filterOriginator;
  readonly attribute Priority                   filterPriority;

  readonly attribute BluetoothMapRequestHandle? handle;
};

dictionary BluetoothMapMessagesListingEventInit : EventInit
{
  unsigned long             maxListCount = 0;
  unsigned long             listStartOffset = 0;
  unsigned long             subjectLength = 0;
  sequence<ParameterMask>   parameterMask = [];
  MessageType               filterMessageType = "no-filtering";
  DOMString                 filterPeriodBegin = "";
  DOMString                 filterPeriodEnd = "";
  ReadStatus                filterReadStatus = "no-filtering";
  DOMString                 filterRecipient = "";
  DOMString                 filterOriginator = "";
  Priority                  filterPriority = "no-filtering";

  BluetoothMapRequestHandle? handle = null;
};
