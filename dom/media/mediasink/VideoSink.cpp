/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef XP_WIN
// Include Windows headers required for enabling high precision timers.
#  include "windows.h"
#  include "mmsystem.h"
#endif

#include "VideoSink.h"

#ifdef MOZ_GECKO_PROFILER
#  include "ProfileJSONWriter.h"
#  include "ProfilerMarkerPayload.h"
#endif
#include "MediaQueue.h"
#include "VideoUtils.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/StaticPrefs.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#undef FMT

#define FMT(x, ...) "VideoSink=%p " x, this, ##__VA_ARGS__
#define VSINK_LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, (FMT(x, ##__VA_ARGS__)))
#define VSINK_LOG_V(x, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (FMT(x, ##__VA_ARGS__)))

#ifdef MOZ_GECKO_PROFILER
#  define VSINK_ADD_PROFILER_MARKER(tag, markerTime, aTime, vTime)          \
    do {                                                                    \
      if (profiler_thread_is_being_profiled()) {                            \
        profiler_add_marker(                                                \
            tag, JS::ProfilingCategoryPair::GRAPHICS,                       \
            MakeUnique<VideoFrameMarkerPayload>(markerTime, aTime, vTime)); \
      }                                                                     \
    } while (0)

class VideoFrameMarkerPayload : public ProfilerMarkerPayload {
 public:
  explicit VideoFrameMarkerPayload(mozilla::TimeStamp aMarkerTime,
                                   int64_t aAudioPositionUs,
                                   int64_t aVideoFrameTimeUs)
      : ProfilerMarkerPayload(aMarkerTime, aMarkerTime),
        mAudioPositionUs(aAudioPositionUs),
        mVideoFrameTimeUs(aVideoFrameTimeUs) {}

  void StreamPayload(SpliceableJSONWriter& aWriter,
                     const TimeStamp& aProcessStartTime,
                     UniqueStacks& aUniqueStacks) {
    StreamCommonProps("UpdateRenderVideoFrames", aWriter, aProcessStartTime,
                      aUniqueStacks);
    aWriter.IntProperty("audio", mAudioPositionUs);
    aWriter.IntProperty("video", mVideoFrameTimeUs);
  }

 private:
  int64_t mAudioPositionUs;
  int64_t mVideoFrameTimeUs;
};
#else
#  define VSINK_ADD_PROFILER_MARKER(tag, markerTime, aTime, vTime)
#endif

using namespace mozilla::layers;

// Minimum update frequency is 1/120th of a second, i.e. half the
// duration of a 60-fps frame.
static const int64_t MIN_UPDATE_INTERVAL_US = 1000000 / (60 * 2);

static void SetImageToGreenPixel(PlanarYCbCrImage* aImage) {
  static uint8_t greenPixel[] = {0x00, 0x00, 0x00};
  PlanarYCbCrData data;
  data.mYChannel = greenPixel;
  data.mCbChannel = greenPixel + 1;
  data.mCrChannel = greenPixel + 2;
  data.mYStride = data.mCbCrStride = 1;
  data.mPicSize = data.mYSize = data.mCbCrSize = gfx::IntSize(1, 1);
  aImage->CopyData(data);
}

VideoSink::VideoSink(AbstractThread* aThread, MediaSink* aAudioSink,
                     MediaQueue<VideoData>& aVideoQueue,
                     VideoFrameContainer* aContainer,
                     FrameStatistics& aFrameStats,
                     uint32_t aVQueueSentToCompositerSize)
    : mOwnerThread(aThread),
      mAudioSink(aAudioSink),
      mVideoQueue(aVideoQueue),
      mContainer(aContainer),
      mProducerID(ImageContainer::AllocateProducerID()),
      mFrameStats(aFrameStats),
      mOldCompositorDroppedCount(mContainer ? mContainer->GetDroppedImageCount()
                                            : 0),
      mPendingDroppedCount(0),
      mHasVideo(false),
      mUpdateScheduler(aThread),
      mVideoQueueSendToCompositorSize(aVQueueSentToCompositerSize),
      mMinVideoQueueSize(StaticPrefs::MediaRuinAvSyncEnabled() ? 1 : 0)
#ifdef XP_WIN
      ,
      mHiResTimersRequested(false)
