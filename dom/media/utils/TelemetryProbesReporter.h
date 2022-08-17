/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TelemetryProbesReporter_H_
#define DOM_TelemetryProbesReporter_H_

#include "MediaInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/AwakeTimeStamp.h"
#include "AudioChannelService.h"
#include "nsISupportsImpl.h"

namespace mozilla {
class FrameStatistics;

class TelemetryProbesReporterOwner {
 public:
  virtual Maybe<nsAutoString> GetKeySystem() const = 0;
  virtual MediaInfo GetMediaInfo() const = 0;
  virtual FrameStatistics* GetFrameStatistics() const = 0;
  virtual bool IsEncrypted() const = 0;
  virtual void DispatchAsyncTestingEvent(const nsAString& aName) = 0;
};

enum class MediaContent : uint8_t {
  MEDIA_HAS_NOTHING = (0 << 0),
  MEDIA_HAS_VIDEO = (1 << 0),
  MEDIA_HAS_AUDIO = (1 << 1),
  MEDIA_HAS_COLOR_DEPTH_ABOVE_8 = (1 << 2),
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MediaContent)

/**
 * This class is used for collecting and reporting telemetry probes for
 * its owner which should inherit from TelemetryProbesReporterOwner. We use it
 * for HTMLMediaElement, and each element has one corresponding reporter.
 */
class TelemetryProbesReporter final {
 public:
  explicit TelemetryProbesReporter(TelemetryProbesReporterOwner* aOwner);
  ~TelemetryProbesReporter() = default;

  enum class Visibility {
    eInitial,
    eVisible,
    eInvisible,
  };

  static MediaContent MediaInfoToMediaContent(const MediaInfo& aInfo);

  using AudibleState = dom::AudioChannelService::AudibleState;

  // State transitions
  void OnPlay(Visibility aVisibility, MediaContent aContent, bool aIsMuted);
  void OnPause(Visibility aVisibility);
  void OnShutdown();

  void OnVisibilityChanged(Visibility aVisibility);
  void OnAudibleChanged(AudibleState aAudible);
  void OnMediaContentChanged(MediaContent aContent);
  void OnMutedChanged(bool aMuted);
  void OnDecodeSuspended();
  void OnDecodeResumed();

  double GetTotalVideoPlayTimeInSeconds() const;
  double GetTotalVideoHDRPlayTimeInSeconds() const;
  double GetVisibleVideoPlayTimeInSeconds() const;
  double GetInvisibleVideoPlayTimeInSeconds() const;
  double GetVideoDecodeSuspendedTimeInSeconds() const;

  double GetTotalAudioPlayTimeInSeconds() const;
  double GetInaudiblePlayTimeInSeconds() const;
  double GetAudiblePlayTimeInSeconds() const;
  double GetMutedPlayTimeInSeconds() const;

 private:
  void StartInvisibleVideoTimeAccumulator();
  void PauseInvisibleVideoTimeAccumulator();
  void StartInaudibleAudioTimeAccumulator();
  void PauseInaudibleAudioTimeAccumulator();
  void StartMutedAudioTimeAccumulator();
  void PauseMutedAudioTimeAccumulator();
  bool HasOwnerHadValidVideo() const;
  bool HasOwnerHadValidMedia() const;
  void AssertOnMainThreadAndNotShutdown() const;

  void ReportTelemetry();
  void ReportResultForVideo();
  void ReportResultForAudio();
  void ReportResultForVideoFrameStatistics(double aTotalPlayTimeS,
                                           const nsCString& key);

  // Helper class to measure times for playback telemetry stats
  class TimeDurationAccumulator {
   public:
    TimeDurationAccumulator() = default;
    void Start() {
      if (IsStarted()) {
        return;
      }
      mStartTime = Some(AwakeTimeStamp::NowLoRes());
    }
    void Pause() {
      if (!IsStarted()) {
        return;
      }
      mSum = (AwakeTimeStamp::NowLoRes() - mStartTime.value());
      mStartTime = Nothing();
    }
    bool IsStarted() const { return mStartTime.isSome(); }

    double GetAndClearTotal() {
      MOZ_ASSERT(!IsStarted(), "only call this when accumulator is paused");
      double total = mSum.ToSeconds();
      mStartTime = Nothing();
      mSum = AwakeTimeDuration();
      return total;
    }

    double PeekTotal() const {
      if (!IsStarted()) {
        return mSum.ToSeconds();
      }
      return (AwakeTimeStamp::NowLoRes() - mStartTime.value()).ToSeconds();
    }

   private:
    Maybe<AwakeTimeStamp> mStartTime;
    AwakeTimeDuration mSum;
  };

  // The owner is HTMLMediaElement that is guaranteed being always alive during
  // our whole life cycle.
  TelemetryProbesReporterOwner* mOwner;

  // Total time an element has spent on playing video.
  TimeDurationAccumulator mTotalVideoPlayTime;

  // Total time an element has spent on playing video that has a color depth
  // greater than 8, which is likely HDR video.
  TimeDurationAccumulator mTotalVideoHDRPlayTime;

  // Total time an element has spent on playing audio
  TimeDurationAccumulator mTotalAudioPlayTime;

  // Total time a VIDEO element has spent playing while the corresponding media
  // element is invisible.
  TimeDurationAccumulator mInvisibleVideoPlayTime;

  // Total time an element has spent playing audio that was not audible
  TimeDurationAccumulator mInaudibleAudioPlayTime;

  // Total time an element with an audio track has spent muted
  TimeDurationAccumulator mMutedAudioPlayTime;

  // Total time a VIDEO has spent in video-decode-suspend mode.
  TimeDurationAccumulator mVideoDecodeSuspendedTime;

  Visibility mMediaElementVisibility = Visibility::eInitial;

  MediaContent mMediaContent = MediaContent::MEDIA_HAS_NOTHING;

  bool mIsPlaying = false;

  bool mIsMuted = false;
};

}  // namespace mozilla

#endif  // DOM_TelemetryProbesReporter_H_
