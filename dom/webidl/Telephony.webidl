/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Func="Navigator::HasTelephonySupport"]
interface Telephony : EventTarget {
  [Throws]
  TelephonyCall dial(DOMString number);
  [Throws]
  TelephonyCall dialEmergency(DOMString number);

  [Throws]
  attribute boolean muted;
  [Throws]
  attribute boolean speakerEnabled;

  readonly attribute (TelephonyCall or TelephonyCallGroup)? active;

  // A call is contained either in Telephony or in TelephonyCallGroup.
  readonly attribute CallsList calls;
  readonly attribute TelephonyCallGroup conferenceGroup;

  [Throws]
  void startTone(DOMString tone);
  [Throws]
  void stopTone();

  [SetterThrows]
  attribute EventHandler onincoming;
  [SetterThrows]
  attribute EventHandler oncallschanged;
  [SetterThrows]
  attribute EventHandler onremoteheld;
  [SetterThrows]
  attribute EventHandler onremoteresumed;
};
