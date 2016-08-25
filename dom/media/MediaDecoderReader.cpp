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
#include "MediaPrefs.h"

#include "nsPrintfCString.h"
#include "mozilla/mozalloc.h"
#include "mozilla/Mutex.h"
#include <stdint.h>
#include <algorithm>

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

// The ReaderQueue is used to keep track of the numer of active readers to
// enforce a given limit on the number of simultaneous active decoders.
// Readers are added/removed during construction/destruction and are
// suspended and resumed by the queue. The max number of active decoders is
// controlled by the "media.decoder.limit" pref.
class ReaderQueue
{
public:
  static ReaderQueue& Instance()
  {
    static StaticMutex sMutex;
    StaticMutexAutoLock lock(sMutex);

    if (!sInstance) {
      sInstance = new ReaderQueue;
      sInstance->MaxNumActive(MediaPrefs::MediaDecoderLimit());
      ClearOnShutdown(&sInstance, ShutdownPhase::Shutdown);
    }
    MOZ_ASSERT(sInstance);
    return *sInstance;
  }

  void MaxNumActive(int32_t aNumActive)
  {
    MutexAutoLock lock(mMutex);

    if (aNumActive < 0) {
      mNumMaxActive = std::numeric_limits<uint32_t>::max();
    } else {
      mNumMaxActive = aNumActive;
    }
  }

  void Add(MediaDecoderReader* aReader)
  {
    MutexAutoLock lock(mMutex);

    if (mActive.Length() < mNumMaxActive) {
      // Below active limit, resume the new reader.
      mActive.AppendElement(aReader);
      DispatchResume(aReader);
    } else if (mActive.IsEmpty()) {
      MOZ_ASSERT(mNumMaxActive == 0);
      mSuspended.AppendElement(aReader);
    } else {
      // We're past the active limit, suspend an old reader and resume the new.
      mActive.AppendElement(aReader);
      MediaDecoderReader* suspendReader = mActive.ElementAt(0);
      mSuspended.AppendElement(suspendReader);
      mActive.RemoveElementAt(0);
      DispatchSuspendResume(suspendReader, aReader);
    }
  }

  void Remove(MediaDecoderReader* aReader)
  {
    MutexAutoLock lock(mMutex);

    // Remove the reader from the queue. Note that the reader's IsSuspended
    // state is updated on the task queue, so we cannot depend on it here to
    // determine the factual suspension state.
    DebugOnly<bool> suspended = mSuspended.RemoveElement(aReader);
    bool active = mActive.RemoveElement(aReader);

    MOZ_ASSERT(suspended || active, "Reader must be in the queue");

    if (active && !mSuspended.IsEmpty()) {
      // For each removed active reader, we resume a suspended one.
      MediaDecoderReader* resumeReader = mSuspended.LastElement();
      mActive.AppendElement(resumeReader);
      mSuspended.RemoveElementAt(mSuspended.Length() - 1);
      DispatchResume(resumeReader);
    }
  }

private:
  ReaderQueue()
    : mNumMaxActive(std::numeric_limits<uint32_t>::max())
    , mMutex("ReaderQueue:mMutex")
  {
  }

  static void Resume(MediaDecoderReader* aReader)
  {
    if (!aReader->IsSuspended()) {
      return;
    }
    aReader->SetIsSuspended(false);
  }

  static void Suspend(MediaDecoderReader* aReader)
  {
    if (aReader->IsSuspended()) {
      return;
    }
    aReader->SetIsSuspended(true);

    aReader->ReleaseResources();
  }

  static void DispatchResume(MediaDecoderReader* aReader)
  {
    RefPtr<MediaDecoderReader> reader = aReader;

    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      [reader]() {
        Resume(reader);
    });
    reader->OwnerThread()->Dispatch(task.forget());
  }

  static void DispatchSuspend(MediaDecoderReader* aReader)
  {
    RefPtr<MediaDecoderReader> reader = aReader;

    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      [reader]() {
        Suspend(reader);
    });
    reader->OwnerThread()->Dispatch(task.forget());
  }

  static void DispatchSuspendResume(MediaDecoderReader* aSuspend,
                                    MediaDecoderReader* aResume)
  {
    RefPtr<MediaDecoderReader> suspend = aSuspend;
    RefPtr<MediaDecoderReader> resume = aResume;

    nsCOMPtr<nsIRunnable> task = NS_NewRunnableFunction(
      [suspend, resume] () {
        Suspend(suspend);
        DispatchResume(resume);
    });
    suspend->OwnerThread()->Dispatch(task.forget());
  }

  static StaticAutoPtr<ReaderQueue> sInstance;

  nsTArray<RefPtr<MediaDecoderReader>> mActive;
  nsTArray<RefPtr<MediaDecoderReader>> mSuspended;
  uint32_t mNumMaxActive;

  mutable Mutex mMutex;
};

StaticAutoPtr<ReaderQueue> ReaderQueue::sInstance;