#endif
{
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  if (StaticPrefs::browser_measurement_render_anims_and_video_solid() &&
      mContainer) {
    InitializeBlankImage();
    MOZ_ASSERT(mBlankImage, "Blank image should exist.");
  }
}

VideoSink::~VideoSink() {
#ifdef XP_WIN
  MOZ_ASSERT(!mHiResTimersRequested);
#endif
}

const MediaSink::PlaybackParams& VideoSink::GetPlaybackParams() const {
  AssertOwnerThread();

  return mAudioSink->GetPlaybackParams();
}

void VideoSink::SetPlaybackParams(const PlaybackParams& aParams) {
  AssertOwnerThread();

  mAudioSink->SetPlaybackParams(aParams);
}

RefPtr<VideoSink::EndedPromise> VideoSink::OnEnded(TrackType aType) {
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->OnEnded(aType);
  } else if (aType == TrackInfo::kVideoTrack) {
    return mEndPromise;
  }
  return nullptr;
}

TimeUnit VideoSink::GetEndTime(TrackType aType) const {
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kVideoTrack) {
    return mVideoFrameEndTime;
  } else if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->GetEndTime(aType);
  }
  return TimeUnit::Zero();
}

TimeUnit VideoSink::GetPosition(TimeStamp* aTimeStamp) const {
  AssertOwnerThread();
  return mAudioSink->GetPosition(aTimeStamp);
}

bool VideoSink::HasUnplayedFrames(TrackType aType) const {
  AssertOwnerThread();
  MOZ_ASSERT(aType == TrackInfo::kAudioTrack,
             "Not implemented for non audio tracks.");

  return mAudioSink->HasUnplayedFrames(aType);
}

void VideoSink::SetPlaybackRate(double aPlaybackRate) {
  AssertOwnerThread();

  mAudioSink->SetPlaybackRate(aPlaybackRate);
}

void VideoSink::SetVolume(double aVolume) {
  AssertOwnerThread();

  mAudioSink->SetVolume(aVolume);
}

void VideoSink::SetPreservesPitch(bool aPreservesPitch) {
  AssertOwnerThread();

  mAudioSink->SetPreservesPitch(aPreservesPitch);
}

void VideoSink::EnsureHighResTimersOnOnlyIfPlaying() {
#ifdef XP_WIN
  const bool needed = IsPlaying();
  if (needed == mHiResTimersRequested) {
    return;
  }
  if (needed) {
    // Ensure high precision timers are enabled on Windows, otherwise the
    // VideoSink isn't woken up at reliable intervals to set the next frame, and
    // we drop frames while painting. Note that each call must be matched by a
    // corresponding timeEndPeriod() call. Enabling high precision timers causes
    // the CPU to wake up more frequently on Windows 7 and earlier, which causes
    // more CPU load and battery use. So we only enable high precision timers
    // when we're actually playing.
    timeBeginPeriod(1);
  } else {
    timeEndPeriod(1);
  }
  mHiResTimersRequested = needed;
#endif
}

void VideoSink::SetPlaying(bool aPlaying) {
  AssertOwnerThread();
  VSINK_LOG_V(" playing (%d) -> (%d)", mAudioSink->IsPlaying(), aPlaying);

  if (!aPlaying) {
    // Reset any update timer if paused.
    mUpdateScheduler.Reset();
    // Since playback is paused, tell compositor to render only current frame.
    RenderVideoFrames(1);
    if (mContainer) {
      mContainer->ClearCachedResources();
    }
    if (mSecondaryContainer) {
      mSecondaryContainer->ClearCachedResources();
    }
  }

  mAudioSink->SetPlaying(aPlaying);

  if (mHasVideo && aPlaying) {
    // There's no thread in VideoSink for pulling video frames, need to trigger
    // rendering while becoming playing status. because the VideoQueue may be
    // full already.
    TryUpdateRenderedVideoFrames();
  }

  EnsureHighResTimersOnOnlyIfPlaying();
}

