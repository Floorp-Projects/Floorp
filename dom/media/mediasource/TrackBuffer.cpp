/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackBuffer.h"

#include "ContainerParser.h"
#include "MediaData.h"
#include "MediaSourceDecoder.h"
#include "SharedThreadPool.h"
#include "MediaTaskQueue.h"
#include "SourceBufferDecoder.h"
#include "SourceBufferResource.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
#include "mozilla/Preferences.h"
#include "nsError.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prlog.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, (__VA_ARGS__))
#define MSE_DEBUGV(...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG+1, (__VA_ARGS__))
#define MSE_API(...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#define MSE_DEBUGV(...)
#define MSE_API(...)
#endif

// Time in seconds to substract from the current time when deciding the
// time point to evict data before in a decoder. This is used to help
// prevent evicting the current playback point.
#define MSE_EVICT_THRESHOLD_TIME 2.0

// Time in microsecond under which a timestamp will be considered to be 0.
#define FUZZ_TIMESTAMP_OFFSET 100000

namespace mozilla {

TrackBuffer::TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType)
  : mParentDecoder(aParentDecoder)
  , mType(aType)
  , mLastStartTimestamp(0)
  , mLastTimestampOffset(0)
  , mAdjustedTimestamp(0)
  , mShutdown(false)
{
  MOZ_COUNT_CTOR(TrackBuffer);
  mParser = ContainerParser::CreateForMIMEType(aType);
  mTaskQueue = new MediaTaskQueue(GetMediaDecodeThreadPool());
  aParentDecoder->AddTrackBuffer(this);
  mDecoderPerSegment = Preferences::GetBool("media.mediasource.decoder-per-segment", false);
  MSE_DEBUG("TrackBuffer(%p) created for parent decoder %p", this, aParentDecoder);
}

TrackBuffer::~TrackBuffer()
{
  MOZ_COUNT_DTOR(TrackBuffer);
}

class ReleaseDecoderTask : public nsRunnable {
public:
  explicit ReleaseDecoderTask(SourceBufferDecoder* aDecoder)
    : mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    mDecoder->GetReader()->BreakCycles();
    mDecoder = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<SourceBufferDecoder> mDecoder;
};

class MOZ_STACK_CLASS DecodersToInitialize MOZ_FINAL {
public:
  explicit DecodersToInitialize(TrackBuffer* aOwner)
    : mOwner(aOwner)
  {
  }

  ~DecodersToInitialize()
  {
    for (size_t i = 0; i < mDecoders.Length(); i++) {
      mOwner->QueueInitializeDecoder(mDecoders[i]);
    }
  }

  bool NewDecoder(int64_t aTimestampOffset)
  {
    nsRefPtr<SourceBufferDecoder> decoder = mOwner->NewDecoder(aTimestampOffset);
    if (!decoder) {
      return false;
    }
    mDecoders.AppendElement(decoder);
    return true;
  }

  size_t Length()
  {
    return mDecoders.Length();
  }

  void AppendElement(SourceBufferDecoder* aDecoder)
  {
    mDecoders.AppendElement(aDecoder);
  }

private:
  TrackBuffer* mOwner;
  nsAutoTArray<nsRefPtr<SourceBufferDecoder>,1> mDecoders;
};

nsRefPtr<ShutdownPromise>
TrackBuffer::Shutdown()
{
  mParentDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mShutdown = true;
  mInitializationPromise.RejectIfExists(NS_ERROR_ABORT, __func__);

  MOZ_ASSERT(mShutdownPromise.IsEmpty());
  nsRefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);

  RefPtr<MediaTaskQueue> queue = mTaskQueue;
  mTaskQueue = nullptr;
  queue->BeginShutdown()
       ->Then(mParentDecoder->GetReader()->GetTaskQueue(), __func__, this,
              &TrackBuffer::ContinueShutdown, &TrackBuffer::ContinueShutdown);

  return p;
}

void
TrackBuffer::ContinueShutdown()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mDecoders.Length()) {
    mDecoders[0]->GetReader()->Shutdown()
                ->Then(mParentDecoder->GetReader()->GetTaskQueue(), __func__, this,
                       &TrackBuffer::ContinueShutdown, &TrackBuffer::ContinueShutdown);
    mShutdownDecoders.AppendElement(mDecoders[0]);
    mDecoders.RemoveElementAt(0);
    return;
  }

  mCurrentDecoder = nullptr;
  mInitializedDecoders.Clear();
  mParentDecoder = nullptr;

  mShutdownPromise.Resolve(true, __func__);
}

