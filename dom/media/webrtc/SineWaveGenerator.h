/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SINEWAVEGENERATOR_H_
#define SINEWAVEGENERATOR_H_

#include "MediaSegment.h"

namespace mozilla {

// generate 1k sine wave per second
template <typename Sample>
class SineWaveGenerator {
  static_assert(std::is_same<Sample, int16_t>::value ||
                std::is_same<Sample, float>::value);

 public:
  static const int bytesPerSample = sizeof(Sample);
  static const int millisecondsPerSecond = PR_MSEC_PER_SEC;

  /* If more than 1 channel, generated samples are interleaved. */
  SineWaveGenerator(uint32_t aSampleRate, uint32_t aFrequency,
                    uint32_t aChannels = 1)
      : mTotalLength(aSampleRate * aChannels / aFrequency), mReadLength(0) {
    MOZ_ASSERT(aChannels >= 1);
    // If we allow arbitrary frequencies, there's no guarantee we won't get
    // rounded here We could include an error term and adjust for it in
    // generation; not worth the trouble
    // MOZ_ASSERT(mTotalLength * aFrequency == aSampleRate);
    mAudioBuffer = MakeUnique<Sample[]>(mTotalLength);
    for (uint32_t i = 0; i < aSampleRate / aFrequency; ++i) {
      for (uint32_t j = 0; j < aChannels; ++j) {
        mAudioBuffer[i * aChannels + j] =
            Amplitude() * sin(2 * M_PI * i * aChannels / mTotalLength);
      }
    }
  }

  // NOTE: only safely called from a single thread (MTG callback)
  void generate(Sample* aBuffer, TrackTicks aLengthInSamples) {
    TrackTicks remaining = aLengthInSamples;

    while (remaining) {
      TrackTicks processSamples = 0;

      if (mTotalLength - mReadLength >= remaining) {
        processSamples = remaining;
      } else {
        processSamples = mTotalLength - mReadLength;
      }
      memcpy(aBuffer, &mAudioBuffer[mReadLength],
             processSamples * bytesPerSample);
      aBuffer += processSamples;
      mReadLength += processSamples;
      remaining -= processSamples;
      if (mReadLength == mTotalLength) {
        mReadLength = 0;
      }
    }
  }

  void SetOffset(TrackTicks aFrames) { mReadLength = aFrames % mTotalLength; }

  TrackTicks Offset() const { return mReadLength; }

  static float Amplitude() {
    // Set volume to -20db.
    if (std::is_same<Sample, int16_t>::value) {
      return 3276.8;  // 32768.0 * 10^(-20/20) = 3276.8
    }
    return 0.1f;  // 1.0 * 10^(-20/20) = 0.1
  }

 private:
  UniquePtr<Sample[]> mAudioBuffer;
  TrackTicks mTotalLength;
  TrackTicks mReadLength;
};

}  // namespace mozilla

#endif /* SINEWAVEGENERATOR_H_ */