nsresult VideoSink::Start(const TimeUnit& aStartTime, const MediaInfo& aInfo) {
  AssertOwnerThread();
  VSINK_LOG("[%s]", __func__);

  nsresult rv = mAudioSink->Start(aStartTime, aInfo);

  mHasVideo = aInfo.HasVideo();

  if (mHasVideo) {
    mEndPromise = mEndPromiseHolder.Ensure(__func__);

    // If the underlying MediaSink has an end promise for the video track (which
    // happens when mAudioSink refers to a DecodedStream), we must wait for it
    // to complete before resolving our own end promise. Otherwise, MDSM might
    // stop playback before DecodedStream plays to the end and cause
    // test_streams_element_capture.html to time out.
    RefPtr<EndedPromise> p = mAudioSink->OnEnded(TrackInfo::kVideoTrack);
    if (p) {
      RefPtr<VideoSink> self = this;
      p->Then(
           mOwnerThread, __func__,
           [self]() {
             self->mVideoSinkEndRequest.Complete();
             self->TryUpdateRenderedVideoFrames();
             // It is possible the video queue size is 0 and we have no
             // frames to render. However, we need to call
             // MaybeResolveEndPromise() to ensure mEndPromiseHolder is
             // resolved.
             self->MaybeResolveEndPromise();
           },
           [self]() {
             self->mVideoSinkEndRequest.Complete();
             self->TryUpdateRenderedVideoFrames();
             self->MaybeResolveEndPromise();
           })
          ->Track(mVideoSinkEndRequest);
    }

    ConnectListener();
    // Run the render loop at least once so we can resolve the end promise
    // when video duration is 0.
    UpdateRenderedVideoFrames();
  }
  return rv;
}

void VideoSink::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "playback not started.");
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Stop();

  mUpdateScheduler.Reset();
  if (mHasVideo) {
    DisconnectListener();
    mVideoSinkEndRequest.DisconnectIfExists();
    mEndPromiseHolder.ResolveIfExists(true, __func__);
    mEndPromise = nullptr;
  }
  mVideoFrameEndTime = TimeUnit::Zero();

  EnsureHighResTimersOnOnlyIfPlaying();
}

bool VideoSink::IsStarted() const {
  AssertOwnerThread();

  return mAudioSink->IsStarted();
}

bool VideoSink::IsPlaying() const {
  AssertOwnerThread();

  return mAudioSink->IsPlaying();
}

void VideoSink::Shutdown() {
  AssertOwnerThread();
  MOZ_ASSERT(!mAudioSink->IsStarted(), "must be called after playback stops.");
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Shutdown();
}

void VideoSink::OnVideoQueuePushed(RefPtr<VideoData>&& aSample) {
  AssertOwnerThread();
  // Listen to push event, VideoSink should try rendering ASAP if first frame
  // arrives but update scheduler is not triggered yet.
  if (!aSample->IsSentToCompositor()) {
    // Since we push rendered frames back to the queue, we will receive
    // push events for them. We only need to trigger render loop
    // when this frame is not rendered yet.
    TryUpdateRenderedVideoFrames();
  }
}

void VideoSink::OnVideoQueueFinished() {
  AssertOwnerThread();
  // Run render loop if the end promise is not resolved yet.
  if (!mUpdateScheduler.IsScheduled() && mAudioSink->IsPlaying() &&
      !mEndPromiseHolder.IsEmpty()) {
    UpdateRenderedVideoFrames();
  }
}

void VideoSink::Redraw(const VideoInfo& aInfo) {
  AssertOwnerThread();

  // No video track, nothing to draw.
  if (!aInfo.IsValid() || !mContainer) {
    return;
  }

  auto now = TimeStamp::Now();

  RefPtr<VideoData> video = VideoQueue().PeekFront();
  if (video) {
    if (mBlankImage) {
      video->mImage = mBlankImage;
    }
    video->MarkSentToCompositor();
    mContainer->SetCurrentFrame(video->mDisplay, video->mImage, now);
    if (mSecondaryContainer) {
      mSecondaryContainer->SetCurrentFrame(video->mDisplay, video->mImage, now);
    }
    return;
  }

  // When we reach here, it means there are no frames in this video track.
  // Draw a blank frame to ensure there is something in the image container
  // to fire 'loadeddata'.

  RefPtr<Image> blank =
      mContainer->GetImageContainer()->CreatePlanarYCbCrImage();
  mContainer->SetCurrentFrame(aInfo.mDisplay, blank, now);

  if (mSecondaryContainer) {
    mSecondaryContainer->SetCurrentFrame(aInfo.mDisplay, blank, now);
  }
}

