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

  /**
   * Make a phone call or send the mmi code depending on the number provided.
   *
   * TelephonyCall - for call setup
   * MMICall - for MMI code
   */
  [Throws]
  Promise<(TelephonyCall or MMICall)> dial(DOMString number, optional unsigned long serviceId);

  [Throws]
  Promise<TelephonyCall> dialEmergency(DOMString number, optional unsigned long serviceId);

/**
  * Send a series of DTMF tones.
  *
  * @param tones
  *    DTMF chars.
  * @param pauseDuraton (ms) [optional]
  *    Time to wait before sending tones. Default value is 3000 ms.
  * @param toneDuration (ms) [optional]
  *    Duration of each tone. Default value is 70 ms.
  * @param serviceId [optional]
  *    Default value is as user setting dom.telephony.defaultServiceId.
  */
  [Throws]
  Promise<void> sendTones(DOMString tones, optional unsigned long pauseDuration = 3000, optional unsigned long toneDuration = 70, optional unsigned long serviceId);

  [Throws]
  void startTone(DOMString tone, optional unsigned long serviceId);

  [Throws]
  void stopTone(optional unsigned long serviceId);

  // Calling this method, the app will be treated as owner of the telephony
  // calls from the AudioChannel policy.
  [Throws]
  void ownAudioChannel();

  [Throws]
  attribute boolean muted;

  [Throws]
  attribute boolean speakerEnabled;

  readonly attribute (TelephonyCall or TelephonyCallGroup)? active;

  // A call is contained either in Telephony or in TelephonyCallGroup.
  readonly attribute CallsList calls;
  readonly attribute TelephonyCallGroup conferenceGroup;

  // Async notification that object initialization is done.
  [Throws]
  readonly attribute Promise<void> ready;

  attribute EventHandler onincoming;
  attribute EventHandler oncallschanged;
  attribute EventHandler onremoteheld;
  attribute EventHandler onremoteresumed;
};
