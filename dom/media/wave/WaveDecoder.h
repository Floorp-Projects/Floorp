/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(WaveDecoder_h_)
#define WaveDecoder_h_

#include "MediaDecoder.h"

/**
 * The decoder implementation is currently limited to Linear PCM encoded
 * audio data with one or two channels of 8- or 16-bit samples at sample
 * rates from 100 Hz to 96 kHz.  The number of channels is limited by what
 * the audio backend (via AudioStream) currently supports.  The supported
 * sample rate is artificially limited to arbitrarily selected sane values.
 * Support for additional channels (and other new features) would
 * require extending WaveDecoder to support parsing the newer
 * WAVE_FORMAT_EXTENSIBLE chunk format.
**/

namespace mozilla {

class WaveDecoder : public MediaDecoder
{
public:
  explicit WaveDecoder(MediaDecoderOwner* aOwner) : MediaDecoder(aOwner) {}
  MediaDecoder* Clone(MediaDecoderOwner* aOwner) override {
    if (!IsWaveEnabled()) {
      return nullptr;
    }
    return new WaveDecoder(aOwner);
  }
  MediaDecoderStateMachine* CreateStateMachine() override;
};

} // namespace mozilla

#endif
