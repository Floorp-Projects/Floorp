/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelemetryProbesReporter.h"

#include <cmath>

#include "FrameStatistics.h"
#include "VideoUtils.h"
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
    case TelemetryProbesReporter::Visibility::eInitial:
      return "initial";
    default:
      MOZ_ASSERT_UNREACHABLE("invalid visibility");
      return "unknown";
  }
}
static const char* ToAudibilityStr(
    TelemetryProbesReporter::AudibleState aAudibleState) {
  switch (aAudibleState) {
    case TelemetryProbesReporter::AudibleState::eAudible:
      return "audible";
    case TelemetryProbesReporter::AudibleState::eNotAudible:
      return "inaudible";
    default:
      MOZ_ASSERT_UNREACHABLE("invalid audibility");
      return "unknown";
  }
}

static const char* ToMutedStr(bool aMuted) {
  return aMuted ? "muted" : "unmuted";
}

MediaContent TelemetryProbesReporter::MediaInfoToMediaContent(
    const MediaInfo& aInfo) {
  MediaContent content = MediaContent::MEDIA_HAS_NOTHING;
  if (aInfo.HasAudio()) {
    content |= MediaContent::MEDIA_HAS_AUDIO;
  }
  if (aInfo.HasVideo()) {
    content |= MediaContent::MEDIA_HAS_VIDEO;
    if (aInfo.mVideo.GetAsVideoInfo()->mColorDepth > gfx::ColorDepth::COLOR_8) {
      content |= MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8;
    }
  }
  return content;
}

TelemetryProbesReporter::TelemetryProbesReporter(
    TelemetryProbesReporterOwner* aOwner)
    : mOwner(aOwner) {
  MOZ_ASSERT(mOwner);
}

void TelemetryProbesReporter::OnPlay(Visibility aVisibility,
                                     MediaContent aMediaContent,
                                     bool aIsMuted) {
  LOG("Start time accumulation for total play time");

  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_VIDEO,
                !mTotalVideoPlayTime.IsStarted());
  MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_AUDIO,
                !mTotalAudioPlayTime.IsStarted());

  if (aMediaContent & MediaContent::MEDIA_HAS_VIDEO) {
    mTotalVideoPlayTime.Start();

    MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8,
                  !mTotalVideoHDRPlayTime.IsStarted());
    if (aMediaContent & MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8) {
      mTotalVideoHDRPlayTime.Start();
    }
  }
  if (aMediaContent & MediaContent::MEDIA_HAS_AUDIO) {
    mTotalAudioPlayTime.Start();
  }

  OnMediaContentChanged(aMediaContent);
  OnVisibilityChanged(aVisibility);
  OnMutedChanged(aIsMuted);

  mOwner->DispatchAsyncTestingEvent(u"moztotalplaytimestarted"_ns);

  mIsPlaying = true;
}

void TelemetryProbesReporter::OnPause(Visibility aVisibility) {
  if (!mIsPlaying) {
    // Not started
    LOG("TelemetryProbesReporter::OnPause: not started, early return");
    return;
  }

  LOG("Pause time accumulation for total play time");

  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_VIDEO,
                mTotalVideoPlayTime.IsStarted());
  MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_AUDIO,
                mTotalAudioPlayTime.IsStarted());

  if (mMediaContent & MediaContent::MEDIA_HAS_VIDEO) {
    MOZ_ASSERT_IF(mMediaContent & MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8,
                  mTotalVideoHDRPlayTime.IsStarted());

    LOG("Pause video time accumulation for total play time");
    if (mInvisibleVideoPlayTime.IsStarted()) {
      LOG("Pause invisible video time accumulation for total play time");
      PauseInvisibleVideoTimeAccumulator();
    }
    mTotalVideoPlayTime.Pause();
    mTotalVideoHDRPlayTime.Pause();
  }
  if (mMediaContent & MediaContent::MEDIA_HAS_AUDIO) {
    LOG("Pause audio time accumulation for total play time");
    if (mInaudibleAudioPlayTime.IsStarted()) {
      LOG("Pause audible audio time accumulation for total play time");
      PauseInaudibleAudioTimeAccumulator();
    }
    if (mMutedAudioPlayTime.IsStarted()) {
      LOG("Pause muted audio time accumulation for total play time");
      PauseMutedAudioTimeAccumulator();
    }
    mTotalAudioPlayTime.Pause();
  }

  mOwner->DispatchAsyncTestingEvent(u"moztotalplaytimepaused"_ns);
  ReportTelemetry();

  mIsPlaying = false;
}

