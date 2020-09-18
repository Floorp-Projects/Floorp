/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_
#define DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_

#include "AudioSegment.h"
#include "prtime.h"
#include "SineWaveGenerator.h"

namespace mozilla {

template <typename Sample>
class AudioGenerator {
 public:
  AudioGenerator(uint32_t aChannels, uint32_t aSampleRate,
                 uint32_t aFrequency = 1000)
      : mChannels(aChannels),
        mSampleRate(aSampleRate),
        mFrequency(aFrequency),
        mGenerator(aSampleRate, aFrequency) {}

  void Generate(mozilla::AudioSegment& aSegment, const uint32_t& aSamples) {
    SetInterleaved(false);
    CheckedInt<size_t> bufferSize(sizeof(Sample));
    bufferSize *= aSamples;
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize);
    Sample* dest = static_cast<Sample*>(buffer->Data());
    mGenerator.generate(dest, aSamples);
    AutoTArray<const Sample*, 1> channels;
    for (uint32_t i = 0; i < mChannels; ++i) {
      channels.AppendElement(dest);
    }
    aSegment.AppendFrames(buffer.forget(), channels, aSamples,
                          PRINCIPAL_HANDLE_NONE);
  }

  void GenerateInterleaved(Sample* aBuffer, const uint32_t& aFrames) {
    SetInterleaved(true);
    mGenerator.generate(aBuffer, aFrames * mChannels);
  }

  void SetInterleaved(bool aInterleaved) {
    if (aInterleaved == mInterleaved) {
      return;
    }
    mInterleaved = aInterleaved;
    if (mInterleaved) {
      TrackTicks offset = Offset();
      mGenerator =
          SineWaveGenerator<Sample>(mSampleRate, mFrequency, mChannels);
      mGenerator.SetOffset(offset * mChannels);
    } else {
      TrackTicks offset = Offset();
      mGenerator = SineWaveGenerator<Sample>(mSampleRate, mFrequency);
      mGenerator.SetOffset(offset / mChannels);
    }
  }

  void SetOffset(TrackTicks aFrames) { mGenerator.SetOffset(aFrames); }

  TrackTicks Offset() const { return mGenerator.Offset(); }

  static float Amplitude() { return SineWaveGenerator<Sample>::Amplitude(); }

  const uint32_t mChannels;
  const uint32_t mSampleRate;
  const uint32_t mFrequency;

 private:
  bool mInterleaved = false;
  mozilla::SineWaveGenerator<Sample> mGenerator;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_