nsRefPtr<TrackBufferAppendPromise>
TrackBuffer::AppendData(LargeDataBuffer* aData, int64_t aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInitializationPromise.IsEmpty());

  DecodersToInitialize decoders(this);
  nsRefPtr<TrackBufferAppendPromise> p = mInitializationPromise.Ensure(__func__);
  bool hadInitData = mParser->HasInitData();
  bool hadCompleteInitData = mParser->HasCompleteInitData();
  nsRefPtr<LargeDataBuffer> oldInit = mParser->InitData();
  bool newInitData = mParser->IsInitSegmentPresent(aData);

  // TODO: Run more of the buffer append algorithm asynchronously.
  if (newInitData) {
    MSE_DEBUG("TrackBuffer(%p)::AppendData: New initialization segment.", this);
  } else if (!hadInitData) {
    MSE_DEBUG("TrackBuffer(%p)::AppendData: Non-init segment appended during initialization.", this);
    mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
    return p;
  }

  int64_t start = 0, end = 0;
  bool gotMedia = mParser->ParseStartAndEndTimestamps(aData, start, end);
  bool gotInit = mParser->HasCompleteInitData();

  if (newInitData) {
    if (!gotInit) {
      // We need a new decoder, but we can't initialize it yet.
      nsRefPtr<SourceBufferDecoder> decoder =
        NewDecoder(aTimestampOffset);
      // The new decoder is stored in mDecoders/mCurrentDecoder, so we
      // don't need to do anything with 'decoder'. It's only a placeholder.
      if (!decoder) {
        mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
        return p;
      }
    } else {
      if (!decoders.NewDecoder(aTimestampOffset)) {
        mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
        return p;
      }
    }
  } else if (!hadCompleteInitData && gotInit) {
    MOZ_ASSERT(mCurrentDecoder);
    // Queue pending decoder for initialization now that we have a full
    // init segment.
    decoders.AppendElement(mCurrentDecoder);
  }

  if (gotMedia) {
    if (mLastEndTimestamp &&
        (!mParser->TimestampsFuzzyEqual(start, mLastEndTimestamp.value()) ||
         mLastTimestampOffset != aTimestampOffset ||
         mDecoderPerSegment ||
         (mCurrentDecoder && mCurrentDecoder->WasTrimmed()))) {
      MSE_DEBUG("TrackBuffer(%p)::AppendData: Data last=[%lld, %lld] overlaps [%lld, %lld]",
                this, mLastStartTimestamp, mLastEndTimestamp.value(), start, end);

      if (!newInitData) {
        // This data is earlier in the timeline than data we have already
        // processed or not continuous, so we must create a new decoder
        // to handle the decoding.
        if (!hadCompleteInitData ||
            !decoders.NewDecoder(aTimestampOffset)) {
          mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
          return p;
        }
        MSE_DEBUG("TrackBuffer(%p)::AppendData: Decoder marked as initialized.", this);
        AppendDataToCurrentResource(oldInit, 0);
      }
      mLastStartTimestamp = start;
    } else {
      MSE_DEBUG("TrackBuffer(%p)::AppendData: Segment last=[%lld, %lld] [%lld, %lld]",
                this, mLastStartTimestamp, mLastEndTimestamp ? mLastEndTimestamp.value() : 0, start, end);
    }
    mLastEndTimestamp.reset();
    mLastEndTimestamp.emplace(end);
  }

  if (gotMedia && start != mAdjustedTimestamp &&
      ((start < 0 && -start < FUZZ_TIMESTAMP_OFFSET && start < mAdjustedTimestamp) ||
       (start > 0 && (start < FUZZ_TIMESTAMP_OFFSET || start < mAdjustedTimestamp)))) {
    AdjustDecodersTimestampOffset(mAdjustedTimestamp - start);
    mAdjustedTimestamp = start;
  }

  if (!AppendDataToCurrentResource(aData, end - start)) {
    mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
    return p;
  }

  if (decoders.Length()) {
    // We're going to have to wait for the decoder to initialize, the promise
    // will be resolved once initialization completes.
    return p;
  } else if (gotMedia) {
    // Tell our reader that we have more data to ensure that playback starts if
    // required when data is appended.
    mParentDecoder->GetReader()->MaybeNotifyHaveData();
  }
  mInitializationPromise.Resolve(gotMedia, __func__);
  return p;
}

