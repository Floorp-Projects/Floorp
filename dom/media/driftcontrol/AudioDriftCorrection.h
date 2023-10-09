/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_
#define DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_

#include "AudioSegment.h"
#include "TimeUnits.h"

namespace mozilla {

class AudioResampler;
class DriftController;

/**
 * Correct the drift between two independent clocks, the source, and the target
 * clock. The target clock is the master clock so the correction syncs the drift
 * of the source clock to the target. The nominal sampling rates of source and
 * target must be provided.
 *
 * It works with AudioSegment in order to be able to be used from the
 * MediaTrackGraph/MediaTrack. The audio buffers are pre-allocated so the only
 * new allocation taking place during operation happens if the input buffer
 * outgrows the memory allocated. The preallocation capacity is 100ms for input
 * and 100ms for output. The class consists of DriftController and
 * AudioResampler check there for more details.
 *
 * The class is not thread-safe. The construction can happen in any thread but
 * the member method must be used in a single thread that can be different than
 * the construction thread. Appropriate for being used in the high priority
 * audio thread.
 */
class AudioDriftCorrection final {
 public:
  AudioDriftCorrection(uint32_t aSourceRate, uint32_t aTargetRate,
                       const PrincipalHandle& aPrincipalHandle);

  ~AudioDriftCorrection();

  /**
   * A segment of input data (in the source rate) and a number of requested
   * output frames (in the target rate) are provided, and a segment (in the
   * target rate) of drift-corrected data is returned. The input is buffered
   * internally so some latency exists. The returned AudioSegment may not be
   * long-lived because any point in the internal buffer gets reused every
   * 100ms. If not enough data is available in the input buffer to produce
   * the requested number of output frames, the input buffer is drained and
   * a smaller segment than requested is returned.
   */
  AudioSegment RequestFrames(const AudioSegment& aInput,
                             uint32_t aOutputFrames);

  uint32_t CurrentBuffering() const;

  uint32_t BufferSize() const;

  uint32_t NumCorrectionChanges() const;

  uint32_t NumUnderruns() const { return mNumUnderruns; }

  void SetSourceLatency(media::TimeUnit aSourceLatency);

  const uint32_t mTargetRate;
  const media::TimeUnit mLatencyReductionTimeLimit =
      media::TimeUnit(15, 1).ToBase(mTargetRate);

 private:
  void SetDesiredBuffering(media::TimeUnit aDesiredBuffering);

  media::TimeUnit mSourceLatency = media::TimeUnit::Zero();
  media::TimeUnit mDesiredBuffering = media::TimeUnit::Zero();
  uint32_t mNumUnderruns = 0;
  bool mIsHandlingUnderrun = false;
  const UniquePtr<DriftController> mDriftController;
  const UniquePtr<AudioResampler> mResampler;
};
}  // namespace mozilla
#endif  // DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_
