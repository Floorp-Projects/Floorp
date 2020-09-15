/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_AUDIBILITYMONITOR_H_
#define DOM_MEDIA_AUDIBILITYMONITOR_H_

#include <cstdint>

#include "AudioSampleFormat.h"
#include "WebAudioUtils.h"

namespace mozilla {

class AudibilityMonitor {
 public:
  // ≈ 20 * log10(pow(2, 12)), noise floor of 12bit audio
  const float AUDIBILITY_THREHSOLD = -72.;

  AudibilityMonitor(uint32_t aSamplerate, float aSilenceDurationSeconds)
      : mSamplerate(aSamplerate),
        mSilenceDurationSeconds(aSilenceDurationSeconds),
        mSilentFramesInARow(0),
        mEverAudible(false) {}

  void ProcessAudioData(const AudioData* aData) {
    ProcessInterleaved(aData->Data(), aData->mChannels);
  }

  void ProcessInterleaved(const Span<AudioDataValue>& aInterleaved,
                          size_t aChannels) {
    MOZ_ASSERT(aInterleaved.Length() % aChannels == 0);
    uint32_t frameCount = aInterleaved.Length() / aChannels;
    AudioDataValue* samples = aInterleaved.Elements();

    uint32_t readIndex = 0;
    for (uint32_t i = 0; i < frameCount; i++) {
      bool atLeastOneAudible = false;
      for (uint32_t j = 0; j < aChannels; j++) {
        float dbfs = dom::WebAudioUtils::ConvertLinearToDecibels(
            abs(AudioSampleToFloat(samples[readIndex++])), -100.f);
        if (dbfs > AUDIBILITY_THREHSOLD) {
          atLeastOneAudible = true;
        }
      }
      if (atLeastOneAudible) {
        mSilentFramesInARow = 0;
        mEverAudible = true;
      } else {
        mSilentFramesInARow++;
      }
    }
  }

  // A stream is considered audible if there was audible content in the last
  // `mSilenceDurationSeconds` seconds, or it has never been audible for now.
  bool RecentlyAudible() {
    return mEverAudible && (static_cast<float>(mSilentFramesInARow) /
                            mSamplerate) < mSilenceDurationSeconds;
  }

 private:
  const uint32_t mSamplerate;
  const float mSilenceDurationSeconds;
  uint64_t mSilentFramesInARow;
  bool mEverAudible;
};

};  // namespace mozilla

#endif  // DOM_MEDIA_AUDIBILITYMONITOR_H_
