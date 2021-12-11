/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SINEWAVEGENERATOR_H_
#define SINEWAVEGENERATOR_H_

#include "MediaSegment.h"
#include "prtime.h"

namespace mozilla {

// generate 1k sine wave per second
template <typename Sample>
class SineWaveGenerator {
  static_assert(std::is_same<Sample, int16_t>::value ||
                std::is_same<Sample, float>::value);

 public:
  static const int bytesPerSample = sizeof(Sample);
  static const int millisecondsPerSecond = PR_MSEC_PER_SEC;
  static constexpr float twopi = 2 * M_PI;

  /* If more than 1 channel, generated samples are interleaved. */
  SineWaveGenerator(uint32_t aSampleRate, uint32_t aFrequency)
      : mPhase(0.), mPhaseIncrement(twopi * aFrequency / aSampleRate) {}

  // NOTE: only safely called from a single thread (MTG callback)
  void generate(Sample* aBuffer, TrackTicks aFrameCount,
                uint32_t aChannelCount = 1) {
    while (aFrameCount--) {
      Sample value = sin(mPhase) * Amplitude();
      for (uint32_t channel = 0; channel < aChannelCount; channel++) {
        *aBuffer++ = value;
      }
      mPhase += mPhaseIncrement;
      if (mPhase > twopi) {
        mPhase -= twopi;
      }
    }
  }

  static float Amplitude() {
    // Set volume to -20db.
    if (std::is_same<Sample, int16_t>::value) {
      return 3276.8;  // 32768.0 * 10^(-20/20) = 3276.8
    }
    return 0.1f;  // 1.0 * 10^(-20/20) = 0.1
  }

 private:
  double mPhase;
  const double mPhaseIncrement;
};

}  // namespace mozilla

#endif /* SINEWAVEGENERATOR_H_ */
