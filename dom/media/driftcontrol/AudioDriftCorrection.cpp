/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDriftCorrection.h"

#include <cmath>

#include "AudioResampler.h"
#include "DriftController.h"
#include "mozilla/StaticPrefs_media.h"

namespace mozilla {

extern LazyLogModule gMediaTrackGraphLog;

#define LOG_CONTROLLER(level, controller, format, ...)             \
  MOZ_LOG(gMediaTrackGraphLog, level,                              \
          ("DriftController %p: (plot-id %u) " format, controller, \
           (controller)->mPlotId, ##__VA_ARGS__))

static uint32_t DesiredBuffering(uint32_t aSourceLatencyFrames,
                                 uint32_t aSourceRate) {
  constexpr uint32_t kMinBufferMs = 10;
  constexpr uint32_t kMaxBufferMs = 2500;
  const uint32_t minBufferingFrames = kMinBufferMs * aSourceRate / 1000;
  const uint32_t maxBufferingFrames = kMaxBufferMs * aSourceRate / 1000;
  return std::clamp(
      std::max(aSourceLatencyFrames,
               StaticPrefs::media_clockdrift_buffering() * aSourceRate / 1000),
      minBufferingFrames, maxBufferingFrames);
}

AudioDriftCorrection::AudioDriftCorrection(
    uint32_t aSourceRate, uint32_t aTargetRate,
    const PrincipalHandle& aPrincipalHandle)
    : mTargetRate(aTargetRate),
      mDriftController(MakeUnique<DriftController>(aSourceRate, aTargetRate,
                                                   mDesiredBufferingFrames)),
      mResampler(MakeUnique<AudioResampler>(aSourceRate, aTargetRate,
                                            mDesiredBufferingFrames,
                                            aPrincipalHandle)) {}

AudioDriftCorrection::~AudioDriftCorrection() = default;

AudioSegment AudioDriftCorrection::RequestFrames(const AudioSegment& aInput,
                                                 uint32_t aOutputFrames) {
  TrackTime inputFrames = aInput.GetDuration();

  if (inputFrames > 0) {
    if (mDesiredBufferingFrames == 0 || inputFrames > mDesiredBufferingFrames) {
      // Input latency is higher than the desired buffering. Increase the
      // desired buffering to try to avoid underruns.
      if (inputFrames > mSourceLatencyFrames) {
        const uint32_t desiredBuffering = DesiredBuffering(
            inputFrames * 11 / 10, mDriftController->mSourceRate);
        LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                       "High observed input latency (%" PRId64
                       "). Increasing desired buffering %u->%u frames (%.2fms)",
                       inputFrames, mDesiredBufferingFrames, desiredBuffering,
                       static_cast<float>(desiredBuffering) * 1000 /
                           mDriftController->mSourceRate);
        SetDesiredBuffering(desiredBuffering);
      } else {
        const uint32_t desiredBuffering = DesiredBuffering(
            mSourceLatencyFrames * 11 / 10, mDriftController->mSourceRate);
        LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                       "Increasing desired buffering %u->%u frames (%.2fms), "
                       "based on reported input-latency %u frames (%.2fms).",
                       mDesiredBufferingFrames, desiredBuffering,
                       static_cast<float>(desiredBuffering) * 1000 /
                           mDriftController->mSourceRate,
                       mSourceLatencyFrames,
                       static_cast<float>(mSourceLatencyFrames) * 1000 /
                           mDriftController->mSourceRate);
        SetDesiredBuffering(desiredBuffering);
      }
    }

    // Very important to go first since DynamicResampler will get the sample
    // format from the chunk.
    mResampler->AppendInput(aInput);
  }
  bool hasUnderrun = false;
  AudioSegment output = mResampler->Resample(aOutputFrames, &hasUnderrun);
  mDriftController->UpdateClock(inputFrames, aOutputFrames, CurrentBuffering(),
                                BufferSize());
  // Update resampler's rate if there is a new correction.
  mResampler->UpdateOutRate(mDriftController->GetCorrectedTargetRate());
  if (hasUnderrun) {
    NS_WARNING("Drift-correction: Underrun");
  }
  return output;
}

uint32_t AudioDriftCorrection::CurrentBuffering() const {
  return mResampler->InputReadableFrames();
}

uint32_t AudioDriftCorrection::BufferSize() const {
  return mResampler->InputCapacityFrames();
}

uint32_t AudioDriftCorrection::NumCorrectionChanges() const {
  return mDriftController->NumCorrectionChanges();
}

void AudioDriftCorrection::SetSourceLatencyFrames(
    uint32_t aSourceLatencyFrames) {
  LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                 "SetSourceLatencyFrames %u->%u", mSourceLatencyFrames,
                 aSourceLatencyFrames);

  mSourceLatencyFrames = aSourceLatencyFrames;
}

void AudioDriftCorrection::SetDesiredBuffering(
    uint32_t aDesiredBufferingFrames) {
  mDesiredBufferingFrames = aDesiredBufferingFrames;
  mDriftController->SetDesiredBuffering(mDesiredBufferingFrames);
  mResampler->SetPreBufferFrames(mDesiredBufferingFrames);
}
}  // namespace mozilla

#undef LOG_CONTROLLER
