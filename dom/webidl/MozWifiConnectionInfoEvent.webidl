/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional MozWifiConnectionInfoEventInit eventInitDict)]
interface MozWifiConnectionInfoEvent : Event
{
  /**
   * Network object with an SSID field.
   */
  readonly attribute any network;

  /**
   * Strength of the signal to network, in dBm between -55 and -100 dBm.
   */
  readonly attribute short signalStrength;

  /**
   * Relative signal strength between 0 and 100.
   */
  readonly attribute short relSignalStrength;

  /**
   * Link speed in Mb/s.
   */
  readonly attribute long linkSpeed;

  /**
   * IP address in the dotted quad format.
   */
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