bool
TrackBuffer::AppendDataToCurrentResource(LargeDataBuffer* aData, uint32_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mCurrentDecoder) {
    return false;
  }

  SourceBufferResource* resource = mCurrentDecoder->GetResource();
  int64_t appendOffset = resource->GetLength();
  resource->AppendData(aData);
  mCurrentDecoder->SetRealMediaDuration(mCurrentDecoder->GetRealMediaDuration() + aDuration);
  // XXX: For future reference: NDA call must run on the main thread.
  mCurrentDecoder->NotifyDataArrived(reinterpret_cast<const char*>(aData->Elements()),
                                     aData->Length(), appendOffset);
  mParentDecoder->NotifyBytesDownloaded();
  mParentDecoder->NotifyTimeRangesChanged();

  return true;
}

class DecoderSorter
{
public:
  bool LessThan(SourceBufferDecoder* aFirst, SourceBufferDecoder* aSecond) const
  {
    nsRefPtr<dom::TimeRanges> first = new dom::TimeRanges();
    aFirst->GetBuffered(first);

    nsRefPtr<dom::TimeRanges> second = new dom::TimeRanges();
    aSecond->GetBuffered(second);

    return first->GetStartTime() < second->GetStartTime();
  }

  bool Equals(SourceBufferDecoder* aFirst, SourceBufferDecoder* aSecond) const
  {
    nsRefPtr<dom::TimeRanges> first = new dom::TimeRanges();
    aFirst->GetBuffered(first);

    nsRefPtr<dom::TimeRanges> second = new dom::TimeRanges();
    aSecond->GetBuffered(second);

    return first->GetStartTime() == second->GetStartTime();
  }
};

bool
TrackBuffer::EvictData(double aPlaybackTime,
                       uint32_t aThreshold,
                       double* aBufferStartTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  if (!mCurrentDecoder) {
    return false;
  }

  int64_t totalSize = 0;
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    totalSize += mDecoders[i]->GetResource()->GetSize();
  }

  int64_t toEvict = totalSize - aThreshold;
  if (toEvict <= 0 || mInitializedDecoders.IsEmpty()) {
    return false;
  }

  // Get a list of initialized decoders
  nsTArray<SourceBufferDecoder*> decoders;
  decoders.AppendElements(mInitializedDecoders);

  // First try to evict data before the current play position, starting
  // with the earliest time.
  uint32_t i = 0;
  bool pastCurrentDecoder = true;
  for (; i < decoders.Length() && toEvict > 0; ++i) {
    nsRefPtr<dom::TimeRanges> buffered = new dom::TimeRanges();
    decoders[i]->GetBuffered(buffered);
    bool onCurrent = decoders[i] == mCurrentDecoder;
    if (onCurrent) {
      pastCurrentDecoder = false;
    }

    MSE_DEBUG("TrackBuffer(%p)::EvictData decoder=%u/%u threshold=%u "
              "toEvict=%lld current=%s pastCurrent=%s",
              this, i, decoders.Length(), aThreshold, toEvict,
              onCurrent ? "true" : "false",
              pastCurrentDecoder ? "true" : "false");

    if (pastCurrentDecoder &&
        !mParentDecoder->IsActiveReader(decoders[i]->GetReader())) {
      // Remove data from older decoders than the current one.
      // Don't remove data if it is currently active.
      MSE_DEBUG("TrackBuffer(%p)::EvictData evicting all before start "
                "bufferedStart=%f bufferedEnd=%f aPlaybackTime=%f size=%lld",
                this, buffered->GetStartTime(), buffered->GetEndTime(),
                aPlaybackTime, decoders[i]->GetResource()->GetSize());
      toEvict -= decoders[i]->GetResource()->EvictAll();
    } else {
      // To ensure we don't evict data past the current playback position
      // we apply a threshold of a few seconds back and evict data up to
      // that point.
      if (aPlaybackTime > MSE_EVICT_THRESHOLD_TIME) {
        double time = aPlaybackTime - MSE_EVICT_THRESHOLD_TIME;
        int64_t playbackOffset = decoders[i]->ConvertToByteOffset(time);
        MSE_DEBUG("TrackBuffer(%p)::EvictData evicting some bufferedEnd=%f"
                  "aPlaybackTime=%f time=%f, playbackOffset=%lld size=%lld",
                  this, buffered->GetEndTime(), aPlaybackTime, time,
                  playbackOffset, decoders[i]->GetResource()->GetSize());
        if (playbackOffset > 0) {
          toEvict -= decoders[i]->GetResource()->EvictData(playbackOffset,
                                                           toEvict);
        }
      }
    }
  }

  // Remove decoders that have no data in them
  for (i = 0; i < decoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> buffered = new dom::TimeRanges();
    decoders[i]->GetBuffered(buffered);
    MSE_DEBUG("TrackBuffer(%p):EvictData maybe remove empty decoders=%d "
              "size=%lld start=%f end=%f",
              this, i, decoders[i]->GetResource()->GetSize(),
              buffered->GetStartTime(), buffered->GetEndTime());
    if (decoders[i] == mCurrentDecoder
        || mParentDecoder->IsActiveReader(decoders[i]->GetReader())) {
      continue;
    }

    if (decoders[i]->GetResource()->GetSize() == 0 ||
        buffered->GetStartTime() < 0.0 ||
        buffered->GetEndTime() < 0.0) {
      MSE_DEBUG("TrackBuffer(%p):EvictData remove empty decoders=%d", this, i);
      RemoveDecoder(decoders[i]);
    }
  }

  bool evicted = toEvict < (totalSize - aThreshold);
  if (evicted) {
    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    mCurrentDecoder->GetBuffered(ranges);
    *aBufferStartTime = std::max(0.0, ranges->GetStartTime());
  }

  return evicted;
}

