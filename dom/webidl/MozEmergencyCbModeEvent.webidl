/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.mobileconnection.enabled",
 Constructor(DOMString type, optional MozEmergencyCbModeEventInit eventInitDict)]
interface MozEmergencyCbModeEvent : Event
{
  /**
   * Whether the mode is activated.
   */
  readonly attribute boolean active;

  /**
   * Automatically exit the mode after the timeoutMs ms.
   */
  readonly attribute unsigned long timeoutMs;
};

dictionary MozEmergencyCbModeEventInit : EventInit
{
  boolean active = false;
  unsigned long timeoutMs = 0;
};
