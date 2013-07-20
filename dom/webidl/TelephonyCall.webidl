/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface TelephonyCall : EventTarget {
  readonly attribute DOMString number;

  // In CDMA networks, the 2nd waiting call shares the connection with the 1st
  // call. We need an additional attribute for the 2nd number.
  readonly attribute DOMString? secondNumber;

  readonly attribute DOMString state;

  // The property "emergency" indicates whether the call number is an emergency
  // number. Only the outgoing call could have a value with true and it is
  // available after dialing state.
  readonly attribute boolean emergency;

  readonly attribute DOMError? error;

  readonly attribute TelephonyCallGroup? group;

  [Throws]
  void answer();
  [Throws]
  void hangUp();
  [Throws]
  void hold();
  [Throws]
  void resume();

  [SetterThrows]
  attribute EventHandler onstatechange;
  [SetterThrows]
  attribute EventHandler ondialing;
  [SetterThrows]
  attribute EventHandler onalerting;
  [SetterThrows]
  attribute EventHandler onconnecting;
  [SetterThrows]
  attribute EventHandler onconnected;
  [SetterThrows]
  attribute EventHandler ondisconnecting;
  [SetterThrows]
  attribute EventHandler ondisconnected;
  [SetterThrows]
  attribute EventHandler onholding;
  [SetterThrows]
  attribute EventHandler onheld;
  [SetterThrows]
  attribute EventHandler onresuming;
  [SetterThrows]
  attribute EventHandler onerror;

  // Fired whenever the group attribute changes.
  [SetterThrows]
  attribute EventHandler ongroupchange;
};
