/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryProbesReporter.h"

#include <cmath>

#include "FrameStatistics.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsThreadUtils.h"

namespace mozilla {

LazyLogModule gTelemetryProbesReporterLog("TelemetryProbesReporter");
#define LOG(msg, ...)                                   \
  MOZ_LOG(gTelemetryProbesReporterLog, LogLevel::Debug, \
          ("TelemetryProbesReporter=%p, " msg, this, ##__VA_ARGS__))

static const char* ToVisibilityStr(
    TelemetryProbesReporter::Visibility aVisibility) {
  switch (aVisibility) {
    case TelemetryProbesReporter::Visibility::eVisible:
      return "visible";
    case TelemetryProbesReporter::Visibility::eInvisible:
      return "invisible";
    default:
      MOZ_ASSERT_UNREACHABLE("invalid visibility");
      return "unknown";
  }
}

TelemetryProbesReporter::TelemetryProbesReporter(
    TelemetryProbesReporterOwner* aOwner)
    : mOwner(aOwner) {
  MOZ_ASSERT(mOwner);
}

void TelemetryProbesReporter::OnPlay(Visibility aVisibility) {
  AssertOnMainThreadAndNotShutdown();
  if (mTotalPlayTime.IsStarted()) {
    return;
  }
  LOG("Start time accumulation for total play time");
  mTotalPlayTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"moztotalplaytimestarted"_ns);
  if (aVisibility == Visibility::eInvisible) {
    StartInvisibleVideoTimeAcculator();
  }
}

void TelemetryProbesReporter::OnPause(Visibility aVisibility) {
  AssertOnMainThreadAndNotShutdown();
  if (!mTotalPlayTime.IsStarted()) {
    return;
  }
  if (aVisibility == Visibility::eInvisible) {
    PauseInvisibleVideoTimeAcculator();
  }
  LOG("Pause time accumulation for total play time");
  mTotalPlayTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"moztotalplaytimepaused"_ns);
  ReportTelemetry();
}

void TelemetryProbesReporter::OnVisibilityChanged(Visibility aVisibility) {
  AssertOnMainThreadAndNotShutdown();
  if (mMediaElementVisibility == aVisibility) {
    return;
  }

  mMediaElementVisibility = aVisibility;
  LOG("Corresponding media element visibility change=%s",
      ToVisibilityStr(aVisibility));
  if (mMediaElementVisibility == Visibility::eInvisible) {
    StartInvisibleVideoTimeAcculator();
  } else {
    PauseInvisibleVideoTimeAcculator();
  }
}

void TelemetryProbesReporter::OnDecodeSuspended() {
  AssertOnMainThreadAndNotShutdown();
  // Suspended time should only be counted after starting accumulating invisible
  // time.
  if (!mInvisibleVideoPlayTime.IsStarted()) {
    return;
  }
  LOG("Start time accumulation for video decoding suspension");
  mVideoDecodeSuspendedTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"mozvideodecodesuspendedstarted"_ns);
}

void TelemetryProbesReporter::OnDecodeResumed() {
  AssertOnMainThreadAndNotShutdown();
  if (!mVideoDecodeSuspendedTime.IsStarted()) {
    return;
  }
  LOG("Pause time accumulation for video decoding suspension");
  mVideoDecodeSuspendedTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"mozvideodecodesuspendedpaused"_ns);
}

void TelemetryProbesReporter::OnShutdown() {
  AssertOnMainThreadAndNotShutdown();
  LOG("Shutdown");
  OnPause(Visibility::eInvisible);
  mOwner = nullptr;
}

void TelemetryProbesReporter::StartInvisibleVideoTimeAcculator() {
  AssertOnMainThreadAndNotShutdown();
  if (!mTotalPlayTime.IsStarted() || mInvisibleVideoPlayTime.IsStarted() ||
      !HasOwnerHadValidVideo()) {
    return;
  }
  LOG("Start time accumulation for invisible video");
  mInvisibleVideoPlayTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"mozinvisibleplaytimestarted"_ns);
}

void TelemetryProbesReporter::PauseInvisibleVideoTimeAcculator() {
  AssertOnMainThreadAndNotShutdown();
  if (!mInvisibleVideoPlayTime.IsStarted()) {
    return;
  }
  OnDecodeResumed();
  LOG("Pause time accumulation for invisible video");
  mInvisibleVideoPlayTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"mozinvisibleplaytimepaused"_ns);
}