void TelemetryProbesReporter::OnVisibilityChanged(Visibility aVisibility) {
  AssertOnMainThreadAndNotShutdown();
  LOG("Corresponding media element visibility change=%s -> %s",
      ToVisibilityStr(mMediaElementVisibility), ToVisibilityStr(aVisibility));
  if (aVisibility == Visibility::eInvisible) {
    StartInvisibleVideoTimeAccumulator();
  } else {
    if (aVisibility != Visibility::eInitial) {
      PauseInvisibleVideoTimeAccumulator();
    } else {
      LOG("Visibility was initial, not pausing.");
    }
  }
  mMediaElementVisibility = aVisibility;
}

void TelemetryProbesReporter::OnAudibleChanged(AudibleState aAudibleState) {
  AssertOnMainThreadAndNotShutdown();
  LOG("Audibility changed, now %s", ToAudibilityStr(aAudibleState));
  if (aAudibleState == AudibleState::eNotAudible) {
    if (!mInaudibleAudioPlayTime.IsStarted()) {
      StartInaudibleAudioTimeAccumulator();
    }
  } else {
    // This happens when starting playback, no need to pause, because it hasn't
    // been started yet.
    if (mInaudibleAudioPlayTime.IsStarted()) {
      PauseInaudibleAudioTimeAccumulator();
    }
  }
}

void TelemetryProbesReporter::OnMutedChanged(bool aMuted) {
  // There are multiple ways to mute an element:
  // - volume = 0
  // - muted = true
  // - set the enabled property of the playing AudioTrack to false
  // Muted -> Muted "transisition" can therefore happen, and we can't add
  // asserts here.
  AssertOnMainThreadAndNotShutdown();
  if (!(mMediaContent & MediaContent::MEDIA_HAS_AUDIO)) {
    return;
  }
  LOG("Muted changed, was %s now %s", ToMutedStr(mIsMuted), ToMutedStr(aMuted));
  if (aMuted) {
    if (!mMutedAudioPlayTime.IsStarted()) {
      StartMutedAudioTimeAccumulator();
    }
  } else {
    // This happens when starting playback, no need to pause, because it hasn't
    // been started yet.
    if (mMutedAudioPlayTime.IsStarted()) {
      PauseMutedAudioTimeAccumulator();
    }
  }
  mIsMuted = aMuted;
}

void TelemetryProbesReporter::OnMediaContentChanged(MediaContent aContent) {
  AssertOnMainThreadAndNotShutdown();
  if (aContent == mMediaContent) {
    return;
  }
  if (mMediaContent & MediaContent::MEDIA_HAS_VIDEO &&
      !(aContent & MediaContent::MEDIA_HAS_VIDEO)) {
    LOG("Video track removed from media.");
    if (mInvisibleVideoPlayTime.IsStarted()) {
      PauseInvisibleVideoTimeAccumulator();
    }
    if (mTotalVideoPlayTime.IsStarted()) {
      mTotalVideoPlayTime.Pause();
      mTotalVideoHDRPlayTime.Pause();
    }
  }
  if (mMediaContent & MediaContent::MEDIA_HAS_AUDIO &&
      !(aContent & MediaContent::MEDIA_HAS_AUDIO)) {
    LOG("Audio track removed from media.");
    if (mTotalAudioPlayTime.IsStarted()) {
      mTotalAudioPlayTime.Pause();
    }
    if (mInaudibleAudioPlayTime.IsStarted()) {
      mInaudibleAudioPlayTime.Pause();
    }
    if (mMutedAudioPlayTime.IsStarted()) {
      mMutedAudioPlayTime.Pause();
    }
  }
  if (!(mMediaContent & MediaContent::MEDIA_HAS_VIDEO) &&
      aContent & MediaContent::MEDIA_HAS_VIDEO) {
    LOG("Video track added to media.");
    if (mIsPlaying) {
      mTotalVideoPlayTime.Start();
      if (mMediaElementVisibility == Visibility::eInvisible) {
        StartInvisibleVideoTimeAccumulator();
      }
    }
  }
  if (!(mMediaContent & MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8) &&
      aContent & MediaContent::MEDIA_HAS_COLOR_DEPTH_ABOVE_8) {
    if (mIsPlaying) {
      mTotalVideoHDRPlayTime.Start();
    }
  }
  if (!(mMediaContent & MediaContent::MEDIA_HAS_AUDIO) &&
      aContent & MediaContent::MEDIA_HAS_AUDIO) {
    LOG("Audio track added to media.");
    if (mIsPlaying) {
      mTotalAudioPlayTime.Start();
      if (mIsMuted) {
        StartMutedAudioTimeAccumulator();
      }
    }
  }

  mMediaContent = aContent;
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

void TelemetryProbesReporter::StartInvisibleVideoTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  if (!mTotalVideoPlayTime.IsStarted() || mInvisibleVideoPlayTime.IsStarted() ||
      !HasOwnerHadValidVideo()) {
    return;
  }
  LOG("Start time accumulation for invisible video");
  mInvisibleVideoPlayTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"mozinvisibleplaytimestarted"_ns);
}

