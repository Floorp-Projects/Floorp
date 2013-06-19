/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional MozWifiConnectionInfoEventInit eventInitDict), HeaderFile="GeneratedEventClasses.h"]
interface MozWifiConnectionInfoEvent : Event
{
  [Throws]
  readonly attribute any network;
  readonly attribute short signalStrength;
  readonly attribute short relSignalStrength;
  readonly attribute long linkSpeed;
  readonly attribute DOMString? ipAddress;
};

dictionary MozWifiConnectionInfoEventInit : EventInit
{
  any network = null;
  short signalStrength = 0;
  short relSignalStrength = 0;
  long linkSpeed = 0;
  DOMString ipAddress = "";
};
