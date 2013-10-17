/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional USSDReceivedEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h", Pref="dom.icc.enabled"]
interface USSDReceivedEvent : Event
{
  readonly attribute DOMString? message;
  readonly attribute boolean sessionEnded;
};

dictionary USSDReceivedEventInit : EventInit
{
  DOMString? message = null;
  boolean sessionEnded = false;
};
