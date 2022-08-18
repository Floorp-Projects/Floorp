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
#include "AudioBlock.h"
#include "MediaData.h"

namespace mozilla {

class AudibilityMonitor {
 public:
  // ≈ 20 * log10(pow(2, 12)), noise floor of 12bit audio
  const float AUDIBILITY_THRESHOLD =
      dom::WebAudioUtils::ConvertDecibelsToLinear(-72.);

  AudibilityMonitor(uint32_t aSamplerate, float aSilenceDurationSeconds)
      : mSamplerate(aSamplerate),
        mSilenceDurationSeconds(aSilenceDurationSeconds),
        mSilentFramesInARow(0),
        mEverAudible(false) {}

  void Process(const AudioData* aData) {
    ProcessInterleaved(aData->Data(), aData->mChannels);
  }

  void Process(const AudioBlock& aData) {
    if (aData.IsNull() || aData.IsMuted()) {
      mSilentFramesInARow += aData.GetDuration();
      return;
    }
    ProcessPlanar(aData.ChannelData<float>(), aData.GetDuration());
  }

  void ProcessPlanar(const nsTArray<const float*>& aPlanar, TrackTime aFrames) {
    uint32_t lastFrameAudibleAcrossChannels = 0;
    for (uint32_t channel = 0; channel < aPlanar.Length(); channel++) {
      uint32_t lastSampleAudible = 0;
      for (uint32_t frame = 0; frame < aFrames; frame++) {
        if (std::fabs(aPlanar[channel][frame]) > AUDIBILITY_THRESHOLD) {
          mEverAudible = true;
          mSilentFramesInARow = 0;
          lastSampleAudible = frame;
        }
      }
      lastFrameAudibleAcrossChannels =
          std::max(lastFrameAudibleAcrossChannels, lastSampleAudible);
    }
    mSilentFramesInARow += aFrames - lastFrameAudibleAcrossChannels - 1;
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
        if (std::fabs(AudioSampleToFloat(samples[readIndex++])) >
            AUDIBILITY_THRESHOLD) {
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