void TelemetryProbesReporter::PauseInvisibleVideoTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  if (!mInvisibleVideoPlayTime.IsStarted()) {
    return;
  }
  OnDecodeResumed();
  LOG("Pause time accumulation for invisible video");
  mInvisibleVideoPlayTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"mozinvisibleplaytimepaused"_ns);
}

void TelemetryProbesReporter::StartInaudibleAudioTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT(!mInaudibleAudioPlayTime.IsStarted());
  mInaudibleAudioPlayTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"mozinaudibleaudioplaytimestarted"_ns);
}

void TelemetryProbesReporter::PauseInaudibleAudioTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT(mInaudibleAudioPlayTime.IsStarted());
  mInaudibleAudioPlayTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"mozinaudibleaudioplaytimepaused"_ns);
}

void TelemetryProbesReporter::StartMutedAudioTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT(!mMutedAudioPlayTime.IsStarted());
  mMutedAudioPlayTime.Start();
  mOwner->DispatchAsyncTestingEvent(u"mozmutedaudioplaytimestarted"_ns);
}

void TelemetryProbesReporter::PauseMutedAudioTimeAccumulator() {
  AssertOnMainThreadAndNotShutdown();
  MOZ_ASSERT(mMutedAudioPlayTime.IsStarted());
  mMutedAudioPlayTime.Pause();
  mOwner->DispatchAsyncTestingEvent(u"mozmutedeaudioplaytimepaused"_ns);
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

bool TelemetryProbesReporter::HasOwnerHadValidMedia() const {
  return mMediaContent != MediaContent::MEDIA_HAS_NOTHING;
}

void TelemetryProbesReporter::AssertOnMainThreadAndNotShutdown() const {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mOwner, "Already shutdown?");
}

void TelemetryProbesReporter::ReportTelemetry() {
  AssertOnMainThreadAndNotShutdown();
  // ReportResultForAudio needs to be called first, because it can use the video
  // play time, that is reset in ReportResultForVideo.
  ReportResultForAudio();
  ReportResultForVideo();
  mOwner->DispatchAsyncTestingEvent(u"mozreportedtelemetry"_ns);
}

