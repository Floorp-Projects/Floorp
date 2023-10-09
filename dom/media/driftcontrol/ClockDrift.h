/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_CLOCKDRIFT_H_
#define DOM_MEDIA_DRIFTCONTROL_CLOCKDRIFT_H_
#include "mozilla/RollingMean.h"

#include <algorithm>
#include <cstdint>

#include "MediaSegment.h"

namespace mozilla {

/**
 * ClockDrift calculates the diverge of the source clock from the nominal
 * (provided) rate compared to the target clock, which is considered the master
 * clock. In the case of different sampling rates, it is assumed that resampling
 * will take place so the returned correction is estimated after the resampling.
 * That means that resampling is taken into account in the calculations but it
 * does appear in the correction. The correction must be applied to the top of
 * the resampling.
 *
 * It works by measuring the incoming, the outgoing frames, and the amount of
 * buffered data and estimates the correction needed. The correction logic has
 * been created with two things in mind. First, not to run out of frames because
 * that means the audio will glitch. Second, not to change the correction very
 * often because this will result in a change in the resampling ratio. The
 * resampler recreates its internal memory when the ratio changes which has a
 * performance impact.
 *
 * The pref `media.clock drift.buffering` can be used to configure the desired
 * internal buffering. Right now it is at 50ms. But it can be increased if there
 * are audio quality problems.
 */
class ClockDrift final {
 public:
  /**
   * Provide the nominal source and the target sample rate.
   */
  ClockDrift(uint32_t aSourceRate, uint32_t aTargetRate,
             uint32_t aDesiredBuffering);

  /**
   * The correction in the form of a ratio. A correction of 0.98 means that the
   * target is 2% slower compared to the source or 1.03 which means that the
   * target is 3% faster than the source.
   */
  float GetCorrection() { return mCorrection; }

  /**
   * The number of times mCorrection has been changed to adjust to drift.
   */
  uint32_t NumCorrectionChanges() const { return mNumCorrectionChanges; }

  /**
   * Update the available source frames, target frames, and the current
   * buffer, in every iteration. If the conditions are met a new correction is
   * calculated. A new correction is calculated in the following cases:
   *   1. There is source frames present, which if the source latency is high
   *      means the comparison between source and target is fairly accurate, and
   *   2.1. Every mAdjustmentIntervalMs milliseconds (1000ms), or
   *   2.2. Every time we run low on buffered frames (less than 20ms).
   * In addition to that, the correction is clamped to 10% to avoid sound
   * distortion so the result will be in [0.9, 1.1].
   */
  void UpdateClock(uint32_t aSourceFrames, uint32_t aTargetFrames,
                   uint32_t aBufferedFrames, uint32_t aRemainingFrames);

 private:
  /**
   * aCalculationWeight is a percentage [0, 1] with which the calculated
   * correction will be weighted. The existing correction will be weighted with
   * 1 - aCalculationWeight. This gives some inertia to the speed at which the
   * correction changes, for smoother changes.
   */
  void CalculateCorrection(float aCalculationWeight, uint32_t aBufferedFrames,
                           uint32_t aRemainingFrames);

 public:
  const uint8_t mPlotId;
  const uint32_t mSourceRate;
  const uint32_t mTargetRate;
  const uint32_t mAdjustmentIntervalMs = 1000;
  const uint32_t mDesiredBuffering;

 private:
  float mCorrection = 1.0;
  uint32_t mNumCorrectionChanges = 0;

  // An estimate of the source's latency, i.e. callback buffer size, in frames.
  RollingMean<TrackTime, TrackTime> mMeasuredSourceLatency;
  // An estimate of the target's latency, i.e. callback buffer size, in frames.
  RollingMean<TrackTime, TrackTime> mMeasuredTargetLatency;

  uint32_t mSourceClock = 0;
  uint32_t mTargetClock = 0;
  TrackTime mTotalTargetClock = 0;
};

}  // namespace mozilla
#endif  // DOM_MEDIA_DRIFTCONTROL_CLOCKDRIFT_H_