bool TelemetryProbesReporter::HasOwnerHadValidVideo() const {
  // Checking both image and display dimensions helps address cases such as
  // suspending, where we use a null decoder. In that case a null decoder
  // produces 0x0 video frames, which might cause layout to resize the display
  // size, but the image dimensions would be still non-null.
  const VideoInfo info = mOwner->GetMediaInfo().mVideo;
  return (info.mDisplay.height > 0 && info.mDisplay.width > 0) ||
         (info.mImage.height > 0 && info.mImage.width > 0);
}

void TelemetryProbesReporter::AssertOnMainThreadAndNotShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwner, "Already shutdown?");
}

void TelemetryProbesReporter::ReportTelemetry() {
  AssertOnMainThreadAndNotShutdown();
  ReportResultForVideo();
}

void TelemetryProbesReporter::ReportResultForVideo() {
  // We don't want to know the result for video without valid video frames.
  if (!HasOwnerHadValidVideo()) {
    return;
  }

  const double totalPlayTimeS = mTotalPlayTime.GetAndClearTotal();
  const double invisiblePlayTimeS = mInvisibleVideoPlayTime.GetAndClearTotal();
  const double videoDecodeSuspendTimeS =
      mVideoDecodeSuspendedTime.GetAndClearTotal();

  // No need to report result for video that didn't start playing.
  if (totalPlayTimeS == 0.0) {
    return;
  }
  MOZ_ASSERT(totalPlayTimeS >= invisiblePlayTimeS);

  LOG("VIDEO_PLAY_TIME_S = %f", totalPlayTimeS);
  Telemetry::Accumulate(Telemetry::VIDEO_PLAY_TIME_MS,
                        SECONDS_TO_MS(totalPlayTimeS));

  LOG("VIDEO_HIDDEN_PLAY_TIME_S = %f", invisiblePlayTimeS);
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_MS,
                        SECONDS_TO_MS(invisiblePlayTimeS));

  if (mOwner->IsEncrypted()) {
    LOG("VIDEO_ENCRYPTED_PLAY_TIME_S = %f", totalPlayTimeS);
    Telemetry::Accumulate(Telemetry::VIDEO_ENCRYPTED_PLAY_TIME_MS,
                          SECONDS_TO_MS(totalPlayTimeS));
  }

  // Report result for video using CDM
  auto keySystem = mOwner->GetKeySystem();
  if (keySystem) {
    if (IsClearkeyKeySystem(*keySystem)) {
      LOG("VIDEO_CLEARKEY_PLAY_TIME_S = %f", totalPlayTimeS);
      Telemetry::Accumulate(Telemetry::VIDEO_CLEARKEY_PLAY_TIME_MS,
                            SECONDS_TO_MS(totalPlayTimeS));

    } else if (IsWidevineKeySystem(*keySystem)) {
      LOG("VIDEO_WIDEVINE_PLAY_TIME_S = %f", totalPlayTimeS);
      Telemetry::Accumulate(Telemetry::VIDEO_WIDEVINE_PLAY_TIME_MS,
                            SECONDS_TO_MS(totalPlayTimeS));
    }
  }

  // Keyed by audio+video or video alone, and by a resolution range.
  const MediaInfo& info = mOwner->GetMediaInfo();
  nsCString key(info.HasAudio() ? "AV," : "V,");
  static const struct {
    int32_t mH;
    const char* mRes;
  } sResolutions[] = {{240, "0<h<=240"},     {480, "240<h<=480"},
                      {576, "480<h<=576"},   {720, "576<h<=720"},
                      {1080, "720<h<=1080"}, {2160, "1080<h<=2160"}};
  const char* resolution = "h>2160";
  int32_t height = info.mVideo.mImage.height;
  for (const auto& res : sResolutions) {
    if (height <= res.mH) {
      resolution = res.mRes;
      break;
    }
  }
  key.AppendASCII(resolution);

  auto visiblePlayTimeS = totalPlayTimeS - invisiblePlayTimeS;
  LOG("VIDEO_VISIBLE_PLAY_TIME = %f, keys: '%s' and 'All'", visiblePlayTimeS,
      key.get());
  Telemetry::Accumulate(Telemetry::VIDEO_VISIBLE_PLAY_TIME_MS, key,
                        SECONDS_TO_MS(visiblePlayTimeS));
  // Also accumulate result in an "All" key.
  Telemetry::Accumulate(Telemetry::VIDEO_VISIBLE_PLAY_TIME_MS, "All"_ns,
                        SECONDS_TO_MS(visiblePlayTimeS));

  const uint32_t hiddenPercentage =
      lround(invisiblePlayTimeS / totalPlayTimeS * 100.0);
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE, key,
                        hiddenPercentage);
  // Also accumulate all percentages in an "All" key.
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE, "All"_ns,
                        hiddenPercentage);
  LOG("VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE = %u, keys: '%s' and 'All'",
      hiddenPercentage, key.get());

  const uint32_t videoDecodeSuspendPercentage =
      lround(videoDecodeSuspendTimeS / totalPlayTimeS * 100.0);
  Telemetry::Accumulate(Telemetry::VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE,
                        key, videoDecodeSuspendPercentage);
  Telemetry::Accumulate(Telemetry::VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE,
                        "All"_ns, videoDecodeSuspendPercentage);
  LOG("VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE = %u, keys: '%s' and 'All'",
      videoDecodeSuspendPercentage, key.get());

  ReportResultForVideoFrameStatistics(totalPlayTimeS, key);
  mOwner->DispatchAsyncTestingEvent(u"mozreportedtelemetry"_ns);
}

