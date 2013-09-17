/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface AudioChannelManager : EventTarget {
  /**
   * Indicates whether the headphones are plugged in or not.
   */
  readonly attribute boolean headphones;

  /**
   * Fired when the headphones are plugged or unplugged.
   *
   * When the headphones are unplugged, we may start playing audio through the
   * system's speakers.  Similarly, when headphones are plugged in, we may
   * switch audio from speakers to headphones.
   *
   * If audio is currently playing in this window or in one of its children, we
   * will fire this event before we switch the audio output from headphones to
   * speakers (or vice versa).  This allows you to, for example, pause your
   * window's audio when the headphones are unplugged.
   */
  attribute EventHandler onheadphoneschange;

  /**
   * Indicates which audio channel is used to adjust volume when pressing HW
   * volume keys.
   */
  attribute DOMString volumeControlChannel;
};
