/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FLAC_DECODER_H_
#define FLAC_DECODER_H_

#include "ChannelMediaDecoder.h"

namespace mozilla {

class MediaContainerType;

class FlacDecoder : public ChannelMediaDecoder
{
public:
  // MediaDecoder interface.
  explicit FlacDecoder(MediaDecoderInit& aInit)
    : ChannelMediaDecoder(aInit)
  {
  }
  ChannelMediaDecoder* Clone(MediaDecoderInit& aInit) override;
  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if the Flac backend is pref'ed on, and we're running on a
  // platform that is likely to have decoders for the format.
  static bool IsEnabled();
  static bool IsSupportedType(const MediaContainerType& aContainerType);
};

} // namespace mozilla

#endif // !FLAC_DECODER_H_
