/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
interface TelephonyCall;

[Constructor(DOMString type, optional CallEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface CallEvent : Event
{
  readonly attribute TelephonyCall? call;
};

dictionary CallEventInit : EventInit
{
  TelephonyCall? call = null;
};
