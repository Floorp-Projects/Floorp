/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.telephony.enabled",
 ChromeOnly,
 Constructor(DOMString type, optional USSDReceivedEventInit eventInitDict)]
interface USSDReceivedEvent : Event
{
  readonly attribute unsigned long serviceId;
  readonly attribute DOMString? message;
  readonly attribute USSDSession? session;  // null if session is ended.
};

dictionary USSDReceivedEventInit : EventInit
{
  unsigned long serviceId = 0;
  DOMString? message = null;
  USSDSession? session = null;
};