void TelemetryProbesReporter::ReportResultForVideo() {
  // We don't want to know the result for video without valid video frames.
  if (!HasOwnerHadValidVideo()) {
    return;
  }

  const double totalVideoPlayTimeS = mTotalVideoPlayTime.GetAndClearTotal();
  const double invisiblePlayTimeS = mInvisibleVideoPlayTime.GetAndClearTotal();
  const double videoDecodeSuspendTimeS =
      mVideoDecodeSuspendedTime.GetAndClearTotal();
  const double totalVideoHDRPlayTimeS =
      mTotalVideoHDRPlayTime.GetAndClearTotal();

  // No need to report result for video that didn't start playing.
  if (totalVideoPlayTimeS == 0.0) {
    return;
  }
  MOZ_ASSERT(totalVideoPlayTimeS >= invisiblePlayTimeS);

  LOG("VIDEO_PLAY_TIME_S = %f", totalVideoPlayTimeS);
  Telemetry::Accumulate(Telemetry::VIDEO_PLAY_TIME_MS,
                        SECONDS_TO_MS(totalVideoPlayTimeS));

  LOG("VIDEO_HIDDEN_PLAY_TIME_S = %f", invisiblePlayTimeS);
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_MS,
                        SECONDS_TO_MS(invisiblePlayTimeS));

  // We only want to accumulate non-zero samples for HDR playback.
  // This is different from the other timings tracked here, but
  // we don't need 0-length play times to do our calculations.
  if (totalVideoHDRPlayTimeS > 0.0) {
    LOG("VIDEO_HDR_PLAY_TIME_S = %f", totalVideoHDRPlayTimeS);
    Telemetry::Accumulate(Telemetry::VIDEO_HDR_PLAY_TIME_MS,
                          SECONDS_TO_MS(totalVideoHDRPlayTimeS));
  }

  if (mOwner->IsEncrypted()) {
    LOG("VIDEO_ENCRYPTED_PLAY_TIME_S = %f", totalVideoPlayTimeS);
    Telemetry::Accumulate(Telemetry::VIDEO_ENCRYPTED_PLAY_TIME_MS,
                          SECONDS_TO_MS(totalVideoPlayTimeS));
  }

  // Report result for video using CDM
  auto keySystem = mOwner->GetKeySystem();
  if (keySystem) {
    if (IsClearkeyKeySystem(*keySystem)) {
      LOG("VIDEO_CLEARKEY_PLAY_TIME_S = %f", totalVideoPlayTimeS);
      Telemetry::Accumulate(Telemetry::VIDEO_CLEARKEY_PLAY_TIME_MS,
                            SECONDS_TO_MS(totalVideoPlayTimeS));

    } else if (IsWidevineKeySystem(*keySystem)) {
      LOG("VIDEO_WIDEVINE_PLAY_TIME_S = %f", totalVideoPlayTimeS);
      Telemetry::Accumulate(Telemetry::VIDEO_WIDEVINE_PLAY_TIME_MS,
                            SECONDS_TO_MS(totalVideoPlayTimeS));
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

  auto visiblePlayTimeS = totalVideoPlayTimeS - invisiblePlayTimeS;
  LOG("VIDEO_VISIBLE_PLAY_TIME = %f, keys: '%s' and 'All'", visiblePlayTimeS,
      key.get());
  Telemetry::Accumulate(Telemetry::VIDEO_VISIBLE_PLAY_TIME_MS, key,
                        SECONDS_TO_MS(visiblePlayTimeS));
  // Also accumulate result in an "All" key.
  Telemetry::Accumulate(Telemetry::VIDEO_VISIBLE_PLAY_TIME_MS, "All"_ns,
                        SECONDS_TO_MS(visiblePlayTimeS));

  const uint32_t hiddenPercentage =
      lround(invisiblePlayTimeS / totalVideoPlayTimeS * 100.0);
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE, key,
                        hiddenPercentage);
  // Also accumulate all percentages in an "All" key.
  Telemetry::Accumulate(Telemetry::VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE, "All"_ns,
                        hiddenPercentage);
  LOG("VIDEO_HIDDEN_PLAY_TIME_PERCENTAGE = %u, keys: '%s' and 'All'",
      hiddenPercentage, key.get());

  const uint32_t videoDecodeSuspendPercentage =
      lround(videoDecodeSuspendTimeS / totalVideoPlayTimeS * 100.0);
  Telemetry::Accumulate(Telemetry::VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE,
                        key, videoDecodeSuspendPercentage);
  Telemetry::Accumulate(Telemetry::VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE,
                        "All"_ns, videoDecodeSuspendPercentage);
  LOG("VIDEO_INFERRED_DECODE_SUSPEND_PERCENTAGE = %u, keys: '%s' and 'All'",
      videoDecodeSuspendPercentage, key.get());

  ReportResultForVideoFrameStatistics(totalVideoPlayTimeS, key);
}

