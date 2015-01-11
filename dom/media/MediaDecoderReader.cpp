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

#include "mozilla/mozalloc.h"
#include <stdint.h>
#include <algorithm>

namespace mozilla {

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(x, ...) \
  PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, ("Decoder=%p " x, mDecoder, ##__VA_ARGS__))
#else
#define DECODER_LOG(x, ...)
#endif

PRLogModuleInfo* gMediaPromiseLog;

void
EnsureMediaPromiseLog()
{
  if (!gMediaPromiseLog) {
    gMediaPromiseLog = PR_NewLogModule("MediaPromise");
  }
}

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
  EnsureMediaPromiseLog();
}

MediaDecoderReader::~MediaDecoderReader()
{
  MOZ_ASSERT(mShutdown);
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
  nsresult res = NS_OK;

  VideoQueue().Reset();
  AudioQueue().Reset();

  mAudioDiscontinuity = true;
  mVideoDiscontinuity = true;

  return res;
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

nsresult
MediaDecoderReader::GetBuffered(mozilla::dom::TimeRanges* aBuffered)
{
  AutoPinned<MediaResource> stream(mDecoder->GetResource());
  int64_t durationUs = 0;
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    durationUs = mDecoder->GetMediaDuration();
  }
  GetEstimatedBufferedTimeRanges(stream, durationUs, aBuffered);
  return NS_OK;
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
  MOZ_ASSERT(startTime >= 0);
  return startTime;
}

class RequestVideoWithSkipTask : public nsRunnable {
public:
  RequestVideoWithSkipTask(MediaDecoderReader* aReader,
                           int64_t aTimeThreshold)
    : mReader(aReader)
    , mTimeThreshold(aTimeThreshold)
  {
  }
  NS_METHOD Run() {
    bool skip = true;
    mReader->RequestVideoData(skip, mTimeThreshold);
    return NS_OK;
  }
private:
  nsRefPtr<MediaDecoderReader> mReader;
  int64_t mTimeThreshold;
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
      RefPtr<nsIRunnable> task(new RequestVideoWithSkipTask(this, aTimeThreshold));
      mTaskQueue->Dispatch(task);
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
      RefPtr<nsIRunnable> task(NS_NewRunnableMethod(
          this, &MediaDecoderReader::RequestAudioData));
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
    RefPtr<SharedThreadPool> decodePool(GetMediaDecodeThreadPool());
    NS_ENSURE_TRUE(decodePool, nullptr);

    mTaskQueue = new MediaTaskQueue(decodePool.forget());
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
  MOZ_ASSERT(OnDecodeThread());
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
    p = new ShutdownPromise(__func__);
    p->Resolve(true, __func__);
  }

  return p;
}

} // namespace mozilla
