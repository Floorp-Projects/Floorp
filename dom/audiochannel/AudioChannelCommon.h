/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelcommon_h__
#define mozilla_dom_audiochannelcommon_h__

namespace mozilla {
namespace dom {

enum AudioChannelState {
  AUDIO_CHANNEL_STATE_NORMAL = 0,
  AUDIO_CHANNEL_STATE_MUTED,
  AUDIO_CHANNEL_STATE_FADED,
  AUDIO_CHANNEL_STATE_LAST
};

} // namespace dom
} // namespace mozilla

#endif