void
TrackBuffer::EvictBefore(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    int64_t endOffset = mInitializedDecoders[i]->ConvertToByteOffset(aTime);
    if (endOffset > 0) {
      MSE_DEBUG("TrackBuffer(%p)::EvictBefore decoder=%u offset=%lld", this, i, endOffset);
      mInitializedDecoders[i]->GetResource()->EvictBefore(endOffset);
    }
  }
}

double
TrackBuffer::Buffered(dom::TimeRanges* aRanges)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  double highestEndTime = 0;

  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mInitializedDecoders[i]->GetBuffered(r);
    if (r->Length() > 0) {
      highestEndTime = std::max(highestEndTime, r->GetEndTime());
      aRanges->Union(r, double(mParser->GetRoundingError()) / USECS_PER_S);
    }
  }

  return highestEndTime;
}

already_AddRefed<SourceBufferDecoder>
TrackBuffer::NewDecoder(int64_t aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mParentDecoder);

  DiscardCurrentDecoder();

  nsRefPtr<SourceBufferDecoder> decoder =
    mParentDecoder->CreateSubDecoder(mType, aTimestampOffset - mAdjustedTimestamp);
  if (!decoder) {
    return nullptr;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  mCurrentDecoder = decoder;
  mDecoders.AppendElement(decoder);

  mLastStartTimestamp = 0;
  mLastEndTimestamp.reset();
  mLastTimestampOffset = aTimestampOffset;

  decoder->SetTaskQueue(mTaskQueue);
  return decoder.forget();
}

bool
TrackBuffer::QueueInitializeDecoder(SourceBufferDecoder* aDecoder)
{
  if (NS_WARN_IF(!mTaskQueue)) {
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return false;
  }

  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                      &TrackBuffer::InitializeDecoder,
                                                      aDecoder);
  if (NS_FAILED(mTaskQueue->Dispatch(task))) {
    MSE_DEBUG("TrackBuffer(%p): Failed to enqueue decoder initialization task", this);
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return false;
  }
  return true;
}