void TelemetryProbesReporter::ReportResultForAudio() {
  // Don't record telemetry for a media that didn't have a valid audio or video
  // to play, or hasn't played.
  if (!HasOwnerHadValidMedia() || (mTotalAudioPlayTime.PeekTotal() == 0.0 &&
                                   mTotalVideoPlayTime.PeekTotal() == 0.0)) {
    return;
  }

  nsCString key;
  nsCString avKey;
  const double totalAudioPlayTimeS = mTotalAudioPlayTime.GetAndClearTotal();
  const double inaudiblePlayTimeS = mInaudibleAudioPlayTime.GetAndClearTotal();
  const double mutedPlayTimeS = mMutedAudioPlayTime.GetAndClearTotal();
  const double audiblePlayTimeS = totalAudioPlayTimeS - inaudiblePlayTimeS;
  const double unmutedPlayTimeS = totalAudioPlayTimeS - mutedPlayTimeS;
  const uint32_t audiblePercentage =
      lround(audiblePlayTimeS / totalAudioPlayTimeS * 100.0);
  const uint32_t unmutedPercentage =
      lround(unmutedPlayTimeS / totalAudioPlayTimeS * 100.0);
  const double totalVideoPlayTimeS = mTotalVideoPlayTime.PeekTotal();

  // Key semantics:
  // - AV: Audible audio + video
  // - IV: Inaudible audio + video
  // - MV: Muted audio + video
  // - A: Audible audio-only
  // - I: Inaudible audio-only
  // - M: Muted audio-only
  // - V: Video-only
  if (mMediaContent & MediaContent::MEDIA_HAS_AUDIO) {
    if (audiblePercentage == 0) {
      // Media element had an audio track, but it was inaudible throughout
      key.AppendASCII("I");
    } else if (unmutedPercentage == 0) {
      // Media element had an audio track, but it was muted throughout
      key.AppendASCII("M");
    } else {
      // Media element had an audible audio track
      key.AppendASCII("A");
    }
    avKey.AppendASCII("A");
  }
  if (mMediaContent & MediaContent::MEDIA_HAS_VIDEO) {
    key.AppendASCII("V");
    avKey.AppendASCII("V");
  }

  LOG("Key: %s", key.get());

  if (mMediaContent & MediaContent::MEDIA_HAS_AUDIO) {
    LOG("Audio:\ntotal: %lf\naudible: %lf\ninaudible: %lf\nmuted: "
        "%lf\npercentage audible: "
        "%u\npercentage unmuted: %u\n",
        totalAudioPlayTimeS, audiblePlayTimeS, inaudiblePlayTimeS,
        mutedPlayTimeS, audiblePercentage, unmutedPercentage);
    Telemetry::Accumulate(Telemetry::MEDIA_PLAY_TIME_MS, key,
                          SECONDS_TO_MS(totalAudioPlayTimeS));
    Telemetry::Accumulate(Telemetry::MUTED_PLAY_TIME_PERCENT, avKey,
                          100 - unmutedPercentage);
    Telemetry::Accumulate(Telemetry::AUDIBLE_PLAY_TIME_PERCENT, avKey,
                          audiblePercentage);
  } else {
    MOZ_ASSERT(mMediaContent & MediaContent::MEDIA_HAS_VIDEO);
    Telemetry::Accumulate(Telemetry::MEDIA_PLAY_TIME_MS, key,
                          SECONDS_TO_MS(totalVideoPlayTimeS));
  }
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
        StaticPrefs::media_suspend_background_video_delay_ms();
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
    const uint32_t proportion = 10000 * droppedFrames / parsedFrames;
    Telemetry::Accumulate(
        Telemetry::VIDEO_DROPPED_FRAMES_PROPORTION_EXPONENTIAL, proportion);

    {
      const uint64_t droppedFrames = stats->GetDroppedDecodedFrames();
      const uint32_t proportion = 10000 * droppedFrames / parsedFrames;
      Telemetry::Accumulate(
          Telemetry::VIDEO_DROPPED_DECODED_FRAMES_PROPORTION_EXPONENTIAL,
          proportion);
    }
    {
      const uint64_t droppedFrames = stats->GetDroppedSinkFrames();
      const uint32_t proportion = 10000 * droppedFrames / parsedFrames;
      Telemetry::Accumulate(
          Telemetry::VIDEO_DROPPED_SINK_FRAMES_PROPORTION_EXPONENTIAL,
          proportion);
    }
    {
      const uint64_t droppedFrames = stats->GetDroppedCompositorFrames();
      const uint32_t proportion = 10000 * droppedFrames / parsedFrames;
      Telemetry::Accumulate(
          Telemetry::VIDEO_DROPPED_COMPOSITOR_FRAMES_PROPORTION_EXPONENTIAL,
          proportion);
    }
  }
}

double TelemetryProbesReporter::GetTotalVideoPlayTimeInSeconds() const {
  return mTotalVideoPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetTotalVideoHDRPlayTimeInSeconds() const {
  return mTotalVideoHDRPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetVisibleVideoPlayTimeInSeconds() const {
  return GetTotalVideoPlayTimeInSeconds() -
         GetInvisibleVideoPlayTimeInSeconds();
}

double TelemetryProbesReporter::GetInvisibleVideoPlayTimeInSeconds() const {
  return mInvisibleVideoPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetVideoDecodeSuspendedTimeInSeconds() const {
  return mVideoDecodeSuspendedTime.PeekTotal();
}

double TelemetryProbesReporter::GetTotalAudioPlayTimeInSeconds() const {
  return mTotalAudioPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetInaudiblePlayTimeInSeconds() const {
  return mInaudibleAudioPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetMutedPlayTimeInSeconds() const {
  return mMutedAudioPlayTime.PeekTotal();
}

double TelemetryProbesReporter::GetAudiblePlayTimeInSeconds() const {
  return GetTotalAudioPlayTimeInSeconds() - GetInaudiblePlayTimeInSeconds();
}

#undef LOG
}  // namespace mozilla
