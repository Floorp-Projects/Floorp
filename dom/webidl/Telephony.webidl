/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.telephony.enabled"]
interface Telephony : EventTarget {
  /**
   * There are multiple telephony services in multi-sim architecture. We use
   * |serviceId| to indicate the target telephony service. If not specified,
   * the implementation MUST use the default service.
   *
   * Possible values of |serviceId| are 0 ~ (number of services - 1), which is
   * simply the index of a service. Get number of services by acquiring
   * |navigator.mozMobileConnections.length|.
   */

  [Throws]
  TelephonyCall dial(DOMString number, optional unsigned long serviceId);

  [Throws]
  TelephonyCall dialEmergency(DOMString number, optional unsigned long serviceId);

  [Throws]
  void startTone(DOMString tone, optional unsigned long serviceId);

  [Throws]
  void stopTone(optional unsigned long serviceId);

  [Throws]
  attribute boolean muted;

  [Throws]
  attribute boolean speakerEnabled;

  readonly attribute (TelephonyCall or TelephonyCallGroup)? active;

  // A call is contained either in Telephony or in TelephonyCallGroup.
  readonly attribute CallsList calls;
  readonly attribute TelephonyCallGroup conferenceGroup;

  attribute EventHandler onincoming;
  attribute EventHandler oncallschanged;
  attribute EventHandler onremoteheld;
  attribute EventHandler onremoteresumed;
};
