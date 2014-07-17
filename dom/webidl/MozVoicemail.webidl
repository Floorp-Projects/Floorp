/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.voicemail.enabled"]
interface MozVoicemail : EventTarget
{
  /**
   * The current voicemail status of a specified service, or null when the
   * status is unknown.
   */
  [Throws]
  MozVoicemailStatus getStatus(optional unsigned long serviceId);

  /**
   * The voicemail box dialing number of a specified service, or null if one
   * wasn't found.
   */
  [Throws]
  DOMString getNumber(optional unsigned long serviceId);

  /**
   * The display name of the voicemail box dialing number of a specified service,
   * or null if one wasn't found.
   */
  [Throws]
  DOMString getDisplayName(optional unsigned long serviceId);

  /**
   * The current voicemail status has changed.
   */
  attribute EventHandler onstatuschanged;
};
