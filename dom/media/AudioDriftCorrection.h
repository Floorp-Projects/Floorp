/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_AUDIO_DRIFT_CORRECTION_H_
#define MOZILLA_AUDIO_DRIFT_CORRECTION_H_

#include "DynamicResampler.h"

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
 * internal buffering. Right now it is at 5ms. But it can be increased if there
 * are audio quality problems.
 */
class ClockDrift final {
 public:
  /**
   * Provide the nominal source and the target sample rate.
   */
  ClockDrift(int32_t aSourceRate, int32_t aTargetRate)
      : mSourceRate(aSourceRate),
        mTargetRate(aTargetRate),
        mDesiredBuffering(5 * mSourceRate / 100) {
    if (Preferences::HasUserValue("media.clockdrift.buffering")) {
      int msecs = Preferences::GetInt("media.clockdrift.buffering");
      mDesiredBuffering = msecs * mSourceRate / 100;
    }
  }

  /**
   * The correction in the form of a ratio. A correction of 0.98 means that the
   * target is 2% slower compared to the source or 1.03 which means that the
   * target is 3% faster than the source.
   */
  float GetCorrection() { return mCorrection; }

  /**
   * Update the available source frames, target frames, and the current
   * buffering, in every iteration. If the condition are met a new correction is
   * calculated. A new correction is calculated in the following cases:
   *   1. Every 100 iterations which mean every 100 calls of this method.
   *   2. Every time we run out of buffered frames (less than 2ms).
   * In addition to that, the correction is clamped to 10% to avoid sound
   * distortion so the result will be in [0.9, 1.1].
   */
  void UpdateClock(int aSourceClock, int aTargetClock, int aBufferedFrames) {
    if (mIterations == mAdjustementWindow) {
      CalculateCorrection(aBufferedFrames);
    } else if (aBufferedFrames < 2 * mSourceRate / 100 /*20ms*/) {
      BufferedFramesCorrection(aBufferedFrames);
    }
    mTargetClock += aTargetClock;
    mSourceClock += aSourceClock;
    ++mIterations;
  }

 private:
  void CalculateCorrection(int aBufferedFrames) {
    // We want to maintain 4 ms buffered
    int32_t bufferedFramesDiff = aBufferedFrames - mDesiredBuffering;
    int32_t resampledSourceClock = mSourceClock + bufferedFramesDiff;
    if (mTargetRate != mSourceRate) {
      resampledSourceClock =
          resampledSourceClock *
          (static_cast<float>(mTargetRate) / static_cast<float>(mSourceRate));
    }
    mCorrection = (float)mTargetClock / resampledSourceClock;

    // Clamp to ragnge [0.9, 1.1] to avoid distortion
    mCorrection = std::min(std::max(mCorrection, 0.9f), 1.1f);

    // If previous correction slightly smaller  (-1%) ignore it to avoid
    // recalculations. Don't do it when is greater (+1%) to avoid risking
    // running out of frames.
    if (mPreviousCorrection - mCorrection <= 0.01 &&
        mPreviousCorrection - mCorrection > 0) {
      mCorrection = mPreviousCorrection;
    }
    mPreviousCorrection = mCorrection;

    // Reset the counters to preper for the new period.
    mIterations = 0;
    mTargetClock = 0;
    mSourceClock = 0;
  }

  void BufferedFramesCorrection(int aBufferedFrames) {
    int32_t bufferedFramesDiff = aBufferedFrames - mDesiredBuffering;
    int32_t resampledSourceClock = mSourceRate + bufferedFramesDiff;
    if (mTargetRate != mSourceRate) {
      resampledSourceClock = resampledSourceClock *
                             (static_cast<float>(mTargetRate) / mSourceRate);
    }
    MOZ_ASSERT(mTargetRate > resampledSourceClock);
    mPreviousCorrection = mCorrection;
    mCorrection +=
        static_cast<float>(mTargetRate) / resampledSourceClock - 1.0f;
    // Clamp to range [0.9, 1.1] to avoid distortion
    mCorrection = std::min(std::max(mCorrection, 0.9f), 1.1f);
  }

 private:
  const int32_t mSourceRate;
  const int32_t mTargetRate;

  float mCorrection = 1.0;
  float mPreviousCorrection = 1.0;
  const int32_t mAdjustementWindow = 100;
  int32_t mDesiredBuffering;  // defult: 5ms

  int32_t mSourceClock = 0;
  int32_t mTargetClock = 0;
  int32_t mIterations = 0;
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
 public:
  AudioDriftCorrection(int32_t aSourceRate, int32_t aTargetRate)
      : mClockDrift(aSourceRate, aTargetRate),
        mResampler(aSourceRate, aTargetRate, aTargetRate / 20 /*50ms*/),
        mTargetRate(aTargetRate) {}

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
                             int32_t aOutputFrames) {
    // Very important to go first since the Dynamic will get the sample format
    // from the chunk.
    if (aInput.GetDuration()) {
      // Always go through the resampler because the clock might shift later.
      mResampler.AppendInput(aInput);
    }
    mClockDrift.UpdateClock(aInput.GetDuration(), aOutputFrames,
                            mResampler.InputDuration());
    TrackRate receivingRate = mTargetRate * mClockDrift.GetCorrection();
    // Update resampler's rate if there is a new correction.
    mResampler.UpdateOutRate(receivingRate);
    // If it does not have enough frames the result will be an empty segment.
    AudioSegment output = mResampler.Resample(aOutputFrames);
    if (output.IsEmpty()) {
      output.AppendNullData(aOutputFrames);
    }
    return output;
  }

 private:
  ClockDrift mClockDrift;
  AudioResampler mResampler;
  const int32_t mTargetRate;
};

};     // namespace mozilla
#endif /* MOZILLA_AUDIO_DRIFT_CORRECTION_H_ */
