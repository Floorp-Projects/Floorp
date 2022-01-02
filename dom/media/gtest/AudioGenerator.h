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
      : mSampleRate(aSampleRate),
        mFrequency(aFrequency),
        mChannelCount(aChannels),
        mGenerator(aSampleRate, aFrequency) {}

  void Generate(mozilla::AudioSegment& aSegment, uint32_t aFrameCount) {
    CheckedInt<size_t> bufferSize(sizeof(Sample));
    bufferSize *= aFrameCount;
    RefPtr<SharedBuffer> buffer = SharedBuffer::Create(bufferSize);
    Sample* dest = static_cast<Sample*>(buffer->Data());
    mGenerator.generate(dest, aFrameCount);
    AutoTArray<const Sample*, 1> channels;
    for (uint32_t i = 0; i < mChannelCount; ++i) {
      channels.AppendElement(dest);
    }
    aSegment.AppendFrames(buffer.forget(), channels, aFrameCount,
                          PRINCIPAL_HANDLE_NONE);
  }

  void GenerateInterleaved(Sample* aSamples, uint32_t aFrameCount) {
    mGenerator.generate(aSamples, aFrameCount, mChannelCount);
  }

  void SetChannelsCount(uint32_t aChannelCount) {
    mChannelCount = aChannelCount;
  }

  uint32_t ChannelCount() const { return mChannelCount; }

  static float Amplitude() {
    return mozilla::SineWaveGenerator<Sample>::Amplitude();
  }

  const uint32_t mSampleRate;
  const uint32_t mFrequency;

 private:
  uint32_t mChannelCount;
  mozilla::SineWaveGenerator<Sample> mGenerator;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_GTEST_AUDIO_GENERATOR_H_
