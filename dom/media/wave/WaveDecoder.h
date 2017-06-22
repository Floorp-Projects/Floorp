/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WaveDecoder_h_)
#define WaveDecoder_h_

#include "ChannelMediaDecoder.h"

namespace mozilla {

class MediaContainerType;

class WaveDecoder : public ChannelMediaDecoder
{
public:
  // MediaDecoder interface.
  explicit WaveDecoder(MediaDecoderInit& aInit)
    : ChannelMediaDecoder(aInit)
  {
  }
  ChannelMediaDecoder* Clone(MediaDecoderInit& aInit) override;
  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if the Wave backend is pref'ed on, and we're running on a
  // platform that is likely to have decoders for the format.
  static bool IsSupportedType(const MediaContainerType& aContainerType);
};

} // namespace mozilla

#endif
