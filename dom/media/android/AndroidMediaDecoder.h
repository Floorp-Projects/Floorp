/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(AndroidMediaDecoder_h_)
#define AndroidMediaDecoder_h_

#include "ChannelMediaDecoder.h"
#include "AndroidMediaDecoder.h"
#include "MediaContainerType.h"

namespace mozilla {

class AndroidMediaDecoder : public ChannelMediaDecoder
{
  MediaContainerType mType;
public:
  AndroidMediaDecoder(MediaDecoderInit& aInit, const MediaContainerType& aType);

  MediaDecoder* Clone(MediaDecoderInit& aInit) override {
    return new AndroidMediaDecoder(aInit, mType);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;
};

} // namespace mozilla

#endif
