/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.mobileconnection.enabled",
 Constructor(DOMString type, optional CFStateChangeEventInit eventInitDict)]
interface CFStateChangeEvent : Event
{
  /**
   * Indicates what to do with the rule.
   *
   * One of the CALL_FORWARD_ACTION_* constants. It will be either disable (0),
   * enable (1), query status (2), registration (3), or erasure (4).
   *
   * @see 3GPP MozMobileConnection.CALL_FORWARD_ACTION_* values.
   * @see 3GPP TS 27.007 7.11 "mode".
   */
  readonly attribute unsigned short action;

  /**
   * Indicates the reason the call is being forwarded.
   *
   * One of the CALL_FORWARD_REASON_* constants. It will be either
   * unconditional (0), mobile busy (1), no reply (2), not reachable (3),
   * all call forwarding (4), or all conditional call forwarding (5).
   *
   * @see 3GPP MozMobileConnection.CALL_FORWARD_REASON_* values.
   * @see 3GPP TS 27.007 7.11 "reason".
   */
  readonly attribute unsigned short reason;

  /**
   * Phone number of forwarding address.
   */
  readonly attribute DOMString? number;

  /**
   * When "no reply" is enabled or queried, this gives the time in
   * seconds to wait before call is forwarded.
   */
  readonly attribute unsigned short timeSeconds;

  /**
   * Service for which the call forward is set up. It should be one of the
   * MozMobileConnection.ICC_SERVICE_CLASS_* values.
   */
  readonly attribute unsigned short serviceClass;
};

dictionary CFStateChangeEventInit : EventInit
{
  unsigned short action = 0;
  unsigned short reason = 0;
  DOMString number = "";
  unsigned short timeSeconds = 0;
  unsigned short serviceClass = 0;
};