void VideoSink::TryUpdateRenderedVideoFrames() {
  AssertOwnerThread();
  if (mUpdateScheduler.IsScheduled() || !mAudioSink->IsPlaying()) {
    return;
  }
  RefPtr<VideoData> v = VideoQueue().PeekFront();
  if (!v) {
    // No frames to render.
    return;
  }

  TimeStamp nowTime;
  const TimeUnit clockTime = mAudioSink->GetPosition(&nowTime);
  if (clockTime >= v->mTime) {
    // Time to render this frame.
    UpdateRenderedVideoFrames();
    return;
  }

  // If we send this future frame to the compositor now, it will be rendered
  // immediately and break A/V sync. Instead, we schedule a timer to send it
  // later.
  int64_t delta = (v->mTime - clockTime).ToMicroseconds() /
                  mAudioSink->GetPlaybackParams().mPlaybackRate;
  TimeStamp target = nowTime + TimeDuration::FromMicroseconds(delta);
  RefPtr<VideoSink> self = this;
  mUpdateScheduler.Ensure(
      target, [self]() { self->UpdateRenderedVideoFramesByTimer(); },
      [self]() { self->UpdateRenderedVideoFramesByTimer(); });
}

void VideoSink::UpdateRenderedVideoFramesByTimer() {
  AssertOwnerThread();
  mUpdateScheduler.CompleteRequest();
  UpdateRenderedVideoFrames();
}

void VideoSink::ConnectListener() {
  AssertOwnerThread();
  mPushListener = VideoQueue().PushEvent().Connect(
      mOwnerThread, this, &VideoSink::OnVideoQueuePushed);
  mFinishListener = VideoQueue().FinishEvent().Connect(
      mOwnerThread, this, &VideoSink::OnVideoQueueFinished);
}

void VideoSink::DisconnectListener() {
  AssertOwnerThread();
  mPushListener.Disconnect();
  mFinishListener.Disconnect();
}

void VideoSink::RenderVideoFrames(int32_t aMaxFrames, int64_t aClockTime,
                                  const TimeStamp& aClockTimeStamp) {
  AssertOwnerThread();

  AutoTArray<RefPtr<VideoData>, 16> frames;
  VideoQueue().GetFirstElements(aMaxFrames, &frames);
  if (frames.IsEmpty() || !mContainer) {
    return;
  }

  AutoTArray<ImageContainer::NonOwningImage, 16> images;
  TimeStamp lastFrameTime;
  MediaSink::PlaybackParams params = mAudioSink->GetPlaybackParams();
  for (uint32_t i = 0; i < frames.Length(); ++i) {
    VideoData* frame = frames[i];
    bool wasSent = frame->IsSentToCompositor();
    frame->MarkSentToCompositor();

    if (!frame->mImage || !frame->mImage->IsValid() ||
        !frame->mImage->GetSize().width || !frame->mImage->GetSize().height) {
      continue;
    }

    if (frame->mTime.IsNegative()) {
      // Frame times before the start time are invalid; drop such frames
      continue;
    }

    TimeStamp t;
    if (aMaxFrames > 1) {
      MOZ_ASSERT(!aClockTimeStamp.IsNull());
      int64_t delta = frame->mTime.ToMicroseconds() - aClockTime;
      t = aClockTimeStamp +
          TimeDuration::FromMicroseconds(delta / params.mPlaybackRate);
      if (!lastFrameTime.IsNull() && t <= lastFrameTime) {
        // Timestamps out of order; drop the new frame. In theory we should
        // probably replace the previous frame with the new frame if the
        // timestamps are equal, but this is a corrupt video file already so
        // never mind.
        continue;
      }
      lastFrameTime = t;
    }

    ImageContainer::NonOwningImage* img = images.AppendElement();
    img->mTimeStamp = t;
    img->mImage = frame->mImage;
    if (mBlankImage) {
      img->mImage = mBlankImage;
    }
    img->mFrameID = frame->mFrameID;
    img->mProducerID = mProducerID;

    VSINK_LOG_V("playing video frame %" PRId64 " (id=%x) (vq-queued=%zu)",
                frame->mTime.ToMicroseconds(), frame->mFrameID,
                VideoQueue().GetSize());
    if (!wasSent) {
      VSINK_ADD_PROFILER_MARKER("VideoSink: play", aClockTimeStamp, aClockTime,
                                frame->mTime.ToMicroseconds());
    }
  }

  if (images.Length() > 0) {
    mContainer->SetCurrentFrames(frames[0]->mDisplay, images);

    if (mSecondaryContainer) {
      mSecondaryContainer->SetCurrentFrames(frames[0]->mDisplay, images);
    }
  }
}

