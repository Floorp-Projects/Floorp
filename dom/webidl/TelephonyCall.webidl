/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.telephony.enabled"]
interface TelephonyCall : EventTarget {
  // Indicate which service the call comes from.
  readonly attribute unsigned long serviceId;

  readonly attribute TelephonyCallId id;

  // In CDMA networks, the 2nd waiting call shares the connection with the 1st
  // call. We need an additional attribute for the CDMA waiting call.
  readonly attribute TelephonyCallId? secondId;

  readonly attribute DOMString state;

  // The property "emergency" indicates whether the call number is an emergency
  // number. Only the outgoing call could have a value with true and it is
  // available after dialing state.
  readonly attribute boolean emergency;

  // Indicate whether the call state can be switched between "connected" and
  // "held".
  readonly attribute boolean switchable;

  // Indicate whether the call can be added into TelephonyCallGroup.
  readonly attribute boolean mergeable;

  readonly attribute DOMError? error;

  readonly attribute TelephonyCallGroup? group;

  [NewObject]
  Promise<void> answer();
  [NewObject]
  Promise<void> hangUp();
  [NewObject]
  Promise<void> hold();
  [NewObject]
  Promise<void> resume();

  attribute EventHandler onstatechange;
  attribute EventHandler ondialing;
  attribute EventHandler onalerting;
  attribute EventHandler onconnected;
  attribute EventHandler ondisconnected;
  attribute EventHandler onheld;
  attribute EventHandler onerror;

  // Fired whenever the group attribute changes.
  attribute EventHandler ongroupchange;
};
