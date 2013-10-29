/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// AudioChannels are used by:
// * HTMLMediaElement
// * AudioContext (WebAudio)
// When used, it has to throw an exception if the app tries to change the audio
// channel type without the permission (manifest file for B2G apps).
// The supported values are:
// * normal (default value)
//   Automatically paused if "notification" or higher priority channel
//   is played
//   Use case: normal applications
// * content
//   Automatically paused if "notification" or higher priority channel
//   is played. Also paused if another app starts using "content"
//   channel. Using this channel never affects applications using
//   the "normal" channel.
//   Use case: video/audio players
// * notification
//   Automatically paused if "alarm" or higher priority channel is played.
//   Use case: New email, incoming SMS
// * alarm
//   Automatically paused if "telephony" or higher priority channel is
//   played.
//   User case: Alarm clock, calendar alarms
// * telephony
//   Automatically paused if "ringer" or higher priority
//   channel is played.
//   Use case: dialer, voip
// * ringer
//   Automatically paused if "publicnotification" or higher priority
//   channel is played.
//   Use case: dialer, voip
// * publicnotification
//   Always plays in speaker, even when headphones are plugged in.
//   Use case: Camera shutter sound.
enum AudioChannel {
  "normal",
  "content",
  "notification",
  "alarm",
  "telephony",
  "ringer",
  "publicnotification",
};
