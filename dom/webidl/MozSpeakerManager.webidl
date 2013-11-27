/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * Allow application can control acoustic sound output through speaker.
 * Reference https://wiki.mozilla.org/WebAPI/SpeakerManager
 */
[Constructor()]
interface MozSpeakerManager : EventTarget {
  /* query the speaker status */
  readonly attribute boolean speakerforced;
  /* force device device's acoustic sound output through speaker */
  attribute boolean forcespeaker;
  /* this event will be fired when device's speaker forced status change */
  attribute EventHandler onspeakerforcedchange;
};