void
TrackBuffer::InitializeDecoder(SourceBufferDecoder* aDecoder)
{
  if (!mParentDecoder) {
    MSE_DEBUG("TrackBuffer(%p) was shutdown. Aborting initialization.",
              this);
    return;
  }
  // ReadMetadata may block the thread waiting on data, so we must be able
  // to leave the monitor while we call it. For the rest of this function
  // we want to hold the monitor though, since we run on a different task queue
  // from the reader and interact heavily with it.
  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("TrackBuffer(%p) append was cancelled. Aborting initialization.",
              this);
    // If we reached this point, the SourceBuffer would have disconnected
    // the promise. So no need to reject it.
    return;
  }

  // We may be shut down at any time by the reader on another thread. So we need
  // to check for this each time we acquire the monitor. If that happens, we
  // need to abort immediately, because the reader has forgotten about us, and
  // important pieces of our state (like mTaskQueue) have also been torn down.
  if (mShutdown) {
    MSE_DEBUG("TrackBuffer(%p) was shut down. Aborting initialization.", this);
    RemoveDecoder(aDecoder);
    return;
  }

  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());
  MediaDecoderReader* reader = aDecoder->GetReader();
  MSE_DEBUG("TrackBuffer(%p): Initializing subdecoder %p reader %p",
            this, aDecoder, reader);

  MediaInfo mi;
  nsAutoPtr<MetadataTags> tags; // TODO: Handle metadata.
  nsresult rv;

  // HACK WARNING:
  // We only reach this point once we know that we have a complete init segment.
  // We don't want the demuxer to do a blocking read as no more data can be
  // appended while this routine is running. Marking the SourceBufferResource
  // as ended will cause any incomplete reads to abort.
  // As this decoder hasn't been initialized yet, the resource isn't yet in use
  // and so it is safe to do so.
  bool wasEnded = aDecoder->GetResource()->IsEnded();
  if (!wasEnded) {
    aDecoder->GetResource()->Ended();
  }
  {
    ReentrantMonitorAutoExit mon(mParentDecoder->GetReentrantMonitor());
    rv = reader->ReadMetadata(&mi, getter_Transfers(tags));
  }
  if (!wasEnded) {
    // Adding an empty buffer will reopen the SourceBufferResource
    nsRefPtr<LargeDataBuffer> emptyBuffer = new LargeDataBuffer;
    aDecoder->GetResource()->AppendData(emptyBuffer);
  }
  // HACK END.

  reader->SetIdle();
  if (mShutdown) {
    MSE_DEBUG("TrackBuffer(%p) was shut down while reading metadata. Aborting initialization.", this);
    return;
  }

  if (NS_SUCCEEDED(rv) && reader->IsWaitingOnCDMResource()) {
    mWaitingDecoders.AppendElement(aDecoder);
    return;
  }

  aDecoder->SetTaskQueue(nullptr);

  if (NS_FAILED(rv) || (!mi.HasVideo() && !mi.HasAudio())) {
    // XXX: Need to signal error back to owning SourceBuffer.
    MSE_DEBUG("TrackBuffer(%p): Reader %p failed to initialize rv=%x audio=%d video=%d",
              this, reader, rv, mi.HasAudio(), mi.HasVideo());
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return;
  }

  if (mi.HasVideo()) {
    MSE_DEBUG("TrackBuffer(%p): Reader %p video resolution=%dx%d",
              this, reader, mi.mVideo.mDisplay.width, mi.mVideo.mDisplay.height);
  }
  if (mi.HasAudio()) {
    MSE_DEBUG("TrackBuffer(%p): Reader %p audio sampleRate=%d channels=%d",
              this, reader, mi.mAudio.mRate, mi.mAudio.mChannels);
  }

  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                      &TrackBuffer::CompleteInitializeDecoder,
                                                      aDecoder);
  if (NS_FAILED(NS_DispatchToMainThread(task))) {
    MSE_DEBUG("TrackBuffer(%p): Failed to enqueue decoder initialization task", this);
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return;
  }
}

void
TrackBuffer::CompleteInitializeDecoder(SourceBufferDecoder* aDecoder)
{
  if (!mParentDecoder) {
    MSE_DEBUG("TrackBuffer(%p) was shutdown. Aborting initialization.",
              this);
    return;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("TrackBuffer(%p) append was cancelled. Aborting initialization.",
              this);
    // If we reached this point, the SourceBuffer would have disconnected
    // the promise. So no need to reject it.
    return;
  }

  if (mShutdown) {
    MSE_DEBUG("TrackBuffer(%p) was shut down. Aborting initialization.", this);
    RemoveDecoder(aDecoder);
    return;
  }

  if (!RegisterDecoder(aDecoder)) {
    MSE_DEBUG("TrackBuffer(%p): Reader %p not activated",
              this, aDecoder->GetReader());
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return;
  }

  int64_t duration = aDecoder->GetMediaDuration();
  if (!duration) {
    // Treat a duration of 0 as infinity
    duration = -1;
  }
  mParentDecoder->SetInitialDuration(duration);

  // Tell our reader that we have more data to ensure that playback starts if
  // required when data is appended.
  mParentDecoder->GetReader()->MaybeNotifyHaveData();

  MSE_DEBUG("TrackBuffer(%p): Reader %p activated",
            this, aDecoder->GetReader());
  mInitializationPromise.ResolveIfExists(aDecoder->GetRealMediaDuration() > 0, __func__);
}

