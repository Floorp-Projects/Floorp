/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_TelemetryProbesReporter_H_
#define DOM_TelemetryProbesReporter_H_

#include "MediaInfo.h"
#include "mozilla/Maybe.h"
#include "mozilla/TimeStamp.h"
#include "nsISupportsImpl.h"

namespace mozilla {
class FrameStatistics;

class TelemetryProbesReporterOwner {
 public:
  virtual Maybe<nsAutoString> GetKeySystem() const = 0;
  virtual MediaInfo GetMediaInfo() const = 0;
  virtual FrameStatistics* GetFrameStatistics() const = 0;
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
    TimeDurationAccumulator() : mCount(0) {}
    void Start() {
      if (IsStarted()) {
        return;
      }
      mStartTime = TimeStamp::Now();
    }
    void Pause() {
      if (!IsStarted()) {
        return;
      }
      mSum += (TimeStamp::Now() - mStartTime);
      mCount++;
      mStartTime = TimeStamp();
    }
    bool IsStarted() const { return !mStartTime.IsNull(); }
    double Total() const {
      if (!IsStarted()) {
        return mSum.ToSeconds();
      }
      // Add current running time until now, but keep it running.
      return (mSum + (TimeStamp::Now() - mStartTime)).ToSeconds();
    }
    uint32_t Count() const {
      if (!IsStarted()) {
        return mCount;
      }
      // Count current run in this report, without increasing the stored count.
      return mCount + 1;
    }
    void Reset() {
      mStartTime = TimeStamp();
      mSum = TimeDuration();
      mCount = 0;
    }

   private:
    TimeStamp mStartTime;
    TimeDuration mSum;
    uint32_t mCount;
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
