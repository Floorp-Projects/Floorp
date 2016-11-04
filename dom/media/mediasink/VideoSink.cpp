/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaQueue.h"
#include "VideoSink.h"
#include "MediaPrefs.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;

#undef FMT
#undef DUMP_LOG

#define FMT(x, ...) "VideoSink=%p " x, this, ##__VA_ARGS__
#define VSINK_LOG(x, ...)   MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(x, ##__VA_ARGS__)))
#define VSINK_LOG_V(x, ...) MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, (FMT(x, ##__VA_ARGS__)))
#define DUMP_LOG(x, ...) NS_DebugBreak(NS_DEBUG_WARNING, nsPrintfCString(FMT(x, ##__VA_ARGS__)).get(), nullptr, nullptr, -1)

using namespace mozilla::layers;

namespace media {

// For badly muxed files, if we try to set a timeout to discard expired
// frames, but the start time of the next frame is less than the clock time,
// we instead just set a timeout FAILOVER_UPDATE_INTERVAL_US in the future,
// and run UpdateRenderedVideoFrames() then.
static const int64_t FAILOVER_UPDATE_INTERVAL_US = 1000000 / 30;

VideoSink::VideoSink(AbstractThread* aThread,
                     MediaSink* aAudioSink,
                     MediaQueue<MediaData>& aVideoQueue,
                     VideoFrameContainer* aContainer,
                     FrameStatistics& aFrameStats,
                     uint32_t aVQueueSentToCompositerSize)
  : mOwnerThread(aThread)
  , mAudioSink(aAudioSink)
  , mVideoQueue(aVideoQueue)
  , mContainer(aContainer)
  , mProducerID(ImageContainer::AllocateProducerID())
  , mFrameStats(aFrameStats)
  , mVideoFrameEndTime(-1)
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

int64_t
VideoSink::GetEndTime(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kVideoTrack) {
    return mVideoFrameEndTime;
  } else if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->GetEndTime(aType);
  }
  return -1;
}

int64_t
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
VideoSink::Start(int64_t aStartTime, const MediaInfo& aInfo)
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
      mVideoSinkEndRequest.Begin(p->Then(mOwnerThread, __func__,
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
        }));
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
  mVideoFrameEndTime = -1;
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
VideoSink::OnVideoQueuePushed(RefPtr<MediaData>&& aSample)
{
  AssertOwnerThread();
  // Listen to push event, VideoSink should try rendering ASAP if first frame
  // arrives but update scheduler is not triggered yet.
  VideoData* v = aSample->As<VideoData>();
  if (!v->mSentToCompositor) {
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

  if (VideoQueue().GetSize() > 0) {
    RenderVideoFrames(1);
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
  if (!mUpdateScheduler.IsScheduled() && VideoQueue().GetSize() >= 1 &&
      mAudioSink->IsPlaying()) {
    UpdateRenderedVideoFrames();
  }
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

  AutoTArray<RefPtr<MediaData>,16> frames;
  VideoQueue().GetFirstElements(aMaxFrames, &frames);
  if (frames.IsEmpty() || !mContainer) {
    return;
  }

  AutoTArray<ImageContainer::NonOwningImage,16> images;
  TimeStamp lastFrameTime;
  MediaSink::PlaybackParams params = mAudioSink->GetPlaybackParams();
  for (uint32_t i = 0; i < frames.Length(); ++i) {
    VideoData* frame = frames[i]->As<VideoData>();

    frame->mSentToCompositor = true;

    if (!frame->mImage || !frame->mImage->IsValid() ||
        !frame->mImage->GetSize().width || !frame->mImage->GetSize().height) {
      continue;
    }

    int64_t frameTime = frame->mTime;
    if (frameTime < 0) {
      // Frame times before the start time are invalid; drop such frames
      continue;
    }

    TimeStamp t;
    if (aMaxFrames > 1) {
      MOZ_ASSERT(!aClockTimeStamp.IsNull());
      int64_t delta = frame->mTime - aClockTime;
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

    VSINK_LOG_V("playing video frame %lld (id=%x) (vq-queued=%i)",
                frame->mTime, frame->mFrameID, VideoQueue().GetSize());
  }
  mContainer->SetCurrentFrames(frames[0]->As<VideoData>()->mDisplay, images);
}

void
VideoSink::UpdateRenderedVideoFrames()
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink->IsPlaying(), "should be called while playing.");

  // Get the current playback position.
  TimeStamp nowTime;
  const int64_t clockTime = mAudioSink->GetPosition(&nowTime);
  NS_ASSERTION(clockTime >= 0, "Should have positive clock time.");

  // Skip frames up to the playback position.
  int64_t lastDisplayedFrameEndTime = 0;
  while (VideoQueue().GetSize() > mMinVideoQueueSize &&
         clockTime >= VideoQueue().PeekFront()->GetEndTime()) {
    RefPtr<MediaData> frame = VideoQueue().PopFront();
    if (frame->As<VideoData>()->mSentToCompositor) {
      lastDisplayedFrameEndTime = frame->GetEndTime();
      mFrameStats.NotifyPresentedFrame();
    } else {
      mFrameStats.NotifyDecodedFrames({ 0, 0, 1 });
      VSINK_LOG_V("discarding video frame mTime=%lld clock_time=%lld",
                  frame->mTime, clockTime);
    }
  }

  // The presentation end time of the last video frame displayed is either
  // the end time of the current frame, or if we dropped all frames in the
  // queue, the end time of the last frame we removed from the queue.
  RefPtr<MediaData> currentFrame = VideoQueue().PeekFront();
  mVideoFrameEndTime = std::max(mVideoFrameEndTime,
    currentFrame ? currentFrame->GetEndTime() : lastDisplayedFrameEndTime);

  MaybeResolveEndPromise();

  RenderVideoFrames(mVideoQueueSendToCompositorSize, clockTime, nowTime);

  // Get the timestamp of the next frame. Schedule the next update at
  // the start time of the next frame. If we don't have a next frame,
  // we will run render loops again upon incoming frames.
  nsTArray<RefPtr<MediaData>> frames;
  VideoQueue().GetFirstElements(2, &frames);
  if (frames.Length() < 2) {
    return;
  }

  int64_t nextFrameTime = frames[1]->mTime;
  int64_t delta = (nextFrameTime > clockTime) ? (nextFrameTime - clockTime)
                                              : FAILOVER_UPDATE_INTERVAL_US;
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
    mEndPromiseHolder.ResolveIfExists(true, __func__);
  }
}

void
VideoSink::DumpDebugInfo()
{
  AssertOwnerThread();
  DUMP_LOG(
    "IsStarted=%d IsPlaying=%d, VideoQueue: finished=%d size=%d, "
    "mVideoFrameEndTime=%lld mHasVideo=%d mVideoSinkEndRequest.Exists()=%d "
    "mEndPromiseHolder.IsEmpty()=%d",
    IsStarted(), IsPlaying(), VideoQueue().IsFinished(), VideoQueue().GetSize(),
    mVideoFrameEndTime, mHasVideo, mVideoSinkEndRequest.Exists(), mEndPromiseHolder.IsEmpty());
  mAudioSink->DumpDebugInfo();
}

} // namespace media
} // namespace mozilla