MediaDecoderReader::MediaDecoderReader(AbstractMediaDecoder* aDecoder)
  : mAudioCompactor(mAudioQueue)
  , mDecoder(aDecoder)
  , mTaskQueue(new TaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK),
                             /* aSupportsTailDispatch = */ true))
  , mWatchManager(this, mTaskQueue)
  , mBuffered(mTaskQueue, TimeIntervals(), "MediaDecoderReader::mBuffered (Canonical)")
  , mDuration(mTaskQueue, NullableTimeUnit(), "MediaDecoderReader::mDuration (Mirror)")
  , mIgnoreAudioOutputFormat(false)
  , mHitAudioDecodeError(false)
  , mShutdown(false)
  , mVideoDiscontinuity(false)
  , mIsSuspended(mTaskQueue, true,
                 "MediaDecoderReader::mIsSuspended (Canonical)")
{
  MOZ_COUNT_CTOR(MediaDecoderReader);
  MOZ_ASSERT(NS_IsMainThread());

  if (mDecoder && mDecoder->DataArrivedEvent()) {
    mDataArrivedListener = mDecoder->DataArrivedEvent()->Connect(
      mTaskQueue, this, &MediaDecoderReader::NotifyDataArrived);
  }

  ReaderQueue::Instance().Add(this);

  // Dispatch initialization that needs to happen on that task queue.
  mTaskQueue->Dispatch(NewRunnableMethod(this, &MediaDecoderReader::InitializationTask));
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
    mVideoDiscontinuity = true;
    mBaseVideoPromise.RejectIfExists(CANCELED, __func__);
  }

  if (aTracks.contains(TrackInfo::kAudioTrack)) {
    AudioQueue().Reset();
    mBaseAudioPromise.RejectIfExists(CANCELED, __func__);
  }

  return NS_OK;
}

RefPtr<MediaDecoderReader::MediaDataPromise>
MediaDecoderReader::DecodeToFirstVideoData()
{
  MOZ_ASSERT(OnTaskQueue());
  typedef MediaDecoderReader::MediaDataPromise PromiseType;
  RefPtr<PromiseType::Private> p = new PromiseType::Private(__func__);
  RefPtr<MediaDecoderReader> self = this;
  InvokeUntil([self] () -> bool {
    MOZ_ASSERT(self->OnTaskQueue());
    NS_ENSURE_TRUE(!self->mShutdown, false);
    bool skip = false;
    if (!self->DecodeVideoFrame(skip, 0)) {
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
    p->Reject(END_OF_STREAM, __func__);
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
  if (!HaveStartTime()) {
    return media::TimeIntervals();
  }
  AutoPinned<MediaResource> stream(mDecoder->GetResource());

  if (!mDuration.Ref().isSome()) {
    return TimeIntervals();
  }

  return GetEstimatedBufferedTimeRanges(stream, mDuration.Ref().ref().ToMicroseconds());
}

RefPtr<MediaDecoderReader::MetadataPromise>
MediaDecoderReader::AsyncReadMetadata()
{
  typedef ReadMetadataFailureReason Reason;

  MOZ_ASSERT(OnTaskQueue());
  DECODER_LOG("MediaDecoderReader::AsyncReadMetadata");

  // Attempt to read the metadata.
  RefPtr<MetadataHolder> metadata = new MetadataHolder();
  nsresult rv = ReadMetadata(&metadata->mInfo, getter_Transfers(metadata->mTags));
  metadata->mInfo.AssertValid();

  // We're not waiting for anything. If we didn't get the metadata, that's an
  // error.
  if (NS_FAILED(rv) || !metadata->mInfo.HasValidMedia()) {
    DECODER_WARN("ReadMetadata failed, rv=%x HasValidMedia=%d", rv, metadata->mInfo.HasValidMedia());
    return MetadataPromise::CreateAndReject(Reason::METADATA_ERROR, __func__);
  }

  // Success!
  return MetadataPromise::CreateAndResolve(metadata, __func__);
}

class ReRequestVideoWithSkipTask : public Runnable
{
public:
  ReRequestVideoWithSkipTask(MediaDecoderReader* aReader,
                             int64_t aTimeThreshold)
    : mReader(aReader)
    , mTimeThreshold(aTimeThreshold)
  {
  }

  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(mReader->OnTaskQueue());

    // Make sure ResetDecode hasn't been called in the mean time.
    if (!mReader->mBaseVideoPromise.IsEmpty()) {
      mReader->RequestVideoData(/* aSkip = */ true, mTimeThreshold);
    }

    return NS_OK;
  }

private:
  RefPtr<MediaDecoderReader> mReader;
  const int64_t mTimeThreshold;
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

RefPtr<MediaDecoderReader::MediaDataPromise>
MediaDecoderReader::RequestVideoData(bool aSkipToNextKeyframe,
                                     int64_t aTimeThreshold)
{
  RefPtr<MediaDataPromise> p = mBaseVideoPromise.Ensure(__func__);
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
    RefPtr<VideoData> v = VideoQueue().PopFront();
    if (v && mVideoDiscontinuity) {
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

RefPtr<MediaDecoderReader::MediaDataPromise>
MediaDecoderReader::RequestAudioData()
{
  RefPtr<MediaDataPromise> p = mBaseAudioPromise.Ensure(__func__);
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
    mBaseAudioPromise.Reject(mHitAudioDecodeError ? DECODE_ERROR : END_OF_STREAM, __func__);
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

  mBaseAudioPromise.RejectIfExists(END_OF_STREAM, __func__);
  mBaseVideoPromise.RejectIfExists(END_OF_STREAM, __func__);

  mDataArrivedListener.DisconnectIfExists();

  ReleaseResources();
  mDuration.DisconnectIfConnected();
  mBuffered.DisconnectAll();
  mIsSuspended.DisconnectAll();

  // Shut down the watch manager before shutting down our task queue.
  mWatchManager.Shutdown();

  RefPtr<ShutdownPromise> p;

  mDecoder = nullptr;

  ReaderQueue::Instance().Remove(this);

  return mTaskQueue->BeginShutdown();
}

} // namespace mozilla
