/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIO_DRIFT_CORRECTION_H_
#define MOZILLA_AUDIO_DRIFT_CORRECTION_H_

#include "DynamicResampler.h"

namespace mozilla {

extern LazyLogModule gMediaTrackGraphLog;

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
             uint32_t aDesiredBuffering)
      : mSourceRate(aSourceRate),
        mTargetRate(aTargetRate),
        mDesiredBuffering(aDesiredBuffering) {}

  /**
   * The correction in the form of a ratio. A correction of 0.98 means that the
   * target is 2% slower compared to the source or 1.03 which means that the
   * target is 3% faster than the source.
   */
  float GetCorrection() { return mCorrection; }

  /**
   * Update the available source frames, target frames, and the current
   * buffer, in every iteration. If the conditions are met a new correction is
   * calculated. A new correction is calculated in the following cases:
   *   1. Every mAdjustmentIntervalMs milliseconds (1000ms).
   *   2. Every time we run low on buffered frames (less than 20ms).
   * In addition to that, the correction is clamped to 10% to avoid sound
   * distortion so the result will be in [0.9, 1.1].
   */
  void UpdateClock(uint32_t aSourceFrames, uint32_t aTargetFrames,
                   uint32_t aBufferedFrames, uint32_t aRemainingFrames) {
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
    mTargetClock += aTargetFrames;
    mSourceClock += aSourceFrames;
  }

 private:
  /**
   * aCalculationWeight is a percentage [0, 1] with which the calculated
   * correction will be weighted. The existing correction will be weighted with
   * 1 - aCalculationWeight. This gives some inertia to the speed at which the
   * correction changes, for smoother changes.
   */
  void CalculateCorrection(float aCalculationWeight, uint32_t aBufferedFrames,
                           uint32_t aRemainingFrames) {
    // We want to maintain the desired buffer
    uint32_t bufferedFramesDiff = aBufferedFrames - mDesiredBuffering;
    uint32_t resampledSourceClock =
        std::max(1u, mSourceClock + bufferedFramesDiff);
    if (mTargetRate != mSourceRate) {
      resampledSourceClock *= static_cast<float>(mTargetRate) / mSourceRate;
    }

    MOZ_LOG(gMediaTrackGraphLog, LogLevel::Verbose,
            ("ClockDrift %p Calculated correction %.3f (with weight: %.1f -> "
             "%.3f) (buffer: %u, desired: %u, remaining: %u)",
             this, static_cast<float>(mTargetClock) / resampledSourceClock,
             aCalculationWeight,
             (1 - aCalculationWeight) * mCorrection +
                 aCalculationWeight * mTargetClock / resampledSourceClock,
             aBufferedFrames, mDesiredBuffering, aRemainingFrames));

    mCorrection = (1 - aCalculationWeight) * mCorrection +
                  aCalculationWeight * mTargetClock / resampledSourceClock;

    // Clamp to range [0.9, 1.1] to avoid distortion
    mCorrection = std::min(std::max(mCorrection, 0.9f), 1.1f);

    // Reset the counters to prepare for the next period.
    mTargetClock = 0;
    mSourceClock = 0;
  }

 public:
  const uint32_t mSourceRate;
  const uint32_t mTargetRate;
  const uint32_t mAdjustmentIntervalMs = 1000;
  const uint32_t mDesiredBuffering;

 private:
  float mCorrection = 1.0;

  uint32_t mSourceClock = 0;
  uint32_t mTargetClock = 0;
};

/**
 * Correct the drift between two independent clocks, the source, and the target
 * clock. The target clock is the master clock so the correction syncs the drift
 * of the source clock to the target. The nominal sampling rates of source and
 * target must be provided. If the source and the target operate in different
 * sample rate the drift correction will be performed on the top of resampling
 * from the source rate to the target rate.
 *
 * It works with AudioSegment in order to be able to be used from the
 * MediaTrackGraph/MediaTrack. The audio buffers are pre-allocated so there is
 * no new allocation takes place during operation. The preallocation capacity is
 * 100ms for input and 100ms for output. The class consists of ClockDrift and
 * AudioResampler check there for more details.
 *
 * The class is not thread-safe. The construction can happen in any thread but
 * the member method must be used in a single thread that can be different than
 * the construction thread. Appropriate for being used in the high priority
 * audio thread.
 */
class AudioDriftCorrection final {
  const uint32_t kMinBufferMs = 5;

 public:
  AudioDriftCorrection(uint32_t aSourceRate, uint32_t aTargetRate,
                       uint32_t aBufferMs,
                       const PrincipalHandle& aPrincipalHandle)
      : mDesiredBuffering(std::max(kMinBufferMs, aBufferMs) * aSourceRate /
                          1000),
        mTargetRate(aTargetRate),
        mClockDrift(aSourceRate, aTargetRate, mDesiredBuffering),
        mResampler(aSourceRate, aTargetRate, mDesiredBuffering,
                   aPrincipalHandle) {}

  /**
   * The source audio frames and request the number of target audio frames must
   * be provided. The duration of the source and the output is considered as the
   * source clock and the target clock. The input is buffered internally so some
   * latency exists. The returned AudioSegment must be cleaned up because the
   * internal buffer will be reused after 100ms. If the drift correction (and
   * possible resampling) is not possible due to lack of input data an empty
   * AudioSegment will be returned. Not thread-safe.
   */
  AudioSegment RequestFrames(const AudioSegment& aInput,
                             uint32_t aOutputFrames) {
    // Very important to go first since the Dynamic will get the sample format
    // from the chunk.
    if (aInput.GetDuration()) {
      // Always go through the resampler because the clock might shift later.
      mResampler.AppendInput(aInput);
    }
    mClockDrift.UpdateClock(aInput.GetDuration(), aOutputFrames,
                            mResampler.InputReadableFrames(),
                            mResampler.InputWritableFrames());
    TrackRate receivingRate = mTargetRate * mClockDrift.GetCorrection();
    // Update resampler's rate if there is a new correction.
    mResampler.UpdateOutRate(receivingRate);
    // If it does not have enough frames the result will be an empty segment.
    AudioSegment output = mResampler.Resample(aOutputFrames);
    if (output.IsEmpty()) {
      NS_WARNING("Got nothing from the resampler");
      output.AppendNullData(aOutputFrames);
    }
    return output;
  }

  // Only accessible from the same thread that is driving RequestFrames().
  uint32_t CurrentBuffering() const { return mResampler.InputReadableFrames(); }

  const uint32_t mDesiredBuffering;
  const uint32_t mTargetRate;

 private:
  ClockDrift mClockDrift;
  AudioResampler mResampler;
};

};     // namespace mozilla
#endif /* MOZILLA_AUDIO_DRIFT_CORRECTION_H_ */
