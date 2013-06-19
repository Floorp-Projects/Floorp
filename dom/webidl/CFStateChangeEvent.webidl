/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional CFStateChangeEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface CFStateChangeEvent : Event
{
  readonly attribute boolean success;
  readonly attribute unsigned short action;
  readonly attribute unsigned short reason;
  readonly attribute DOMString? number;
  readonly attribute unsigned short timeSeconds;
  readonly attribute unsigned short serviceClass;
};

dictionary CFStateChangeEventInit : EventInit
{
  boolean success = false;
  unsigned short action = 0;
  unsigned short reason = 0;
  DOMString number = "";
  unsigned short timeSeconds = 0;
  unsigned short serviceClass = 0;
};
