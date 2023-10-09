/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioDriftCorrection.h"

#include <cmath>

#include "AudioResampler.h"
#include "DriftController.h"

namespace mozilla {

extern LazyLogModule gMediaTrackGraphLog;

#define LOG_CONTROLLER(level, controller, format, ...)             \
  MOZ_LOG(gMediaTrackGraphLog, level,                              \
          ("DriftController %p: (plot-id %u) " format, controller, \
           (controller)->mPlotId, ##__VA_ARGS__))

static media::TimeUnit DesiredBuffering(media::TimeUnit aSourceLatency) {
  constexpr media::TimeUnit kMinBuffer(10, MSECS_PER_S);
  constexpr media::TimeUnit kMaxBuffer(2500, MSECS_PER_S);

  const auto clamped = std::clamp(aSourceLatency, kMinBuffer, kMaxBuffer);

  // Ensure the base is the source's sampling rate.
  return clamped.ToBase(aSourceLatency);
}

AudioDriftCorrection::AudioDriftCorrection(
    uint32_t aSourceRate, uint32_t aTargetRate,
    const PrincipalHandle& aPrincipalHandle)
    : mTargetRate(aTargetRate),
      mDriftController(MakeUnique<DriftController>(aSourceRate, aTargetRate,
                                                   mDesiredBuffering)),
      mResampler(MakeUnique<AudioResampler>(
          aSourceRate, aTargetRate, mDesiredBuffering, aPrincipalHandle)) {}

AudioDriftCorrection::~AudioDriftCorrection() = default;

AudioSegment AudioDriftCorrection::RequestFrames(const AudioSegment& aInput,
                                                 uint32_t aOutputFrames) {
  const media::TimeUnit inputDuration(aInput.GetDuration(),
                                      mDriftController->mSourceRate);
  const media::TimeUnit outputDuration(aOutputFrames, mTargetRate);

  if (inputDuration.IsPositive()) {
    if (mDesiredBuffering.IsZero()) {
      // Start with the desired buffering at at least 50ms, since the drift is
      // still unknown. It may be adjust downward later on, when we have adapted
      // to the drift more.
      const media::TimeUnit desiredBuffering = DesiredBuffering(std::max(
          inputDuration * 11 / 10, media::TimeUnit::FromSeconds(0.05)));
      LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                     "Initial desired buffering %.2fms",
                     desiredBuffering.ToSeconds() * 1000.0);
      SetDesiredBuffering(desiredBuffering);
    } else if (inputDuration > mDesiredBuffering) {
      // Input latency is higher than the desired buffering. Increase the
      // desired buffering to try to avoid underruns.
      if (inputDuration > mSourceLatency) {
        const media::TimeUnit desiredBuffering =
            DesiredBuffering(inputDuration * 11 / 10);
        LOG_CONTROLLER(
            LogLevel::Info, mDriftController.get(),
            "High observed input latency %.2fms (%" PRId64
            " frames). Increasing desired buffering %.2fms->%.2fms frames",
            inputDuration.ToSeconds() * 1000.0, aInput.GetDuration(),
            mDesiredBuffering.ToSeconds() * 1000.0,
            desiredBuffering.ToSeconds() * 1000.0);
        SetDesiredBuffering(desiredBuffering);
      } else {
        const media::TimeUnit desiredBuffering =
            DesiredBuffering(mSourceLatency * 11 / 10);
        LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                       "Increasing desired buffering %.2fms->%.2fms, "
                       "based on reported input-latency %.2fms.",
                       mDesiredBuffering.ToSeconds() * 1000.0,
                       desiredBuffering.ToSeconds() * 1000.0,
                       mSourceLatency.ToSeconds() * 1000.0);
        SetDesiredBuffering(desiredBuffering);
      }
    }

    mIsHandlingUnderrun = false;
    // Very important to go first since DynamicResampler will get the sample
    // format from the chunk.
    mResampler->AppendInput(aInput);
  }
  bool hasUnderrun = false;
  AudioSegment output = mResampler->Resample(aOutputFrames, &hasUnderrun);
  mDriftController->UpdateClock(inputDuration, outputDuration,
                                CurrentBuffering(), BufferSize());
  // Update resampler's rate if there is a new correction.
  mResampler->UpdateOutRate(mDriftController->GetCorrectedTargetRate());
  if (hasUnderrun) {
    if (!mIsHandlingUnderrun) {
      NS_WARNING("Drift-correction: Underrun");
      LOG_CONTROLLER(LogLevel::Info, mDriftController.get(),
                     "Underrun. Doubling the desired buffering %.2fms->%.2fms",
                     mDesiredBuffering.ToSeconds() * 1000.0,
                     (mDesiredBuffering * 2).ToSeconds() * 1000.0);
      mIsHandlingUnderrun = true;
      ++mNumUnderruns;
      SetDesiredBuffering(DesiredBuffering(mDesiredBuffering * 2));
      mDriftController->ResetAfterUnderrun();
    }
  }

  if (mDriftController->DurationWithinHysteresis() >
          mLatencyReductionTimeLimit &&
      mDriftController->DurationSinceDesiredBufferingChange() >
          mLatencyReductionTimeLimit) {
    // We have been stable within hysteresis for a while. Let's reduce the
    // desired buffering if we can.
    const media::TimeUnit sourceLatency =
        mDriftController->MeasuredSourceLatency();
    // We target 30% over the measured source latency, a bit higher than how we
    // adapt to high source latency.
    const media::TimeUnit targetDesiredBuffering =
        DesiredBuffering(sourceLatency * 13 / 10);
    if (targetDesiredBuffering < mDesiredBuffering) {
      // The new target is lower than the current desired buffering. Proceed by
      // reducing the difference by 10%, but do it in 10ms-steps so there is a
      // chance of reaching the target (by truncation).
      const media::TimeUnit diff =
          (mDesiredBuffering - targetDesiredBuffering) / 10;
      // Apply the 10%-diff and 2ms-steps, but don't go lower than the
      // already-decided desired target.
      const media::TimeUnit target = std::max(
          targetDesiredBuffering, (mDesiredBuffering - diff).ToBase(500));
      if (target < mDesiredBuffering) {
        LOG_CONTROLLER(
            LogLevel::Info, mDriftController.get(),
            "Reducing desired buffering because the buffering level is stable. "
            "%.2fms->%.2fms. Measured source latency is %.2fms, ideal target "
            "is %.2fms.",
            mDesiredBuffering.ToSeconds() * 1000.0, target.ToSeconds() * 1000.0,
            sourceLatency.ToSeconds() * 1000.0,
            targetDesiredBuffering.ToSeconds() * 1000.0);
        SetDesiredBuffering(target);
      }
    }
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

void AudioDriftCorrection::SetSourceLatency(media::TimeUnit aSourceLatency) {
  LOG_CONTROLLER(
      LogLevel::Info, mDriftController.get(), "SetSourceLatency %.2fms->%.2fms",
      mSourceLatency.ToSeconds() * 1000.0, aSourceLatency.ToSeconds() * 1000.0);

  mSourceLatency = aSourceLatency;
}

void AudioDriftCorrection::SetDesiredBuffering(
    media::TimeUnit aDesiredBuffering) {
  mDesiredBuffering = aDesiredBuffering;
  mDriftController->SetDesiredBuffering(mDesiredBuffering);
  mResampler->SetPreBufferDuration(mDesiredBuffering);
}
}  // namespace mozilla

#undef LOG_CONTROLLER
