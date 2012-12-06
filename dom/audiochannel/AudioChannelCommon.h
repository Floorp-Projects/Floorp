/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelcommon_h__
#define mozilla_dom_audiochannelcommon_h__

namespace mozilla {
namespace dom {

// The audio channel. Read the nsIHTMLMediaElement.idl for a description
// about this attribute.
enum AudioChannelType {
  AUDIO_CHANNEL_NORMAL = 0,
  AUDIO_CHANNEL_CONTENT,
  AUDIO_CHANNEL_NOTIFICATION,
  AUDIO_CHANNEL_ALARM,
  AUDIO_CHANNEL_TELEPHONY,
  AUDIO_CHANNEL_RINGER,
  AUDIO_CHANNEL_PUBLICNOTIFICATION,
  AUDIO_CHANNEL_LAST
};

} // namespace dom
} // namespace mozilla

#endif

