/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_
#define DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_

#include "AudioSegment.h"

namespace mozilla {

class AudioResampler;
class ClockDrift;

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
                       const PrincipalHandle& aPrincipalHandle);

  ~AudioDriftCorrection();

  /**
   * The source audio frames and request the number of target audio frames must
   * be provided. The duration of the source and the output is considered as the
   * source clock and the target clock. The input is buffered internally so some
   * latency exists. The returned AudioSegment must be cleaned up because the
   * internal buffer will be reused after 100ms. If not enough data is available
   * in the input buffer to produce the requested number of output frames, the
   * input buffer is drained and a smaller segment than requested is returned.
   * Not thread-safe.
   */
  AudioSegment RequestFrames(const AudioSegment& aInput,
                             uint32_t aOutputFrames);

  // Only accessible from the same thread that is driving RequestFrames().
  uint32_t CurrentBuffering() const;

  // Only accessible from the same thread that is driving RequestFrames().
  uint32_t NumCorrectionChanges() const;

  const uint32_t mDesiredBuffering;
  const uint32_t mTargetRate;

 private:
  const UniquePtr<ClockDrift> mClockDrift;
  const UniquePtr<AudioResampler> mResampler;
};

}  // namespace mozilla
#endif  // DOM_MEDIA_DRIFTCONTROL_AUDIODRIFTCORRECTION_H_
