/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderReader.h"

#include "AbstractMediaDecoder.h"
#include "ImageContainer.h"
#include "MediaPrefs.h"
#include "MediaResource.h"
#include "VideoUtils.h"
#include "mozilla/Mutex.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/mozalloc.h"
#include "nsPrintfCString.h"
#include <algorithm>
#include <stdint.h>

using namespace mozilla::media;

namespace mozilla {

// Un-comment to enable logging of seek bisections.
//#define SEEK_LOGGING

extern LazyLogModule gMediaDecoderLog;

// avoid redefined macro in unified build
#undef FMT
#undef DECODER_LOG
#undef DECODER_WARN

#define FMT(x, ...) "Decoder=%p " x, mDecoder, ##__VA_ARGS__
#define DECODER_LOG(...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define DECODER_WARN(...) NS_WARNING(nsPrintfCString(FMT(__VA_ARGS__)).get())

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
  , mTaskQueue(new TaskQueue(
      GetMediaThreadPool(MediaThreadType::PLAYBACK),
      "MediaDecoderReader::mTaskQueue",
      /* aSupportsTailDispatch = */ true))
  , mWatchManager(this, mTaskQueue)
  , mBuffered(mTaskQueue, TimeIntervals(), "MediaDecoderReader::mBuffered (Canonical)")
  , mDuration(mTaskQueue, NullableTimeUnit(), "MediaDecoderReader::mDuration (Mirror)")
  , mIgnoreAudioOutputFormat(false)
  , mHitAudioDecodeError(false)
  , mShutdown(false)
{
  MOZ_COUNT_CTOR(MediaDecoderReader);
  MOZ_ASSERT(NS_IsMainThread());
}

nsresult
MediaDecoderReader::Init()
{
  if (mDecoder && mDecoder->DataArrivedEvent()) {
    mDataArrivedListener = mDecoder->DataArrivedEvent()->Connect(
      mTaskQueue, this, &MediaDecoderReader::NotifyDataArrived);
  }
  // Dispatch initialization that needs to happen on that task queue.
  mTaskQueue->Dispatch(NewRunnableMethod(this, &MediaDecoderReader::InitializationTask));
  return InitInternal();
}

void
MediaDecoderReader::InitializationTask()
{
  if (!mDecoder) {
    return;
  }
  if (mDecoder->CanonicalDurationOrNull()) {
    mDuration.Connect(mDecoder->CanonicalDurationOrNull());
  }

  // Initialize watchers.
  mWatchManager.Watch(mDuration, &MediaDecoderReader::UpdateBuffered);
}

MediaDecoderReader::~MediaDecoderReader()
{
  MOZ_ASSERT(mShutdown);
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

nsresult MediaDecoderReader::ResetDecode(TrackSet aTracks)
{
  if (aTracks.contains(TrackInfo::kVideoTrack)) {
    VideoQueue().Reset();
    mBaseVideoPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  if (aTracks.contains(TrackInfo::kAudioTrack)) {
    AudioQueue().Reset();
    mBaseAudioPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
  }

  return NS_OK;
}

RefPtr<MediaDecoderReader::VideoDataPromise>
MediaDecoderReader::DecodeToFirstVideoData()
{
  MOZ_ASSERT(OnTaskQueue());
  typedef VideoDataPromise PromiseType;
  RefPtr<PromiseType::Private> p = new PromiseType::Private(__func__);
  RefPtr<MediaDecoderReader> self = this;
  InvokeUntil([self] () -> bool {
    MOZ_ASSERT(self->OnTaskQueue());
    NS_ENSURE_TRUE(!self->mShutdown, false);
    bool skip = false;
    if (!self->DecodeVideoFrame(skip, media::TimeUnit::Zero())) {
      self->VideoQueue().Finish();
      return !!self->VideoQueue().PeekFront();
    }
    return true;
  }, [self] () -> bool {
    MOZ_ASSERT(self->OnTaskQueue());
    return self->VideoQueue().GetSize();
  })->Then(OwnerThread(), __func__, [self, p] () {
    p->Resolve(self->VideoQueue().PeekFront(), __func__);
  }, [p] () {
    // We don't have a way to differentiate EOS, error, and shutdown here. :-(
    p->Reject(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  });

  return p.forget();
}

void
MediaDecoderReader::UpdateBuffered()
{
  MOZ_ASSERT(OnTaskQueue());
  NS_ENSURE_TRUE_VOID(!mShutdown);
  mBuffered = GetBuffered();
}

void
MediaDecoderReader::VisibilityChanged()
{}

media::TimeIntervals
MediaDecoderReader::GetBuffered()
{
  MOZ_ASSERT(OnTaskQueue());

  AutoPinned<MediaResource> stream(mDecoder->GetResource());

  if (!mDuration.Ref().isSome()) {
    return TimeIntervals();
  }

  return GetEstimatedBufferedTimeRanges(stream, mDuration.Ref().ref().ToMicroseconds());
}

RefPtr<MediaDecoderReader::MetadataPromise>
MediaDecoderReader::AsyncReadMetadata()
{
  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("MediaDecoderReader::AsyncReadMetadata");

  // Attempt to read the metadata.
  MetadataHolder metadata;
  metadata.mInfo = MakeUnique<MediaInfo>();
  MetadataTags* tags = nullptr;
  nsresult rv = ReadMetadata(metadata.mInfo.get(), &tags);
  metadata.mTags.reset(tags);
  metadata.mInfo->AssertValid();

  // Update the buffer ranges before resolving the metadata promise. Bug 1320258.
  UpdateBuffered();

  // We're not waiting for anything. If we didn't get the metadata, that's an
  // error.
  if (NS_FAILED(rv) || !metadata.mInfo->HasValidMedia()) {
    DECODER_WARN("ReadMetadata failed, rv=%" PRIx32 " HasValidMedia=%d",
                 static_cast<uint32_t>(rv), metadata.mInfo->HasValidMedia());
    return MetadataPromise::CreateAndReject(NS_ERROR_DOM_MEDIA_METADATA_ERR, __func__);
  }

  // Success!
  return MetadataPromise::CreateAndResolve(Move(metadata), __func__);
}

class ReRequestVideoWithSkipTask : public Runnable
{
public:
  ReRequestVideoWithSkipTask(MediaDecoderReader* aReader,
                             const media::TimeUnit& aTimeThreshold)
    : mReader(aReader)
    , mTimeThreshold(aTimeThreshold)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mReader->OnTaskQueue());

    // Make sure ResetDecode hasn't been called in the mean time.
    if (!mReader->mBaseVideoPromise.IsEmpty()) {
      mReader->RequestVideoData(mTimeThreshold);
    }

    return NS_OK;
  }

private:
  RefPtr<MediaDecoderReader> mReader;
  const media::TimeUnit mTimeThreshold;
};

class ReRequestAudioTask : public Runnable
{
public:
  explicit ReRequestAudioTask(MediaDecoderReader* aReader)
    : mReader(aReader)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mReader->OnTaskQueue());

    // Make sure ResetDecode hasn't been called in the mean time.
    if (!mReader->mBaseAudioPromise.IsEmpty()) {
      mReader->RequestAudioData();
    }

    return NS_OK;
  }

private:
  RefPtr<MediaDecoderReader> mReader;
};

