/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_
#define DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_

#include "TimeUnits.h"
#include "mozilla/RollingMean.h"

#include <algorithm>
#include <cstdint>

#include "MediaSegment.h"

namespace mozilla {

/**
 * DriftController calculates the divergence of the source clock from its
 * nominal (provided) rate compared to that of the target clock, which drives
 * the calculations.
 *
 * The DriftController looks at how the current buffering level differs from the
 * desired buffering level and sets a corrected target rate. A resampler should
 * be configured to resample from the nominal source rate to the corrected
 * target rate. It assumes that the resampler is initially configured to
 * resample from the nominal source rate to the nominal target rate.
 *
 * The pref `media.clock drift.buffering` can be used to configure the minimum
 * initial desired internal buffering. Right now it is at 50ms. A larger desired
 * buffering level will be used if deemed necessary based on input device
 * latency, reported or observed. It will also be increased as a response to an
 * underrun, since that indicates the buffer was too small.
 */
class DriftController final {
 public:
  /**
   * Provide the nominal source and the target sample rate.
   */
  DriftController(uint32_t aSourceRate, uint32_t aTargetRate,
                  media::TimeUnit aDesiredBuffering);

  /**
   * Set the buffering level that the controller should target.
   */
  void SetDesiredBuffering(media::TimeUnit aDesiredBuffering);

  /**
   * Reset internal PID-controller state in a way that is suitable for handling
   * an underrun.
   */
  void ResetAfterUnderrun();

  /**
   * Returns the drift-corrected target rate.
   */
  uint32_t GetCorrectedTargetRate() const;

  /**
   * The number of times mCorrectedTargetRate has been changed to adjust to
   * drift.
   */
  uint32_t NumCorrectionChanges() const { return mNumCorrectionChanges; }

  /**
   * The amount of time the buffering level has been within the hysteresis
   * threshold.
   */
  media::TimeUnit DurationWithinHysteresis() const {
    return mDurationWithinHysteresis;
  }

  /**
   * The amount of time that has passed since the last time SetDesiredBuffering
   * was called.
   */
  media::TimeUnit DurationSinceDesiredBufferingChange() const {
    return mTotalTargetClock - mLastDesiredBufferingChangeTime;
  }

  /**
   * A rolling window average measurement of source latency by looking at the
   * duration of the source buffer.
   */
  media::TimeUnit MeasuredSourceLatency() const {
    return mMeasuredSourceLatency.mean();
  }

  /**
   * Update the available source frames, target frames, and the current
   * buffer, in every iteration. If the conditions are met a new correction is
   * calculated. A new correction is calculated every mAdjustmentInterval. In
   * addition to that, the correction is clamped so that the output sample rate
   * changes by at most 0.1% of its nominal rate each correction.
   */
  void UpdateClock(media::TimeUnit aSourceDuration,
                   media::TimeUnit aTargetDuration, uint32_t aBufferedFrames,
                   uint32_t aBufferSize);

 private:
  // This implements a simple PID controller with feedback.
  // Set point:     SP = mDesiredBuffering.
  // Process value: PV(t) = aBufferedFrames. This is the feedback.
  // Error:         e(t) = mDesiredBuffering - aBufferedFrames.
  // Control value: CV(t) = the number to add to the nominal target rate, i.e.
  //                the corrected target rate = CV(t) + nominal target rate.
  //
  // Controller:
  // Proportional part: The error, p(t) = e(t), multiplied by a gain factor, Kp.
  // Integral part:     The historic cumulative value of the error,
  //                    i(t+1) = i(t) + e(t+1), multiplied by a gain factor, Ki.
  // Derivative part:   The error's rate of change, d(t+1) = (e(t+1)-e(t))/1,
  //                    multiplied by a gain factor, Kd.
  // Control signal:    The sum of the parts' output,
  //                    u(t) = Kp*p(t) + Ki*i(t) + Kd*d(t).
  //
  // Control action: Converting the control signal to a target sample rate.
  //                 Simplified, a positive control signal means the buffer is
  //                 lower than desired (because the error is positive), so the
  //                 target sample rate must be increased in order to consume
  //                 input data slower. We calculate the corrected target rate
  //                 by simply adding the control signal, u(t), to the nominal
  //                 target rate.
  //
  // Hysteresis: As long as the error is within a threshold of 20% of the set
  //             point (desired buffering level) (up to 10ms for >50ms desired
  //             buffering), we call this the hysteresis threshold, the control
  //             signal does not influence the corrected target rate at all.
  //             This is to reduce the frequency at which we need to reconfigure
  //             the resampler, as it causes some allocations.
  void CalculateCorrection(uint32_t aBufferedFrames, uint32_t aBufferSize);

 public:
  const uint8_t mPlotId;
  const uint32_t mSourceRate;
  const uint32_t mTargetRate;
  const media::TimeUnit mAdjustmentInterval = media::TimeUnit::FromSeconds(1);
  const media::TimeUnit mIntegralCapTimeLimit =
      media::TimeUnit(10, 1).ToBase(mTargetRate);

 private:
  media::TimeUnit mDesiredBuffering;
  int32_t mPreviousError = 0;
  float mIntegral = 0.0;
  Maybe<float> mIntegralCenterForCap;
  float mCorrectedTargetRate;
  Maybe<int32_t> mLastHysteresisBoundaryCorrection;
  media::TimeUnit mDurationWithinHysteresis;
  uint32_t mNumCorrectionChanges = 0;

  // An estimate of the source's latency, i.e. callback buffer size, in frames.
  RollingMean<media::TimeUnit, media::TimeUnit> mMeasuredSourceLatency;
  // An estimate of the target's latency, i.e. callback buffer size, in frames.
  RollingMean<media::TimeUnit, media::TimeUnit> mMeasuredTargetLatency;

  media::TimeUnit mTargetClock;
  media::TimeUnit mTotalTargetClock;
  media::TimeUnit mLastDesiredBufferingChangeTime;
};

}  // namespace mozilla
#endif  // DOM_MEDIA_DRIFTCONTROL_DRIFTCONTROLLER_H_
