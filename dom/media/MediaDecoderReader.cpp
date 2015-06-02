/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderReader.h"
#include "AbstractMediaDecoder.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "ImageContainer.h"

#include "nsPrintfCString.h"
#include "mozilla/mozalloc.h"
#include <stdint.h>
#include <algorithm>

namespace mozilla {

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(x, ...) \
  MOZ_LOG(gMediaDecoderLog, PR_LOG_DEBUG, ("Decoder=%p " x, mDecoder, ##__VA_ARGS__))

// Same workaround as MediaDecoderStateMachine.cpp.
#define DECODER_WARN_HELPER(a, b) NS_WARNING b
#define DECODER_WARN(x, ...) \
  DECODER_WARN_HELPER(0, (nsPrintfCString("Decoder=%p " x, mDecoder, ##__VA_ARGS__).get()))

class VideoQueueMemoryFunctor : public nsDequeFunctor {
public:
  VideoQueueMemoryFunctor() : mSize(0) {}

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  virtual void* operator()(void* aObject) {
    const VideoData* v = static_cast<const VideoData*>(aObject);
    mSize += v->SizeOfIncludingThis(MallocSizeOf);
    return nullptr;
  }

  size_t mSize;
};


class AudioQueueMemoryFunctor : public nsDequeFunctor {
public:
  AudioQueueMemoryFunctor() : mSize(0) {}

  MOZ_DEFINE_MALLOC_SIZE_OF(MallocSizeOf);

  virtual void* operator()(void* aObject) {
    const AudioData* audioData = static_cast<const AudioData*>(aObject);
    mSize += audioData->SizeOfIncludingThis(MallocSizeOf);
    return nullptr;
  }

  size_t mSize;
};

MediaDecoderReader::MediaDecoderReader(AbstractMediaDecoder* aDecoder)
  : mAudioCompactor(mAudioQueue)
  , mDecoder(aDecoder)
  , mIgnoreAudioOutputFormat(false)
  , mStartTime(-1)
  , mHitAudioDecodeError(false)
  , mShutdown(false)
  , mTaskQueueIsBorrowed(false)
  , mAudioDiscontinuity(false)
  , mVideoDiscontinuity(false)
{
  MOZ_COUNT_CTOR(MediaDecoderReader);
}

MediaDecoderReader::~MediaDecoderReader()
{
  MOZ_ASSERT(mShutdown);
  MOZ_ASSERT(!mDecoder);
  ResetDecode();
  MOZ_COUNT_DTOR(MediaDecoderReader);
}

size_t MediaDecoderReader::SizeOfVideoQueueInBytes() const
{
  VideoQueueMemoryFunctor functor;
  mVideoQueue.LockedForEach(functor);
  return functor.mSize;
}

size_t MediaDecoderReader::SizeOfAudioQueueInBytes() const
{
  AudioQueueMemoryFunctor functor;
  mAudioQueue.LockedForEach(functor);
  return functor.mSize;
}

size_t MediaDecoderReader::SizeOfVideoQueueInFrames()
{
  return mVideoQueue.GetSize();
}

size_t MediaDecoderReader::SizeOfAudioQueueInFrames()
{
  return mAudioQueue.GetSize();
}

nsresult MediaDecoderReader::ResetDecode()
{
  VideoQueue().Reset();
  AudioQueue().Reset();

  mAudioDiscontinuity = true;
  mVideoDiscontinuity = true;

  mBaseAudioPromise.RejectIfExists(CANCELED, __func__);
  mBaseVideoPromise.RejectIfExists(CANCELED, __func__);

  return NS_OK;
}

VideoData* MediaDecoderReader::DecodeToFirstVideoData()
{
  bool eof = false;
  while (!eof && VideoQueue().GetSize() == 0) {
    {
      ReentrantMonitorAutoEnter decoderMon(mDecoder->GetReentrantMonitor());
      if (mDecoder->IsShutdown()) {
        return nullptr;
      }
    }
    bool keyframeSkip = false;
    eof = !DecodeVideoFrame(keyframeSkip, 0);
  }
  if (eof) {
    VideoQueue().Finish();
  }
  VideoData* d = nullptr;
  return (d = VideoQueue().PeekFront()) ? d : nullptr;
}

void
MediaDecoderReader::SetStartTime(int64_t aStartTime)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mStartTime = aStartTime;
}

media::TimeIntervals
MediaDecoderReader::GetBuffered()
{
  AutoPinned<MediaResource> stream(mDecoder->GetResource());
  int64_t durationUs = 0;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    durationUs = mDecoder->GetMediaDuration();
  }
  return GetEstimatedBufferedTimeRanges(stream, durationUs);
}

int64_t
MediaDecoderReader::ComputeStartTime(const VideoData* aVideo, const AudioData* aAudio)
{
  int64_t startTime = std::min<int64_t>(aAudio ? aAudio->mTime : INT64_MAX,
                                        aVideo ? aVideo->mTime : INT64_MAX);
  if (startTime == INT64_MAX) {
    startTime = 0;
  }
  DECODER_LOG("ComputeStartTime first video frame start %lld", aVideo ? aVideo->mTime : -1);
  DECODER_LOG("ComputeStartTime first audio frame start %lld", aAudio ? aAudio->mTime : -1);
  NS_ASSERTION(startTime >= 0, "Start time is negative");
  return startTime;
}

nsRefPtr<MediaDecoderReader::MetadataPromise>
MediaDecoderReader::AsyncReadMetadata()
{
  typedef ReadMetadataFailureReason Reason;

  MOZ_ASSERT(OnTaskQueue());
  mDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  DECODER_LOG("MediaDecoderReader::AsyncReadMetadata");

  if (IsWaitingMediaResources()) {
    return MetadataPromise::CreateAndReject(Reason::WAITING_FOR_RESOURCES, __func__);
  }

  // Attempt to read the metadata.
  nsRefPtr<MetadataHolder> metadata = new MetadataHolder();
  nsresult rv = ReadMetadata(&metadata->mInfo, getter_Transfers(metadata->mTags));

  // Reading metadata can cause us to discover that we need resources (a hardware
  // resource initialized but not yet ready for use).
  if (IsWaitingMediaResources()) {
    return MetadataPromise::CreateAndReject(Reason::WAITING_FOR_RESOURCES, __func__);
  }

  // We're not waiting for anything. If we didn't get the metadata, that's an
  // error.
  if (NS_FAILED(rv) || !metadata->mInfo.HasValidMedia()) {
    DECODER_WARN("ReadMetadata failed, rv=%x HasValidMedia=%d", rv, metadata->mInfo.HasValidMedia());
    return MetadataPromise::CreateAndReject(Reason::METADATA_ERROR, __func__);
  }

  // Success!
  return MetadataPromise::CreateAndResolve(metadata, __func__);
}

class ReRequestVideoWithSkipTask : public nsRunnable
{
public:
  ReRequestVideoWithSkipTask(MediaDecoderReader* aReader,
                             int64_t aTimeThreshold)
    : mReader(aReader)
    , mTimeThreshold(aTimeThreshold)
  {
  }

  NS_METHOD Run()
  {
    MOZ_ASSERT(mReader->GetTaskQueue()->IsCurrentThreadIn());

    // Make sure ResetDecode hasn't been called in the mean time.
    if (!mReader->mBaseVideoPromise.IsEmpty()) {
      mReader->RequestVideoData(/* aSkip = */ true, mTimeThreshold);
    }

    return NS_OK;
  }

private:
  nsRefPtr<MediaDecoderReader> mReader;
  const int64_t mTimeThreshold;
};

class ReRequestAudioTask : public nsRunnable
{
public:
  explicit ReRequestAudioTask(MediaDecoderReader* aReader)
    : mReader(aReader)
  {
  }

  NS_METHOD Run()
  {
    MOZ_ASSERT(mReader->GetTaskQueue()->IsCurrentThreadIn());

    // Make sure ResetDecode hasn't been called in the mean time.
    if (!mReader->mBaseAudioPromise.IsEmpty()) {
      mReader->RequestAudioData();
    }

    return NS_OK;
  }

private:
  nsRefPtr<MediaDecoderReader> mReader;
};

nsRefPtr<MediaDecoderReader::VideoDataPromise>
MediaDecoderReader::RequestVideoData(bool aSkipToNextKeyframe,
                                     int64_t aTimeThreshold)
{
  nsRefPtr<VideoDataPromise> p = mBaseVideoPromise.Ensure(__func__);
  bool skip = aSkipToNextKeyframe;
  while (VideoQueue().GetSize() == 0 &&
         !VideoQueue().IsFinished()) {
    if (!DecodeVideoFrame(skip, aTimeThreshold)) {
      VideoQueue().Finish();
    } else if (skip) {
      // We still need to decode more data in order to skip to the next
      // keyframe. Post another task to the decode task queue to decode
      // again. We don't just decode straight in a loop here, as that
      // would hog the decode task queue.
      RefPtr<nsIRunnable> task(new ReRequestVideoWithSkipTask(this, aTimeThreshold));
      mTaskQueue->Dispatch(task.forget());
      return p;
    }
  }
  if (VideoQueue().GetSize() > 0) {
    nsRefPtr<VideoData> v = VideoQueue().PopFront();
    if (v && mVideoDiscontinuity) {
      v->mDiscontinuity = true;
      mVideoDiscontinuity = false;
    }
    mBaseVideoPromise.Resolve(v, __func__);
  } else if (VideoQueue().IsFinished()) {
    mBaseVideoPromise.Reject(END_OF_STREAM, __func__);
  } else {
    MOZ_ASSERT(false, "Dropping this promise on the floor");
  }

  return p;
}

nsRefPtr<MediaDecoderReader::AudioDataPromise>
MediaDecoderReader::RequestAudioData()
{
  nsRefPtr<AudioDataPromise> p = mBaseAudioPromise.Ensure(__func__);
  while (AudioQueue().GetSize() == 0 &&
         !AudioQueue().IsFinished()) {
    if (!DecodeAudioData()) {
      AudioQueue().Finish();
      break;
    }
    // AudioQueue size is still zero, post a task to try again. Don't spin
    // waiting in this while loop since it somehow prevents audio EOS from
    // coming in gstreamer 1.x when there is still video buffer waiting to be
    // consumed. (|mVideoSinkBufferCount| > 0)
    if (AudioQueue().GetSize() == 0 && mTaskQueue) {
      RefPtr<nsIRunnable> task(new ReRequestAudioTask(this));
      mTaskQueue->Dispatch(task.forget());
      return p;
    }
  }
  if (AudioQueue().GetSize() > 0) {
    nsRefPtr<AudioData> a = AudioQueue().PopFront();
    if (mAudioDiscontinuity) {
      a->mDiscontinuity = true;
      mAudioDiscontinuity = false;
    }
    mBaseAudioPromise.Resolve(a, __func__);
  } else if (AudioQueue().IsFinished()) {
    mBaseAudioPromise.Reject(mHitAudioDecodeError ? DECODE_ERROR : END_OF_STREAM, __func__);
    mHitAudioDecodeError = false;
  } else {
    MOZ_ASSERT(false, "Dropping this promise on the floor");
  }

  return p;
}

MediaTaskQueue*
MediaDecoderReader::EnsureTaskQueue()
{
  if (!mTaskQueue) {
    MOZ_ASSERT(!mTaskQueueIsBorrowed);
    RefPtr<SharedThreadPool> pool(GetMediaThreadPool(MediaThreadType::PLAYBACK));
    MOZ_DIAGNOSTIC_ASSERT(pool);
    mTaskQueue = new MediaTaskQueue(pool.forget());
  }

  return mTaskQueue;
}

void
MediaDecoderReader::BreakCycles()
{
  mTaskQueue = nullptr;
}

nsRefPtr<ShutdownPromise>
MediaDecoderReader::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  mShutdown = true;

  mBaseAudioPromise.RejectIfExists(END_OF_STREAM, __func__);
  mBaseVideoPromise.RejectIfExists(END_OF_STREAM, __func__);

  ReleaseMediaResources();
  nsRefPtr<ShutdownPromise> p;

  // Spin down the task queue if necessary. We wait until BreakCycles to null
  // out mTaskQueue, since otherwise any remaining tasks could crash when they
  // invoke GetTaskQueue()->IsCurrentThreadIn().
  if (mTaskQueue && !mTaskQueueIsBorrowed) {
    // If we own our task queue, shutdown ends when the task queue is done.
    p = mTaskQueue->BeginShutdown();
  } else {
    // If we don't own our task queue, we resolve immediately (though
    // asynchronously).
    p = ShutdownPromise::CreateAndResolve(true, __func__);
  }

  mDecoder = nullptr;

  return p;
}

} // namespace mozilla

#undef DECODER_LOG
#undef DECODER_WARN
#undef DECODER_WARN_HELPER
