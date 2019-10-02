/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DriftCompensation_h_
#define DriftCompensation_h_

#include "MediaSegment.h"
#include "VideoUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Unused.h"

namespace mozilla {

static LazyLogModule gDriftCompensatorLog("DriftCompensator");
#define LOG(type, ...) MOZ_LOG(gDriftCompensatorLog, type, (__VA_ARGS__))

/**
 * DriftCompensator can be used to handle drift between audio and video tracks
 * from the MediaStreamGraph.
 *
 * Drift can occur because audio is driven by a MediaStreamGraph running off an
 * audio callback, thus it's progressed by the clock of one the audio output
 * devices on the user's machine. Video on the other hand is always expressed in
 * wall-clock TimeStamps, i.e., it's progressed by the system clock. These
 * clocks will, over time, drift apart.
 *
 * Do not use the DriftCompensator across multiple audio tracks, as it will
 * automatically record the start time of the first audio samples, and all
 * samples for the same audio track on the same audio clock will have to be
 * processed to retain accuracy.
 *
 * DriftCompensator is designed to be used from two threads:
 * - The audio thread for notifications of audio samples.
 * - The video thread for compensating drift of video frames to match the audio
 *   clock.
 */
class DriftCompensator {
  const RefPtr<nsIEventTarget> mVideoThread;
  const TrackRate mAudioRate;

  // Number of audio samples produced. Any thread.
  Atomic<StreamTime> mAudioSamples{0};

  // Time the first audio samples were added. mVideoThread only.
  TimeStamp mAudioStartTime;

  void SetAudioStartTime(TimeStamp aTime) {
    MOZ_ASSERT(mVideoThread->IsOnCurrentThread());
    MOZ_ASSERT(mAudioStartTime.IsNull());
    mAudioStartTime = aTime;
  }

 protected:
  virtual ~DriftCompensator() = default;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DriftCompensator)

  DriftCompensator(RefPtr<nsIEventTarget> aVideoThread, TrackRate aAudioRate)
      : mVideoThread(std::move(aVideoThread)), mAudioRate(aAudioRate) {
    MOZ_ASSERT(mAudioRate > 0);
  }

  void NotifyAudioStart(TimeStamp aStart) {
    MOZ_ASSERT(mAudioSamples == 0);
    LOG(LogLevel::Info, "DriftCompensator %p at rate %d started", this,
        mAudioRate);
    nsresult rv = mVideoThread->Dispatch(NewRunnableMethod<TimeStamp>(
        "DriftCompensator::SetAudioStartTime", this,
        &DriftCompensator::SetAudioStartTime, aStart));
    MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
    Unused << rv;
  }

  /**
   * aSamples is the number of samples fed by an AudioStream.
   */
  void NotifyAudio(StreamTime aSamples) {
    MOZ_ASSERT(aSamples > 0);
    mAudioSamples += aSamples;

    LOG(LogLevel::Verbose,
        "DriftCompensator %p Processed another %" PRId64
        " samples; now %.3fs audio",
        this, aSamples, static_cast<double>(mAudioSamples) / mAudioRate);
  }

  /**
   * Drift compensates a video TimeStamp based on historical audio data.
   */
  virtual TimeStamp GetVideoTime(TimeStamp aNow, TimeStamp aTime) {
    MOZ_ASSERT(mVideoThread->IsOnCurrentThread());
    StreamTime samples = mAudioSamples;

    if (samples / mAudioRate < 10) {
      // We don't apply compensation for the first 10 seconds because of the
      // higher inaccuracy during this time.
      LOG(LogLevel::Debug, "DriftCompensator %p %" PRId64 "ms so far; ignoring",
          this, samples * 1000 / mAudioRate);
      return aTime;
    }

    if (aNow == mAudioStartTime) {
      LOG(LogLevel::Warning,
          "DriftCompensator %p video scale 0, assuming no drift", this);
      return aTime;
    }

    double videoScaleUs = (aNow - mAudioStartTime).ToMicroseconds();
    double audioScaleUs = FramesToUsecs(samples, mAudioRate).value();
    double videoDurationUs = (aTime - mAudioStartTime).ToMicroseconds();

    TimeStamp reclocked =
        mAudioStartTime + TimeDuration::FromMicroseconds(
                              videoDurationUs * audioScaleUs / videoScaleUs);

    LOG(LogLevel::Debug,
        "DriftCompensator %p GetVideoTime, v-now: %.3fs, a-now: %.3fs; %.3fs "
        "-> %.3fs (d %.3fms)",
        this, (aNow - mAudioStartTime).ToSeconds(),
        TimeDuration::FromMicroseconds(audioScaleUs).ToSeconds(),
        (aTime - mAudioStartTime).ToSeconds(),
        (reclocked - mAudioStartTime).ToSeconds(),
        (reclocked - aTime).ToMilliseconds());

    return reclocked;
  }
};

#undef LOG

}  // namespace mozilla

#endif /* DriftCompensation_h_ */