bool
TrackBuffer::ValidateTrackFormats(const MediaInfo& aInfo)
{
  if (mInfo.HasAudio() != aInfo.HasAudio() ||
      mInfo.HasVideo() != aInfo.HasVideo()) {
    MSE_DEBUG("TrackBuffer(%p)::ValidateTrackFormats audio/video track mismatch", this);
    return false;
  }

  // TODO: Support dynamic audio format changes.
  if (mInfo.HasAudio() &&
      (mInfo.mAudio.mRate != aInfo.mAudio.mRate ||
       mInfo.mAudio.mChannels != aInfo.mAudio.mChannels)) {
    MSE_DEBUG("TrackBuffer(%p)::ValidateTrackFormats audio format mismatch", this);
    return false;
  }

  return true;
}

bool
TrackBuffer::RegisterDecoder(SourceBufferDecoder* aDecoder)
{
  mParentDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  const MediaInfo& info = aDecoder->GetReader()->GetMediaInfo();
  // Initialize the track info since this is the first decoder.
  if (mInitializedDecoders.IsEmpty()) {
    mInfo = info;
    mParentDecoder->OnTrackBufferConfigured(this, mInfo);
  }
  if (!ValidateTrackFormats(info)) {
    MSE_DEBUG("TrackBuffer(%p)::RegisterDecoder with mismatched audio/video tracks", this);
    return false;
  }
  mInitializedDecoders.AppendElement(aDecoder);
  mParentDecoder->NotifyTimeRangesChanged();
  return true;
}

void
TrackBuffer::DiscardCurrentDecoder()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  EndCurrentDecoder();
  mCurrentDecoder = nullptr;
}

void
TrackBuffer::EndCurrentDecoder()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mCurrentDecoder) {
    mCurrentDecoder->GetResource()->Ended();
  }
}

void
TrackBuffer::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mCurrentDecoder) {
    DiscardCurrentDecoder();
  }
}

bool
TrackBuffer::HasInitSegment()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  return mParser->HasCompleteInitData();
}

bool
TrackBuffer::IsReady()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  MOZ_ASSERT((mInfo.HasAudio() || mInfo.HasVideo()) || mInitializedDecoders.IsEmpty());
  return mInfo.HasAudio() || mInfo.HasVideo();
}

bool
TrackBuffer::ContainsTime(int64_t aTime, int64_t aTolerance)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mInitializedDecoders[i]->GetBuffered(r);
    if (r->Find(double(aTime) / USECS_PER_S,
                double(aTolerance) / USECS_PER_S) != dom::TimeRanges::NoIndex) {
      return true;
    }
  }

  return false;
}

void
TrackBuffer::BreakCycles()
{
  MOZ_ASSERT(NS_IsMainThread());

  for (uint32_t i = 0; i < mShutdownDecoders.Length(); ++i) {
    mShutdownDecoders[i]->BreakCycles();
  }
  mShutdownDecoders.Clear();

  // These are cleared in Shutdown()
  MOZ_ASSERT(!mDecoders.Length());
  MOZ_ASSERT(mInitializedDecoders.IsEmpty());
  MOZ_ASSERT(!mParentDecoder);
}

void
TrackBuffer::ResetDecode()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->ResetDecode();
  }
}

void
TrackBuffer::ResetParserState()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mParser->HasInitData() && !mParser->HasCompleteInitData()) {
    // We have an incomplete init segment pending. reset current parser and
    // discard the current decoder.
    mParser = ContainerParser::CreateForMIMEType(mType);
    DiscardCurrentDecoder();
  }
}

void
TrackBuffer::AbortAppendData()
{
  DiscardCurrentDecoder();
  // The SourceBuffer would have disconnected its promise.
  // However we must ensure that the MediaPromiseHolder handle all pending
  // promises.
  mInitializationPromise.RejectIfExists(NS_ERROR_ABORT, __func__);
}

const nsTArray<nsRefPtr<SourceBufferDecoder>>&
TrackBuffer::Decoders()
{
  // XXX assert OnDecodeThread
  return mInitializedDecoders;
}