void TelemetryProbesReporter::ReportResultForVideoFrameStatistics(
    double aTotalPlayTimeS, const nsCString& key) {
  FrameStatistics* stats = mOwner->GetFrameStatistics();
  if (!stats) {
    return;
  }

  FrameStatisticsData data = stats->GetFrameStatisticsData();
  if (data.mInterKeyframeCount != 0) {
    const uint32_t average_ms = uint32_t(
        std::min<uint64_t>(lround(double(data.mInterKeyframeSum_us) /
                                  double(data.mInterKeyframeCount) / 1000.0),
                           UINT32_MAX));
    Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_AVERAGE_MS, key,
                          average_ms);
    Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_AVERAGE_MS, "All"_ns,
                          average_ms);
    LOG("VIDEO_INTER_KEYFRAME_AVERAGE_MS = %u, keys: '%s' and 'All'",
        average_ms, key.get());

    const uint32_t max_ms = uint32_t(std::min<uint64_t>(
        (data.mInterKeyFrameMax_us + 500) / 1000, UINT32_MAX));
    Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_MAX_MS, key, max_ms);
    Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_MAX_MS, "All"_ns,
                          max_ms);
    LOG("VIDEO_INTER_KEYFRAME_MAX_MS = %u, keys: '%s' and 'All'", max_ms,
        key.get());
  } else {
    // Here, we have played *some* of the video, but didn't get more than 1
    // keyframe. Report '0' if we have played for longer than the video-
    // decode-suspend delay (showing recovery would be difficult).
    const uint32_t suspendDelay_ms =
        StaticPrefs::media_suspend_bkgnd_video_delay_ms();
    if (uint32_t(aTotalPlayTimeS * 1000.0) > suspendDelay_ms) {
      Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_MAX_MS, key, 0);
      Telemetry::Accumulate(Telemetry::VIDEO_INTER_KEYFRAME_MAX_MS, "All"_ns,
                            0);
      LOG("VIDEO_INTER_KEYFRAME_MAX_MS = 0 (only 1 keyframe), keys: '%s' and "
          "'All'",
          key.get());
    }
  }

  const uint64_t parsedFrames = stats->GetParsedFrames();
  if (parsedFrames) {
    const uint64_t droppedFrames = stats->GetDroppedFrames();
    MOZ_ASSERT(droppedFrames <= parsedFrames);
    // Dropped frames <= total frames, so 'percentage' cannot be higher than
    // 100 and therefore can fit in a uint32_t (that Telemetry takes).
    const uint32_t percentage = 100 * droppedFrames / parsedFrames;
    LOG("DROPPED_FRAMES_IN_VIDEO_PLAYBACK = %u", percentage);
    Telemetry::Accumulate(Telemetry::VIDEO_DROPPED_FRAMES_PROPORTION,
                          percentage);
  }
}

double TelemetryProbesReporter::GetTotalPlayTimeInSeconds() const {
  return mTotalPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetInvisibleVideoPlayTimeInSeconds() const {
  return mInvisibleVideoPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetVideoDecodeSuspendedTimeInSeconds() const {
  return mVideoDecodeSuspendedTime.PeekTotal();
}

#undef LOG
}  // namespace mozilla