RefPtr<MediaDecoderReader::VideoDataPromise>
MediaDecoderReader::RequestVideoData(const media::TimeUnit& aTimeThreshold)
{
  RefPtr<VideoDataPromise> p = mBaseVideoPromise.Ensure(__func__);
  bool skip = false;
  while (VideoQueue().GetSize() == 0 &&
         !VideoQueue().IsFinished()) {
    if (!DecodeVideoFrame(skip, aTimeThreshold)) {
      VideoQueue().Finish();
    } else if (skip) {
      // We still need to decode more data in order to skip to the next
      // keyframe. Post another task to the decode task queue to decode
      // again. We don't just decode straight in a loop here, as that
      // would hog the decode task queue.
      RefPtr<nsIRunnable> task(
        new ReRequestVideoWithSkipTask(this, aTimeThreshold));
      mTaskQueue->Dispatch(task.forget());
      return p;
    }
  }
  if (VideoQueue().GetSize() > 0) {
    RefPtr<VideoData> v = VideoQueue().PopFront();
    mBaseVideoPromise.Resolve(v, __func__);
  } else if (VideoQueue().IsFinished()) {
    mBaseVideoPromise.Reject(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  } else {
    MOZ_ASSERT(false, "Dropping this promise on the floor");
  }

  return p;
}

RefPtr<MediaDecoderReader::AudioDataPromise>
MediaDecoderReader::RequestAudioData()
{
  RefPtr<AudioDataPromise> p = mBaseAudioPromise.Ensure(__func__);
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
    if (AudioQueue().GetSize() == 0) {
      RefPtr<nsIRunnable> task(new ReRequestAudioTask(this));
      mTaskQueue->Dispatch(task.forget());
      return p;
    }
  }
  if (AudioQueue().GetSize() > 0) {
    RefPtr<AudioData> a = AudioQueue().PopFront();
    mBaseAudioPromise.Resolve(a, __func__);
  } else if (AudioQueue().IsFinished()) {
    mBaseAudioPromise.Reject(mHitAudioDecodeError
                             ? NS_ERROR_DOM_MEDIA_FATAL_ERR
                             : NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
    mHitAudioDecodeError = false;
  } else {
    MOZ_ASSERT(false, "Dropping this promise on the floor");
  }

  return p;
}

RefPtr<ShutdownPromise>
MediaDecoderReader::Shutdown()
{
  MOZ_ASSERT(OnTaskQueue());
  mShutdown = true;

  mBaseAudioPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);
  mBaseVideoPromise.RejectIfExists(NS_ERROR_DOM_MEDIA_END_OF_STREAM, __func__);

  mDataArrivedListener.DisconnectIfExists();

  ReleaseResources();
  mDuration.DisconnectIfConnected();
  mBuffered.DisconnectAll();

  // Shut down the watch manager before shutting down our task queue.
  mWatchManager.Shutdown();

  mDecoder = nullptr;

  return mTaskQueue->BeginShutdown();
}

} // namespace mozilla
