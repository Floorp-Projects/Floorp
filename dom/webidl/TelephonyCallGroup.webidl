/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.telephony.enabled"]
interface TelephonyCallGroup : EventTarget {
  readonly attribute CallsList calls;

  [NewObject, Throws]
  Promise<void> add(TelephonyCall call);

  [NewObject, Throws]
  Promise<void> add(TelephonyCall call, TelephonyCall secondCall);

  [NewObject, Throws]
  Promise<void> remove(TelephonyCall call);

  [NewObject]
  Promise<void> hangUp();

  [NewObject, Throws]
  Promise<void> hold();

  [NewObject, Throws]
  Promise<void> resume();

  readonly attribute TelephonyCallGroupState state;

  attribute EventHandler onstatechange;
  attribute EventHandler onconnected;
  attribute EventHandler onheld;
  attribute EventHandler oncallschanged;
  attribute EventHandler onerror;
};

enum TelephonyCallGroupState {
  "",
  "connected",
  "held",
};
