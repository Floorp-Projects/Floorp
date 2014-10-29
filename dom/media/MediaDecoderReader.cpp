/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderReader.h"
#include "AbstractMediaDecoder.h"
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
  , mAudioDiscontinuity(false)
  , mVideoDiscontinuity(false)
{
  MOZ_COUNT_CTOR(MediaDecoderReader);
}

MediaDecoderReader::~MediaDecoderReader()
{
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

nsresult
MediaDecoderReader::GetBuffered(mozilla::dom::TimeRanges* aBuffered,
                                int64_t aStartTime)
{
  MediaResource* stream = mDecoder->GetResource();
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

void
MediaDecoderReader::RequestVideoData(bool aSkipToNextKeyframe,
                                     int64_t aTimeThreshold)
{
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
      return;
    }
  }
  if (VideoQueue().GetSize() > 0) {
    VideoData* v = VideoQueue().PopFront();
    if (v && mVideoDiscontinuity) {
      v->mDiscontinuity = true;
      mVideoDiscontinuity = false;
    }
    GetCallback()->OnVideoDecoded(v);
  } else if (VideoQueue().IsFinished()) {
    GetCallback()->OnVideoEOS();
  }
}

void
MediaDecoderReader::RequestAudioData()
{
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
      return;
    }
  }
  if (AudioQueue().GetSize() > 0) {
    AudioData* a = AudioQueue().PopFront();
    if (mAudioDiscontinuity) {
      a->mDiscontinuity = true;
      mAudioDiscontinuity = false;
    }
    GetCallback()->OnAudioDecoded(a);
    return;
  } else if (AudioQueue().IsFinished()) {
    GetCallback()->OnAudioEOS();
    return;
  }
}

void
MediaDecoderReader::SetCallback(RequestSampleCallback* aCallback)
{
  mSampleDecodedCallback = aCallback;
}

void
MediaDecoderReader::SetTaskQueue(MediaTaskQueue* aTaskQueue)
{
  mTaskQueue = aTaskQueue;
}

void
MediaDecoderReader::BreakCycles()
{
  if (mSampleDecodedCallback) {
    mSampleDecodedCallback->BreakCycles();
    mSampleDecodedCallback = nullptr;
  }
  mTaskQueue = nullptr;
}

void
MediaDecoderReader::Shutdown()
{
  ReleaseMediaResources();
}

AudioDecodeRendezvous::AudioDecodeRendezvous()
  : mMonitor("AudioDecodeRendezvous")
  , mHaveResult(false)
{
}

AudioDecodeRendezvous::~AudioDecodeRendezvous()
{
}

void
AudioDecodeRendezvous::OnAudioDecoded(AudioData* aSample)
{
  MonitorAutoLock mon(mMonitor);
  mSample = aSample;
  mStatus = NS_OK;
  mHaveResult = true;
  mon.NotifyAll();
}

void
AudioDecodeRendezvous::OnAudioEOS()
{
  MonitorAutoLock mon(mMonitor);
  mSample = nullptr;
  mStatus = NS_OK;
  mHaveResult = true;
  mon.NotifyAll();
}

void
AudioDecodeRendezvous::OnDecodeError()
{
  MonitorAutoLock mon(mMonitor);
  mSample = nullptr;
  mStatus = NS_ERROR_FAILURE;
  mHaveResult = true;
  mon.NotifyAll();
}

void
AudioDecodeRendezvous::Reset()
{
  MonitorAutoLock mon(mMonitor);
  mHaveResult = false;
  mStatus = NS_OK;
  mSample = nullptr;
}

nsresult
AudioDecodeRendezvous::Await(nsAutoPtr<AudioData>& aSample)
{
  MonitorAutoLock mon(mMonitor);
  while (!mHaveResult) {
    mon.Wait();
  }
  mHaveResult = false;
  aSample = mSample;
  return mStatus;
}

void
AudioDecodeRendezvous::Cancel()
{
  MonitorAutoLock mon(mMonitor);
  mStatus = NS_ERROR_ABORT;
  mHaveResult = true;
  mon.NotifyAll();
}

} // namespace mozilla