void VideoSink::UpdateRenderedVideoFrames() {
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsPlaying(), "should be called while playing.");

  // Get the current playback position.
  TimeStamp nowTime;
  const auto clockTime = mAudioSink->GetPosition(&nowTime);
  MOZ_ASSERT(!clockTime.IsNegative(), "Should have positive clock time.");

  uint32_t sentToCompositorCount = 0;
  uint32_t droppedInSink = 0;

  // Skip frames up to the playback position.
  TimeUnit lastFrameEndTime;
  while (VideoQueue().GetSize() > mMinVideoQueueSize &&
         clockTime >= VideoQueue().PeekFront()->GetEndTime()) {
    RefPtr<VideoData> frame = VideoQueue().PopFront();
    lastFrameEndTime = frame->GetEndTime();
    if (frame->IsSentToCompositor()) {
      sentToCompositorCount++;
    } else {
      droppedInSink++;
      VSINK_LOG_V("discarding video frame mTime=%" PRId64
                  " clock_time=%" PRId64,
                  frame->mTime.ToMicroseconds(), clockTime.ToMicroseconds());
      VSINK_ADD_PROFILER_MARKER("VideoSink: discard", nowTime,
                                clockTime.ToMicroseconds(),
                                frame->mTime.ToMicroseconds());
    }
  }

  if (droppedInSink || sentToCompositorCount) {
    uint32_t totalCompositorDroppedCount = mContainer->GetDroppedImageCount();
    uint32_t droppedInCompositor =
        totalCompositorDroppedCount - mOldCompositorDroppedCount;
    if (droppedInCompositor > 0) {
      mOldCompositorDroppedCount = totalCompositorDroppedCount;
      VSINK_LOG_V("%u video frame previously discarded by compositor",
                  droppedInCompositor);
    }
    mPendingDroppedCount += droppedInCompositor;
    uint32_t droppedReported = mPendingDroppedCount > sentToCompositorCount
                                   ? sentToCompositorCount
                                   : mPendingDroppedCount;
    mPendingDroppedCount -= droppedReported;

    mFrameStats.Accumulate({0, 0, sentToCompositorCount - droppedReported, 0,
                            droppedInSink, droppedInCompositor});
  }

  // The presentation end time of the last video frame displayed is either
  // the end time of the current frame, or if we dropped all frames in the
  // queue, the end time of the last frame we removed from the queue.
  RefPtr<VideoData> currentFrame = VideoQueue().PeekFront();
  mVideoFrameEndTime =
      std::max(mVideoFrameEndTime,
               currentFrame ? currentFrame->GetEndTime() : lastFrameEndTime);

  RenderVideoFrames(mVideoQueueSendToCompositorSize, clockTime.ToMicroseconds(),
                    nowTime);

  MaybeResolveEndPromise();

  // Get the timestamp of the next frame. Schedule the next update at
  // the start time of the next frame. If we don't have a next frame,
  // we will run render loops again upon incoming frames.
  nsTArray<RefPtr<VideoData>> frames;
  VideoQueue().GetFirstElements(2, &frames);
  if (frames.Length() < 2) {
    return;
  }

  int64_t nextFrameTime = frames[1]->mTime.ToMicroseconds();
  int64_t delta = std::max(nextFrameTime - clockTime.ToMicroseconds(),
                           MIN_UPDATE_INTERVAL_US);
  TimeStamp target =
      nowTime + TimeDuration::FromMicroseconds(
                    delta / mAudioSink->GetPlaybackParams().mPlaybackRate);

  RefPtr<VideoSink> self = this;
  mUpdateScheduler.Ensure(
      target, [self]() { self->UpdateRenderedVideoFramesByTimer(); },
      [self]() { self->UpdateRenderedVideoFramesByTimer(); });
}