#ifdef MOZ_EME
nsresult
TrackBuffer::SetCDMProxy(CDMProxy* aProxy)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    nsresult rv = mDecoders[i]->SetCDMProxy(aProxy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  for (uint32_t i = 0; i < mWaitingDecoders.Length(); ++i) {
    CDMCaps::AutoLock caps(aProxy->Capabilites());
    caps.CallOnMainThreadWhenCapsAvailable(
      NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                        &TrackBuffer::QueueInitializeDecoder,
                                                        mWaitingDecoders[i]));
  }

  mWaitingDecoders.Clear();

  return NS_OK;
}
#endif

#if defined(DEBUG)
void
TrackBuffer::Dump(const char* aPath)
{
  char path[255];
  PR_snprintf(path, sizeof(path), "%s/trackbuffer-%p", aPath, this);
  PR_MkDir(path, 0700);

  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    char buf[255];
    PR_snprintf(buf, sizeof(buf), "%s/reader-%p", path, mDecoders[i]->GetReader());
    PR_MkDir(buf, 0700);

    mDecoders[i]->GetResource()->Dump(buf);
  }
}
#endif

class DelayedDispatchToMainThread : public nsRunnable {
public:
  explicit DelayedDispatchToMainThread(SourceBufferDecoder* aDecoder)
    : mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    // Shutdown the reader, and remove its reference to the decoder
    // so that it can't accidentally read it after the decoder
    // is destroyed.
    mDecoder->GetReader()->Shutdown();
    mDecoder->GetReader()->ClearDecoder();
    RefPtr<nsIRunnable> task = new ReleaseDecoderTask(mDecoder);
    mDecoder = nullptr;
    // task now holds the only ref to the decoder.
    NS_DispatchToMainThread(task);
    return NS_OK;
  }

private:
  RefPtr<SourceBufferDecoder> mDecoder;
};

void
TrackBuffer::RemoveDecoder(SourceBufferDecoder* aDecoder)
{
  RefPtr<nsIRunnable> task = new DelayedDispatchToMainThread(aDecoder);

  {
    ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
    // There should be no other references to the decoder. Assert that
    // we aren't using it in the MediaSourceReader.
    MOZ_ASSERT(!mParentDecoder->IsActiveReader(aDecoder->GetReader()));
    mInitializedDecoders.RemoveElement(aDecoder);
    mDecoders.RemoveElement(aDecoder);

    if (mCurrentDecoder == aDecoder) {
      DiscardCurrentDecoder();
    }
  }
  aDecoder->GetReader()->GetTaskQueue()->Dispatch(task);
}

bool
TrackBuffer::RangeRemoval(int64_t aStart, int64_t aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  nsRefPtr<dom::TimeRanges> buffered = new dom::TimeRanges();
  int64_t bufferedEnd = Buffered(buffered) * USECS_PER_S;
  int64_t bufferedStart = buffered->GetStartTime() * USECS_PER_S;

  if (bufferedStart < 0 || aStart > bufferedEnd || aEnd < bufferedStart) {
    // Nothing to remove.
    return false;
  }
  if (aEnd < bufferedEnd) {
    // TODO. We only handle trimming.
    NS_WARNING("TrackBuffer::RangeRemoval unsupported arguments. "
               "Can only handle trimming");
    return false;
  }

  nsTArray<SourceBufferDecoder*> decoders;
  decoders.AppendElements(mInitializedDecoders);

  // Only trimming existing buffers.
  for (size_t i = 0; i < decoders.Length(); ++i) {
    decoders[i]->Trim(aStart);
    if (aStart <= buffered->GetStartTime()) {
      // We've completely emptied it, can clear the data.
      int64_t size = decoders[i]->GetResource()->GetSize();
      decoders[i]->GetResource()->EvictData(size, size);
      if (decoders[i] == mCurrentDecoder ||
          mParentDecoder->IsActiveReader(decoders[i]->GetReader())) {
        continue;
      }
      MSE_DEBUG("TrackBuffer(%p):RangeRemoval remove empty decoders=%d", this, i);
      RemoveDecoder(decoders[i]);
    }
  }
  return true;
}

void
TrackBuffer::AdjustDecodersTimestampOffset(int32_t aOffset)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mDecoders.Length(); i++) {
    mDecoders[i]->SetTimestampOffset(mDecoders[i]->GetTimestampOffset() + aOffset);
  }
}

} // namespace mozilla
