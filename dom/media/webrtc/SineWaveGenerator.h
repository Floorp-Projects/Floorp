/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SINEWAVEGENERATOR_H_
#define SINEWAVEGENERATOR_H_

namespace mozilla {

// generate 1k sine wave per second
class SineWaveGenerator
{
public:
  static const int bytesPerSample = 2;
  static const int millisecondsPerSecond = PR_MSEC_PER_SEC;

  explicit SineWaveGenerator(uint32_t aSampleRate, uint32_t aFrequency) :
    mTotalLength(aSampleRate / aFrequency),
    mReadLength(0) {
    // If we allow arbitrary frequencies, there's no guarantee we won't get rounded here
    // We could include an error term and adjust for it in generation; not worth the trouble
    //MOZ_ASSERT(mTotalLength * aFrequency == aSampleRate);
    mAudioBuffer = MakeUnique<int16_t[]>(mTotalLength);
    for (int i = 0; i < mTotalLength; i++) {
      // Set volume to -20db. It's from 32768.0 * 10^(-20/20) = 3276.8
      mAudioBuffer[i] = (3276.8f * sin(2 * M_PI * i / mTotalLength));
    }
  }

  // NOTE: only safely called from a single thread (MSG callback)
  void generate(int16_t* aBuffer, int16_t aLengthInSamples) {
    int16_t remaining = aLengthInSamples;

    while (remaining) {
      int16_t processSamples = 0;

      if (mTotalLength - mReadLength >= remaining) {
        processSamples = remaining;
      } else {
        processSamples = mTotalLength - mReadLength;
      }
      memcpy(aBuffer, &mAudioBuffer[mReadLength], processSamples * bytesPerSample);
      aBuffer += processSamples;
      mReadLength += processSamples;
      remaining -= processSamples;
      if (mReadLength == mTotalLength) {
        mReadLength = 0;
      }
    }
  }

private:
  UniquePtr<int16_t[]> mAudioBuffer;
  int16_t mTotalLength;
  int16_t mReadLength;
};

} // namespace mozilla

#endif /* SINEWAVEGENERATOR_H_ */
