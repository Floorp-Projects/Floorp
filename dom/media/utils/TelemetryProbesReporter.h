/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TelemetryProbesReporter_H_
#define DOM_TelemetryProbesReporter_H_

#include "MediaInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/AwakeTimeStamp.h"
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
    eVisible,
    eInvisible,
  };

  void OnPlay(Visibility aVisibility);
  void OnPause(Visibility aVisibility);
  void OnVisibilityChanged(Visibility aVisibility);
  void OnDecodeSuspended();
  void OnDecodeResumed();
  void OnShutdown();

  double GetTotalPlayTimeInSeconds() const;
  double GetInvisibleVideoPlayTimeInSeconds() const;
  double GetVideoDecodeSuspendedTimeInSeconds() const;

 private:
  void StartInvisibleVideoTimeAcculator();
  void PauseInvisibleVideoTimeAcculator();
  bool HasOwnerHadValidVideo() const;
  void AssertOnMainThreadAndNotShutdown() const;

  void ReportTelemetry();
  void ReportResultForVideo();
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

  // Total time an element has spent on playing.
  TimeDurationAccumulator mTotalPlayTime;

  // Total time a VIDEO element has spent playing while the corresponding media
  // element is invisible.
  TimeDurationAccumulator mInvisibleVideoPlayTime;

  // Total time a VIDEO has spent in video-decode-suspend mode.
  TimeDurationAccumulator mVideoDecodeSuspendedTime;

  Visibility mMediaElementVisibility = Visibility::eInvisible;
};

}  // namespace mozilla

#endif  // DOM_TelemetryProbesReporter_H_
