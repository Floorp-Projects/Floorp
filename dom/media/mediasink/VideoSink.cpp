/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaQueue.h"
#include "VideoSink.h"
#include "MediaPrefs.h"

#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#undef FMT

#define FMT(x, ...) "VideoSink=%p " x, this, ##__VA_ARGS__
#define VSINK_LOG(x, ...)   MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(x, ##__VA_ARGS__)))
#define VSINK_LOG_V(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (FMT(x, ##__VA_ARGS__)))

using namespace mozilla::layers;

namespace media {

// Minimum update frequency is 1/120th of a second, i.e. half the
// duration of a 60-fps frame.
static const int64_t MIN_UPDATE_INTERVAL_US = 1000000 / (60 * 2);

VideoSink::VideoSink(AbstractThread* aThread,
                     MediaSink* aAudioSink,
                     MediaQueue<VideoData>& aVideoQueue,
                     VideoFrameContainer* aContainer,
                     FrameStatistics& aFrameStats,
                     uint32_t aVQueueSentToCompositerSize)
  : mOwnerThread(aThread)
  , mAudioSink(aAudioSink)
  , mVideoQueue(aVideoQueue)
  , mContainer(aContainer)
  , mProducerID(ImageContainer::AllocateProducerID())
  , mFrameStats(aFrameStats)
  , mHasVideo(false)
  , mUpdateScheduler(aThread)
  , mVideoQueueSendToCompositorSize(aVQueueSentToCompositerSize)
  , mMinVideoQueueSize(MediaPrefs::RuinAvSync() ? 1 : 0)
{
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
}

VideoSink::~VideoSink()
{
}

const MediaSink::PlaybackParams&
VideoSink::GetPlaybackParams() const
{
  AssertOwnerThread();

  return mAudioSink->GetPlaybackParams();
}

void
VideoSink::SetPlaybackParams(const PlaybackParams& aParams)
{
  AssertOwnerThread();

  mAudioSink->SetPlaybackParams(aParams);
}

RefPtr<GenericPromise>
VideoSink::OnEnded(TrackType aType)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->OnEnded(aType);
  } else if (aType == TrackInfo::kVideoTrack) {
    return mEndPromise;
  }
  return nullptr;
}

TimeUnit
VideoSink::GetEndTime(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kVideoTrack) {
    return mVideoFrameEndTime;
  } else if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->GetEndTime(aType);
  }
  return TimeUnit::Zero();
}

TimeUnit
VideoSink::GetPosition(TimeStamp* aTimeStamp) const
{
  AssertOwnerThread();
  return mAudioSink->GetPosition(aTimeStamp);
}

bool
VideoSink::HasUnplayedFrames(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(aType == TrackInfo::kAudioTrack, "Not implemented for non audio tracks.");

  return mAudioSink->HasUnplayedFrames(aType);
}

void
VideoSink::SetPlaybackRate(double aPlaybackRate)
{
  AssertOwnerThread();

  mAudioSink->SetPlaybackRate(aPlaybackRate);
}

void
VideoSink::SetVolume(double aVolume)
{
  AssertOwnerThread();

  mAudioSink->SetVolume(aVolume);
}

void
VideoSink::SetPreservesPitch(bool aPreservesPitch)
{
  AssertOwnerThread();

  mAudioSink->SetPreservesPitch(aPreservesPitch);
}

void
VideoSink::SetPlaying(bool aPlaying)
{
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
  }

  mAudioSink->SetPlaying(aPlaying);

  if (mHasVideo && aPlaying) {
    // There's no thread in VideoSink for pulling video frames, need to trigger
    // rendering while becoming playing status. because the VideoQueue may be
    // full already.
    TryUpdateRenderedVideoFrames();
  }
}

