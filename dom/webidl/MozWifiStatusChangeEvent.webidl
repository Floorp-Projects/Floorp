/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Constructor(DOMString type, optional MozWifiStatusChangeEventInit eventInitDict)]
interface MozWifiStatusChangeEvent : Event
{
  /**
   * Network object with a SSID field describing the network affected by
   * this change. This might be null.
   */
  readonly attribute any network;

  /**
   * String describing the current status of the wifi manager. See above for
   * the possible values.
   */
  readonly attribute DOMString? status;
};

dictionary MozWifiStatusChangeEventInit : EventInit
{
  any network = null;
  DOMString status = "";
};
