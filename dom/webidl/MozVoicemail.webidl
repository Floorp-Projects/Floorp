/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsIDOMMozVoicemailStatus
interface MozVoicemailStatus;

[Pref="dom.voicemail.enabled"]
interface MozVoicemail : EventTarget
{
  /**
   * The current voicemail status, or null when the status is unknown
   */
  [GetterThrows]
  readonly attribute MozVoicemailStatus? status;

  /**
   * The voicemail box dialing number, or null if one wasn't found
   */
  [GetterThrows]
  readonly attribute DOMString? number;

  /**
   * The display name of the voicemail box dialing number, or null if one
   * wasn't found
   */
  [GetterThrows]
  readonly attribute DOMString? displayName;

  /**
   * The current voicemail status has changed
   */
  attribute EventHandler onstatuschanged;
};