void VideoSink::MaybeResolveEndPromise() {
  AssertOwnerThread();
  // All frames are rendered, Let's resolve the promise.
  if (VideoQueue().IsFinished() && VideoQueue().GetSize() <= 1 &&
      !mVideoSinkEndRequest.Exists()) {
    if (VideoQueue().GetSize() == 1) {
      // Remove the last frame since we have sent it to compositor.
      RefPtr<VideoData> frame = VideoQueue().PopFront();
      if (mPendingDroppedCount > 0) {
        mFrameStats.Accumulate({0, 0, 0, 0, 0, 1});
        mPendingDroppedCount--;
      } else {
        mFrameStats.NotifyPresentedFrame();
      }
    }

    TimeStamp nowTime;
    const auto clockTime = mAudioSink->GetPosition(&nowTime);
    if (clockTime < mVideoFrameEndTime) {
      VSINK_LOG_V(
          "Not reach video end time yet, reschedule timer to resolve "
          "end promise. clockTime=%" PRId64 ", endTime=%" PRId64,
          clockTime.ToMicroseconds(), mVideoFrameEndTime.ToMicroseconds());
      int64_t delta = (mVideoFrameEndTime - clockTime).ToMicroseconds() /
                      mAudioSink->GetPlaybackParams().mPlaybackRate;
      TimeStamp target = nowTime + TimeDuration::FromMicroseconds(delta);
      auto resolveEndPromise = [self = RefPtr<VideoSink>(this)]() {
        self->mEndPromiseHolder.ResolveIfExists(true, __func__);
        self->mUpdateScheduler.CompleteRequest();
      };
      mUpdateScheduler.Ensure(target, std::move(resolveEndPromise),
                              std::move(resolveEndPromise));
    } else {
      mEndPromiseHolder.ResolveIfExists(true, __func__);
    }
  }
}

void VideoSink::SetSecondaryVideoContainer(VideoFrameContainer* aSecondary) {
  AssertOwnerThread();
  mSecondaryContainer = aSecondary;
  if (!IsPlaying() && mSecondaryContainer) {
    ImageContainer* mainImageContainer = mContainer->GetImageContainer();
    ImageContainer* secondaryImageContainer =
        mSecondaryContainer->GetImageContainer();
    MOZ_DIAGNOSTIC_ASSERT(mainImageContainer);
    MOZ_DIAGNOSTIC_ASSERT(secondaryImageContainer);

    // If the video isn't currently playing, get the most recently
    // decoded frame and display that in the secondary container as
    // well.
    nsTArray<ImageContainer::OwningImage> oldImages;
    mainImageContainer->GetCurrentImages(&oldImages);
    if (oldImages.Length()) {
      ImageContainer::OwningImage& old = oldImages.LastElement();

      nsTArray<ImageContainer::NonOwningImage> currentFrame;
      // We hardcode this first frame to 0 so that we ensure that subsequent
      // frames always have a greater frameID, which is an ImageContainer
      // invariant.
      currentFrame.AppendElement(ImageContainer::NonOwningImage(
          old.mImage, old.mTimeStamp, /* frameID */ 0, old.mProducerID));

      secondaryImageContainer->SetCurrentImages(currentFrame);
    }
  }
}

void VideoSink::ClearSecondaryVideoContainer() {
  AssertOwnerThread();
  mSecondaryContainer = nullptr;
}

nsCString VideoSink::GetDebugInfo() {
  AssertOwnerThread();
  auto str = nsPrintfCString(
      "VideoSink: IsStarted=%d IsPlaying=%d VideoQueue(finished=%d "
      "size=%zu) mVideoFrameEndTime=%" PRId64
      " mHasVideo=%d "
      "mVideoSinkEndRequest.Exists()=%d mEndPromiseHolder.IsEmpty()=%d",
      IsStarted(), IsPlaying(), VideoQueue().IsFinished(),
      VideoQueue().GetSize(), mVideoFrameEndTime.ToMicroseconds(), mHasVideo,
      mVideoSinkEndRequest.Exists(), mEndPromiseHolder.IsEmpty());
  AppendStringIfNotEmpty(str, mAudioSink->GetDebugInfo());
  return std::move(str);
}

bool VideoSink::InitializeBlankImage() {
  mBlankImage = mContainer->GetImageContainer()->CreatePlanarYCbCrImage();
  if (mBlankImage == nullptr) {
    return false;
  }
  SetImageToGreenPixel(mBlankImage->AsPlanarYCbCrImage());
  return true;
}
}  // namespace mozilla
