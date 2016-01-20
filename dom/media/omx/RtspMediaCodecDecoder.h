/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(RtspMediaCodecDecoder_h_)
#define RtspMediaCodecDecoder_h_

#include "MediaOmxCommonDecoder.h"

namespace mozilla {

class RtspMediaCodecDecoder final : public MediaOmxCommonDecoder
{
public:
  explicit RtspMediaCodecDecoder(MediaDecoderOwner* aOwner)
    : MediaOmxCommonDecoder(aOwner) {}

  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override;

  MediaOmxCommonReader* CreateReader() override;

  MediaDecoderStateMachine* CreateStateMachineFromReader(MediaOmxCommonReader* aReader) override;

  void ChangeState(PlayState aState) override;
};

} // namespace mozilla

#endif
