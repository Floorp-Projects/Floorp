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
#define LOG_PLOT_NAMES()                                                    \
  MOZ_LOG(gDriftControllerGraphsLog, LogLevel::Verbose,                     \
          ("id,t,buffering,desired,buffersize,inlatency,outlatency,inrate," \
           "outrate,corrected,configured,p,i,d,kpp,kii,kdd,control"))
#define LOG_PLOT_VALUES(id, t, buffering, desired, buffersize, inlatency,      \
                        outlatency, inrate, outrate, corrected, configured, p, \
                        i, d, kpp, kii, kdd, control)                          \
  MOZ_LOG(                                                                     \
      gDriftControllerGraphsLog, LogLevel::Verbose,                            \
      ("DriftController %u,%.3f,%u,%u,%u,%u,%u,%u,%u,%.5f,%ld,%d,%.5f,%.5f,"   \
       "%.5f,%.5f,%.5f,%.5f",                                                  \
       id, t, buffering, desired, buffersize, inlatency, outlatency, inrate,   \
       outrate, corrected, configured, p, i, d, kpp, kii, kdd, control))

static uint8_t GenerateId() {
  static std::atomic<uint8_t> id{0};
  return ++id;
}

DriftController::DriftController(uint32_t aSourceRate, uint32_t aTargetRate,
                                 uint32_t aDesiredBuffering)
    : mPlotId(GenerateId()),
      mSourceRate(aSourceRate),
      mTargetRate(aTargetRate),
      mDesiredBuffering(aDesiredBuffering),
      mCorrectedTargetRate(static_cast<float>(aTargetRate)),
      mMeasuredSourceLatency(5),
      mMeasuredTargetLatency(5) {
  LOG_CONTROLLER(
      LogLevel::Info, this,
      "Created. Resampling %uHz->%uHz. Initial desired buffering: %u frames.",
      mSourceRate, mTargetRate, mDesiredBuffering);
  static std::once_flag sOnceFlag;
  std::call_once(sOnceFlag, [] { LOG_PLOT_NAMES(); });
}

void DriftController::SetDesiredBuffering(uint32_t aDesiredBuffering) {
  LOG_CONTROLLER(LogLevel::Debug, this, "SetDesiredBuffering %u->%u",
                 mDesiredBuffering, aDesiredBuffering);
  mDesiredBuffering = aDesiredBuffering;
}

uint32_t DriftController::GetCorrectedTargetRate() const {
  return std::lround(mCorrectedTargetRate);
}

void DriftController::UpdateClock(uint32_t aSourceFrames,
                                  uint32_t aTargetFrames,
                                  uint32_t aBufferedFrames,
                                  uint32_t aBufferSize) {
  mTargetClock += aTargetFrames;
  mTotalTargetClock += aTargetFrames;

  mMeasuredTargetLatency.insert(aTargetFrames);

  if (aSourceFrames == 0) {
    // Only update the clock after having received input, so input buffering
    // estimates are somewhat recent. This helps stabilize the controller
    // input (buffering measurements) when the input stream's callback
    // interval is much larger than that of the output stream.
    return;
  }

  mMeasuredSourceLatency.insert(aSourceFrames);

  if ((mTargetClock * 1000 / mTargetRate) >= mAdjustmentIntervalMs) {
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

  int32_t error = (CheckedInt32(mDesiredBuffering) - aBufferedFrames).value();
  int32_t proportional = error;
  // targetClockSec is the number of target clock seconds since last
  // correction.
  float targetClockSec = static_cast<float>(mTargetClock) / mTargetRate;
  // delta-t is targetClockSec.
  float integralStep = static_cast<float>(error) * targetClockSec;
  mIntegral += integralStep;
  float derivative =
      static_cast<float>(error - mPreviousError) / targetClockSec;
  float controlSignal = kProportionalGain * static_cast<float>(proportional) +
                        kIntegralGain * mIntegral +
                        kDerivativeGain * derivative;
  float correctedRate =
      std::clamp(static_cast<float>(mTargetRate) + controlSignal,
                 mCorrectedTargetRate - cap, mCorrectedTargetRate + cap);

  LOG_CONTROLLER(
      LogLevel::Verbose, this,
      "Recalculating Correction: Nominal: %uHz->%uHz, Corrected: "
      "%uHz->%.2fHz  (diff %.2fHz), buffering: %u, desired buffering: %u",
      mSourceRate, mTargetRate, mSourceRate, correctedRate,
      correctedRate - mCorrectedTargetRate, aBufferedFrames, mDesiredBuffering);
  LOG_PLOT_VALUES(
      mPlotId, static_cast<double>(mTotalTargetClock) / mTargetRate,
      aBufferedFrames, mDesiredBuffering, aBufferSize,
      static_cast<uint32_t>(mMeasuredSourceLatency.mean()),
      static_cast<uint32_t>(mMeasuredTargetLatency.mean()), mSourceRate,
      mTargetRate, correctedRate, std::lround(correctedRate), proportional,
      mIntegral, derivative, kProportionalGain * proportional,
      kIntegralGain * mIntegral, kDerivativeGain * derivative, controlSignal);

  if (std::lround(mCorrectedTargetRate) != std::lround(correctedRate)) {
    ++mNumCorrectionChanges;
  }

  mPreviousError = error;
  mCorrectedTargetRate = correctedRate;

  // Reset the counters to prepare for the next period.
  mTargetClock = 0;
}
}  // namespace mozilla

#undef LOG_PLOT_VALUES
#undef LOG_PLOT_NAMES
#undef LOG_CONTROLLER