void
VideoSink::Start(const TimeUnit& aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Start(aStartTime, aInfo);

  mHasVideo = aInfo.HasVideo();

  if (mHasVideo) {
    mEndPromise = mEndPromiseHolder.Ensure(__func__);

    // If the underlying MediaSink has an end promise for the video track (which
    // happens when mAudioSink refers to a DecodedStream), we must wait for it
    // to complete before resolving our own end promise. Otherwise, MDSM might
    // stop playback before DecodedStream plays to the end and cause
    // test_streams_element_capture.html to time out.
    RefPtr<GenericPromise> p = mAudioSink->OnEnded(TrackInfo::kVideoTrack);
    if (p) {
      RefPtr<VideoSink> self = this;
      p->Then(mOwnerThread, __func__,
        [self] () {
          self->mVideoSinkEndRequest.Complete();
          self->TryUpdateRenderedVideoFrames();
          // It is possible the video queue size is 0 and we have no frames to
          // render. However, we need to call MaybeResolveEndPromise() to ensure
          // mEndPromiseHolder is resolved.
          self->MaybeResolveEndPromise();
        }, [self] () {
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
}

void
VideoSink::Stop()
{
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
}

bool
VideoSink::IsStarted() const
{
  AssertOwnerThread();

  return mAudioSink->IsStarted();
}

bool
VideoSink::IsPlaying() const
{
  AssertOwnerThread();

  return mAudioSink->IsPlaying();
}

void
VideoSink::Shutdown()
{
  AssertOwnerThread();
  MOZ_ASSERT(!mAudioSink->IsStarted(), "must be called after playback stops.");
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Shutdown();
}

void
VideoSink::OnVideoQueuePushed(RefPtr<VideoData>&& aSample)
{
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

void
VideoSink::OnVideoQueueFinished()
{
  AssertOwnerThread();
  // Run render loop if the end promise is not resolved yet.
  if (!mUpdateScheduler.IsScheduled() &&
      mAudioSink->IsPlaying() &&
      !mEndPromiseHolder.IsEmpty()) {
    UpdateRenderedVideoFrames();
  }
}

void
VideoSink::Redraw(const VideoInfo& aInfo)
{
  AssertOwnerThread();

  // No video track, nothing to draw.
  if (!aInfo.IsValid() || !mContainer) {
    return;
  }

  RefPtr<VideoData> video = VideoQueue().PeekFront();
  if (video) {
    video->MarkSentToCompositor();
    mContainer->SetCurrentFrame(video->mDisplay, video->mImage, TimeStamp::Now());
    return;
  }

  // When we reach here, it means there are no frames in this video track.
  // Draw a blank frame to ensure there is something in the image container
  // to fire 'loadeddata'.
  RefPtr<Image> blank =
    mContainer->GetImageContainer()->CreatePlanarYCbCrImage();
  mContainer->SetCurrentFrame(aInfo.mDisplay, blank, TimeStamp::Now());
}

void
VideoSink::TryUpdateRenderedVideoFrames()
{
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
    target,
    [self]() { self->UpdateRenderedVideoFramesByTimer(); },
    [self]() { self->UpdateRenderedVideoFramesByTimer(); });
}

void
VideoSink::UpdateRenderedVideoFramesByTimer()
{
  AssertOwnerThread();
  mUpdateScheduler.CompleteRequest();
  UpdateRenderedVideoFrames();
}

void
VideoSink::ConnectListener()
{
  AssertOwnerThread();
  mPushListener = VideoQueue().PushEvent().Connect(
    mOwnerThread, this, &VideoSink::OnVideoQueuePushed);
  mFinishListener = VideoQueue().FinishEvent().Connect(
    mOwnerThread, this, &VideoSink::OnVideoQueueFinished);
}

void
VideoSink::DisconnectListener()
{
  AssertOwnerThread();
  mPushListener.Disconnect();
  mFinishListener.Disconnect();
}

void
VideoSink::RenderVideoFrames(int32_t aMaxFrames,
                             int64_t aClockTime,
                             const TimeStamp& aClockTimeStamp)
{
  AssertOwnerThread();

  AutoTArray<RefPtr<VideoData>,16> frames;
  VideoQueue().GetFirstElements(aMaxFrames, &frames);
  if (frames.IsEmpty() || !mContainer) {
    return;
  }

  AutoTArray<ImageContainer::NonOwningImage,16> images;
  TimeStamp lastFrameTime;
  MediaSink::PlaybackParams params = mAudioSink->GetPlaybackParams();
  for (uint32_t i = 0; i < frames.Length(); ++i) {
    VideoData* frame = frames[i];

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
    img->mFrameID = frame->mFrameID;
    img->mProducerID = mProducerID;

    VSINK_LOG_V("playing video frame %" PRId64 " (id=%x) (vq-queued=%zu)",
                frame->mTime.ToMicroseconds(), frame->mFrameID,
                VideoQueue().GetSize());
  }

  if (images.Length() > 0) {
    mContainer->SetCurrentFrames(frames[0]->mDisplay, images);
  }
}

void
VideoSink::UpdateRenderedVideoFrames()
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsPlaying(), "should be called while playing.");

  // Get the current playback position.
  TimeStamp nowTime;
  const auto clockTime = mAudioSink->GetPosition(&nowTime);
  MOZ_ASSERT(!clockTime.IsNegative(), "Should have positive clock time.");

  // Skip frames up to the playback position.
  TimeUnit lastFrameEndTime;
  while (VideoQueue().GetSize() > mMinVideoQueueSize &&
         clockTime >= VideoQueue().PeekFront()->GetEndTime()) {
    RefPtr<VideoData> frame = VideoQueue().PopFront();
    lastFrameEndTime = frame->GetEndTime();
    if (frame->IsSentToCompositor()) {
      mFrameStats.NotifyPresentedFrame();
    } else {
      mFrameStats.NotifyDecodedFrames({ 0, 0, 1 });
      VSINK_LOG_V("discarding video frame mTime=%" PRId64 " clock_time=%" PRId64,
                  frame->mTime.ToMicroseconds(), clockTime.ToMicroseconds());
    }
  }

  // The presentation end time of the last video frame displayed is either
  // the end time of the current frame, or if we dropped all frames in the
  // queue, the end time of the last frame we removed from the queue.
  RefPtr<VideoData> currentFrame = VideoQueue().PeekFront();
  mVideoFrameEndTime = std::max(mVideoFrameEndTime,
    currentFrame ? currentFrame->GetEndTime() : lastFrameEndTime);

  RenderVideoFrames(
    mVideoQueueSendToCompositorSize,
    clockTime.ToMicroseconds(), nowTime);

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
  int64_t delta = std::max(
    nextFrameTime - clockTime.ToMicroseconds(), MIN_UPDATE_INTERVAL_US);
  TimeStamp target = nowTime + TimeDuration::FromMicroseconds(
     delta / mAudioSink->GetPlaybackParams().mPlaybackRate);

  RefPtr<VideoSink> self = this;
  mUpdateScheduler.Ensure(target, [self] () {
    self->UpdateRenderedVideoFramesByTimer();
  }, [self] () {
    self->UpdateRenderedVideoFramesByTimer();
  });
}

void
VideoSink::MaybeResolveEndPromise()
{
  AssertOwnerThread();
  // All frames are rendered, Let's resolve the promise.
  if (VideoQueue().IsFinished() &&
      VideoQueue().GetSize() <= 1 &&
      !mVideoSinkEndRequest.Exists()) {
    if (VideoQueue().GetSize() == 1) {
      // Remove the last frame since we have sent it to compositor.
      RefPtr<VideoData> frame = VideoQueue().PopFront();
      mFrameStats.NotifyPresentedFrame();
    }
    mEndPromiseHolder.ResolveIfExists(true, __func__);
  }
}

nsCString
VideoSink::GetDebugInfo()
{
  AssertOwnerThread();
  return nsPrintfCString(
    "VideoSink Status: IsStarted=%d IsPlaying=%d VideoQueue(finished=%d "
    "size=%zu) mVideoFrameEndTime=%" PRId64 " mHasVideo=%d "
    "mVideoSinkEndRequest.Exists()=%d mEndPromiseHolder.IsEmpty()=%d\n",
    IsStarted(), IsPlaying(), VideoQueue().IsFinished(),
    VideoQueue().GetSize(), mVideoFrameEndTime.ToMicroseconds(), mHasVideo,
    mVideoSinkEndRequest.Exists(), mEndPromiseHolder.IsEmpty())
    + mAudioSink->GetDebugInfo();
}

} // namespace media
} // namespace mozilla
