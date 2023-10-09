/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClockDrift.h"

#include <atomic>
#include <mutex>

#include "mozilla/Logging.h"

namespace mozilla {
LazyLogModule gClockDriftGraphsLog("ClockDriftGraphs");
extern LazyLogModule gMediaTrackGraphLog;

#define LOG_PLOT_NAMES()                            \
  MOZ_LOG(gClockDriftGraphsLog, LogLevel::Verbose,  \
          ("id,t,buffering,desired,inrate,outrate," \
           "corrected"))
#define LOG_PLOT_VALUES(id, t, buffering, desired, inrate, outrate, corrected) \
  MOZ_LOG(gClockDriftGraphsLog, LogLevel::Verbose,                             \
          ("ClockDrift %u,%.3f,%u,%u,%u,%u,%.5f", id, t, buffering, desired,   \
           inrate, outrate, corrected))

static uint8_t GenerateId() {
  static std::atomic<uint8_t> id{0};
  return ++id;
}

ClockDrift::ClockDrift(uint32_t aSourceRate, uint32_t aTargetRate,
                       uint32_t aDesiredBuffering)
    : mPlotId(GenerateId()),
      mSourceRate(aSourceRate),
      mTargetRate(aTargetRate),
      mDesiredBuffering(aDesiredBuffering) {
  static std::once_flag sOnceFlag;
  std::call_once(sOnceFlag, [] { LOG_PLOT_NAMES(); });
}

void ClockDrift::UpdateClock(uint32_t aSourceFrames, uint32_t aTargetFrames,
                             uint32_t aBufferedFrames,
                             uint32_t aRemainingFrames) {
  mTargetClock += aTargetFrames;
  mSourceClock += aSourceFrames;
  mTotalTargetClock += aTargetFrames;

  if (mSourceClock >= mSourceRate / 10 || mTargetClock >= mTargetRate / 10) {
    // Only update the correction if 100ms has passed since last update.
    if (aBufferedFrames < mDesiredBuffering * 4 / 10 /*40%*/ ||
        aRemainingFrames < mDesiredBuffering * 4 / 10 /*40%*/) {
      // We are getting close to the lower or upper bound of the internal
      // buffer. Steer clear.
      CalculateCorrection(0.9, aBufferedFrames, aRemainingFrames);
    } else if ((mTargetClock * 1000 / mTargetRate) >= mAdjustmentIntervalMs ||
               (mSourceClock * 1000 / mSourceRate) >= mAdjustmentIntervalMs) {
      // The adjustment interval has passed on one side. Recalculate.
      CalculateCorrection(0.6, aBufferedFrames, aRemainingFrames);
    }
  }
}

void ClockDrift::CalculateCorrection(float aCalculationWeight,
                                     uint32_t aBufferedFrames,
                                     uint32_t aRemainingFrames) {
  // We want to maintain the desired buffer
  uint32_t bufferedFramesDiff = aBufferedFrames - mDesiredBuffering;
  uint32_t resampledSourceClock =
      std::max(1u, mSourceClock + bufferedFramesDiff);
  if (mTargetRate != mSourceRate) {
    resampledSourceClock *= static_cast<float>(mTargetRate) / mSourceRate;
  }

  MOZ_LOG(
      gMediaTrackGraphLog, LogLevel::Verbose,
      ("ClockDrift %p (plot-id %u) Calculated correction %.3f (with weight: "
       "%.1f -> %.3f) (buffer: %u, desired: %u, remaining: %u)",
       this, mPlotId, static_cast<float>(mTargetClock) / resampledSourceClock,
       aCalculationWeight,
       (1 - aCalculationWeight) * mCorrection +
           aCalculationWeight * mTargetClock / resampledSourceClock,
       aBufferedFrames, mDesiredBuffering, aRemainingFrames));

  auto oldCorrection = mCorrection;

  mCorrection = (1 - aCalculationWeight) * mCorrection +
                aCalculationWeight * mTargetClock / resampledSourceClock;

  LOG_PLOT_VALUES(mPlotId, static_cast<double>(mTotalTargetClock) / mTargetRate,
                  aBufferedFrames, mDesiredBuffering, mSourceRate, mTargetRate,
                  mCorrection * mTargetRate);

  if (oldCorrection != mCorrection) {
    // Do the comparison pre-clamping as a low number of correction changes
    // should indicate that we are close to the desired buffering (correction
    // 1).
    ++mNumCorrectionChanges;
  }

  // Clamp to range [0.9, 1.1] to avoid distortion
  mCorrection = std::min(std::max(mCorrection, 0.9f), 1.1f);

  // Reset the counters to prepare for the next period.
  mTargetClock = 0;
  mSourceClock = 0;
}
}  // namespace mozilla

#undef LOG_PLOT_VALUES
#undef LOG_PLOT_NAMES
