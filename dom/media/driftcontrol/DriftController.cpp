/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DriftController.h"

#include <atomic>
#include <cmath>
#include <mutex>

#include "mozilla/CheckedInt.h"
#include "mozilla/Logging.h"

namespace mozilla {

LazyLogModule gDriftControllerGraphsLog("DriftControllerGraphs");
extern LazyLogModule gMediaTrackGraphLog;

#define LOG_CONTROLLER(level, controller, format, ...)             \
  MOZ_LOG(gMediaTrackGraphLog, level,                              \
          ("DriftController %p: (plot-id %u) " format, controller, \
           (controller)->mPlotId, ##__VA_ARGS__))
#define LOG_PLOT_NAMES()                                                       \
  MOZ_LOG(                                                                     \
      gDriftControllerGraphsLog, LogLevel::Verbose,                            \
      ("id,t,buffering,desired,buffersize,inlatency,outlatency,inrate,"        \
       "outrate,hysteresisthreshold,corrected,hysteresiscorrected,configured," \
       "p,i,d,kpp,kii,kdd,control"))
#define LOG_PLOT_VALUES(id, t, buffering, desired, buffersize, inlatency,    \
                        outlatency, inrate, outrate, hysteresisthreshold,    \
                        corrected, hysteresiscorrected, configured, p, i, d, \
                        kpp, kii, kdd, control)                              \
  MOZ_LOG(                                                                   \
      gDriftControllerGraphsLog, LogLevel::Verbose,                          \
      ("DriftController %u,%.3f,%u,%" PRId64 ",%u,%" PRId64 ",%" PRId64      \
       ",%u,%u,%" PRId64 ",%.5f,%.5f,%ld,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f",  \
       id, t, buffering, desired, buffersize, inlatency, outlatency, inrate, \
       outrate, hysteresisthreshold, corrected, hysteresiscorrected,         \
       configured, p, i, d, kpp, kii, kdd, control))

static uint8_t GenerateId() {
  static std::atomic<uint8_t> id{0};
  return ++id;
}

DriftController::DriftController(uint32_t aSourceRate, uint32_t aTargetRate,
                                 media::TimeUnit aDesiredBuffering)
    : mPlotId(GenerateId()),
      mSourceRate(aSourceRate),
      mTargetRate(aTargetRate),
      mDesiredBuffering(aDesiredBuffering),
      mCorrectedTargetRate(static_cast<float>(aTargetRate)),
      mMeasuredSourceLatency(5),
      mMeasuredTargetLatency(5) {
  LOG_CONTROLLER(
      LogLevel::Info, this,
      "Created. Resampling %uHz->%uHz. Initial desired buffering: %.2fms.",
      mSourceRate, mTargetRate, mDesiredBuffering.ToSeconds() * 1000.0);
  static std::once_flag sOnceFlag;
  std::call_once(sOnceFlag, [] { LOG_PLOT_NAMES(); });
}

void DriftController::SetDesiredBuffering(media::TimeUnit aDesiredBuffering) {
  LOG_CONTROLLER(LogLevel::Debug, this, "SetDesiredBuffering %.2fms->%.2fms",
                 mDesiredBuffering.ToSeconds() * 1000.0,
                 aDesiredBuffering.ToSeconds() * 1000.0);
  mLastDesiredBufferingChangeTime = mTotalTargetClock;
  mDesiredBuffering = aDesiredBuffering.ToBase(mSourceRate);
}

void DriftController::ResetAfterUnderrun() {
  mIntegral = 0.0;
  mPreviousError = 0.0;
  // Trigger a recalculation on the next clock update.
  mTargetClock = mAdjustmentInterval;
}

uint32_t DriftController::GetCorrectedTargetRate() const {
  return std::lround(mCorrectedTargetRate);
}

void DriftController::UpdateClock(media::TimeUnit aSourceDuration,
                                  media::TimeUnit aTargetDuration,
                                  uint32_t aBufferedFrames,
                                  uint32_t aBufferSize) {
  mTargetClock += aTargetDuration;
  mTotalTargetClock += aTargetDuration;

  mMeasuredTargetLatency.insert(aTargetDuration);

  if (aSourceDuration.IsZero()) {
    // Only update the clock after having received input, so input buffering
    // estimates are somewhat recent. This helps stabilize the controller
    // input (buffering measurements) when the input stream's callback
    // interval is much larger than that of the output stream.
    return;
  }

  mMeasuredSourceLatency.insert(aSourceDuration);

  if (mTargetClock >= mAdjustmentInterval) {
    // The adjustment interval has passed. Recalculate.
    CalculateCorrection(aBufferedFrames, aBufferSize);
  }
}

void DriftController::CalculateCorrection(uint32_t aBufferedFrames,
                                          uint32_t aBufferSize) {
  static constexpr float kProportionalGain = 0.07;
  static constexpr float kIntegralGain = 0.006;
  static constexpr float kDerivativeGain = 0.12;

  // Maximum 0.1% change per update.
  const float cap = static_cast<float>(mTargetRate) / 1000.0f;

  // The integral term can make us grow far outside the cap. Impose a cap on
  // it individually that is roughly equivalent to the final cap.
  const float integralCap = cap / kIntegralGain;

  int32_t error = CheckedInt32(mDesiredBuffering.ToTicksAtRate(mSourceRate) -
                               aBufferedFrames)
                      .value();
  int32_t proportional = error;
  // targetClockSec is the number of target clock seconds since last
  // correction.
  float targetClockSec = static_cast<float>(mTargetClock.ToSeconds());
  // delta-t is targetClockSec.
  float integralStep = std::clamp(static_cast<float>(error) * targetClockSec,
                                  -integralCap, integralCap);
  mIntegral += integralStep;
  float derivative =
      static_cast<float>(error - mPreviousError) / targetClockSec;
  float controlSignal = kProportionalGain * static_cast<float>(proportional) +
                        kIntegralGain * mIntegral +
                        kDerivativeGain * derivative;
  float correctedRate =
      std::clamp(static_cast<float>(mTargetRate) + controlSignal,
                 mCorrectedTargetRate - cap, mCorrectedTargetRate + cap);

  // mDesiredBuffering is divided by this to calculate the amount of
  // hysteresis to apply. With a denominator of 5, an error within +/- 20% of
  // the desired buffering will not make corrections to the target sample
  // rate.
  static constexpr uint32_t kHysteresisDenominator = 5;  // +/- 20%

  // +/- 10ms hysteresis maximum.
  const media::TimeUnit hysteresisCap = media::TimeUnit::FromSeconds(0.01);

  // For the minimum desired buffering of 10ms we have a hysteresis threshold
  // of +/- 2ms (20%). This goes up to +/- 10ms (clamped) at most for when the
  // desired buffering is 50 ms or higher.
  const auto hysteresisThreshold =
      std::min(hysteresisCap, mDesiredBuffering / kHysteresisDenominator)
          .ToTicksAtRate(mSourceRate);

  float hysteresisCorrectedRate = [&] {
    uint32_t abserror = std::abs(error);
    if (abserror > hysteresisThreshold) {
      // The error is outside a hysteresis threshold boundary.
      mDurationWithinHysteresis = media::TimeUnit::Zero();
      mIntegralCenterForCap = Nothing();
      mLastHysteresisBoundaryCorrection = Some(error);
      return correctedRate;
    }

    // The error is within the hysteresis threshold boundaries.
    mDurationWithinHysteresis += mTargetClock;
    if (!mIntegralCenterForCap) {
      mIntegralCenterForCap = Some(mIntegral);
    }

    // Would prefer std::signbit, but..
    // https://github.com/microsoft/STL/issues/519.
    if (mLastHysteresisBoundaryCorrection &&
        (*mLastHysteresisBoundaryCorrection < 0) != (error < 0) &&
        abserror > hysteresisThreshold * 3 / 10) {
      // The error came from a boundary and just went 30% past the center line
      // (of the distance between center and boundary). Correct now rather
      // than when reaching the opposite boundary, so we have a chance of
      // finding a stable rate.
      mLastHysteresisBoundaryCorrection = Nothing();
      return correctedRate;
    }

    return mCorrectedTargetRate;
  }();

  if (mDurationWithinHysteresis > mIntegralCapTimeLimit) {
    // Impose a cap on the integral term to not let it grow unboundedly
    // while we're within the hysteresis threshold boundaries. Since the
    // integral is what finds the drift we center the cap around the integral's
    // value when we entered the hysteresis threshold rarther than around 0. We
    // impose the cap only after the error has been within the hysteresis
    // threshold boundaries for some time, since it would otherwise increase the
    // time it takes to reach stability.
    mIntegral = std::clamp(mIntegral, *mIntegralCenterForCap - integralCap,
                           *mIntegralCenterForCap + integralCap);
  }

  LOG_CONTROLLER(
      LogLevel::Verbose, this,
      "Recalculating Correction: Nominal: %uHz->%uHz, Corrected: "
      "%uHz->%.2fHz  (diff %.2fHz), error: %.2fms (hysteresisThreshold: "
      "%.2fms), buffering: %.2fms, desired buffering: %.2fms",
      mSourceRate, mTargetRate, mSourceRate, hysteresisCorrectedRate,
      hysteresisCorrectedRate - mCorrectedTargetRate,
      media::TimeUnit(error, mSourceRate).ToSeconds() * 1000.0,
      media::TimeUnit(hysteresisThreshold, mSourceRate).ToSeconds() * 1000.0,
      media::TimeUnit(aBufferedFrames, mSourceRate).ToSeconds() * 1000.0,
      mDesiredBuffering.ToSeconds() * 1000.0);
  LOG_PLOT_VALUES(mPlotId, mTotalTargetClock.ToSeconds(), aBufferedFrames,
                  mDesiredBuffering.ToTicksAtRate(mSourceRate), aBufferSize,
                  mMeasuredSourceLatency.mean().ToTicksAtRate(mSourceRate),
                  mMeasuredTargetLatency.mean().ToTicksAtRate(mTargetRate),
                  mSourceRate, mTargetRate, hysteresisThreshold, correctedRate,
                  hysteresisCorrectedRate, std::lround(hysteresisCorrectedRate),
                  proportional, mIntegral, derivative,
                  kProportionalGain * proportional, kIntegralGain * mIntegral,
                  kDerivativeGain * derivative, controlSignal);

  if (std::lround(mCorrectedTargetRate) !=
      std::lround(hysteresisCorrectedRate)) {
    ++mNumCorrectionChanges;
  }

  mPreviousError = error;
  mCorrectedTargetRate = hysteresisCorrectedRate;

  // Reset the counters to prepare for the next period.
  mTargetClock = media::TimeUnit::Zero();
}
}  // namespace mozilla

#undef LOG_PLOT_VALUES
#undef LOG_PLOT_NAMES
#undef LOG_CONTROLLER
