/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(OggDecoder_h_)
#define OggDecoder_h_

#include "ChannelMediaDecoder.h"

namespace mozilla {

class MediaContainerType;

class OggDecoder : public ChannelMediaDecoder
{
public:
  explicit OggDecoder(MediaDecoderInit& aInit)
    : ChannelMediaDecoder(aInit)
  {}

  MediaDecoder* Clone(MediaDecoderInit& aInit) override {
    if (!IsOggEnabled()) {
      return nullptr;
    }
    return new OggDecoder(aInit);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;

  // Returns true if aContainerType is an Ogg type that we think we can render
  // with an enabled platform decoder backend.
  // If provided, codecs are checked for support.
  static bool IsSupportedType(const MediaContainerType& aContainerType);
};

} // namespace mozilla

#endif
