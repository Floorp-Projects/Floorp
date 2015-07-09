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
#include "mozilla/TypeTraits.h"
#include "nsError.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"

extern PRLogModuleInfo* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("TrackBuffer(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

// Time in seconds to substract from the current time when deciding the
// time point to evict data before in a decoder. This is used to help
// prevent evicting the current playback point.
#define MSE_EVICT_THRESHOLD_TIME 2.0

// Time in microsecond under which a timestamp will be considered to be 0.
#define FUZZ_TIMESTAMP_OFFSET 100000

#define EOS_FUZZ_US 125000

using media::TimeIntervals;
using media::Interval;

namespace mozilla {

TrackBuffer::TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType)
  : mParentDecoder(aParentDecoder)
  , mType(aType)
  , mLastStartTimestamp(0)
  , mIsWaitingOnCDM(false)
  , mShutdown(false)
{
  MOZ_COUNT_CTOR(TrackBuffer);
  mParser = ContainerParser::CreateForMIMEType(aType);
  mTaskQueue =
    new MediaTaskQueue(GetMediaThreadPool(MediaThreadType::PLAYBACK));
  aParentDecoder->AddTrackBuffer(this);
  mDecoderPerSegment = Preferences::GetBool("media.mediasource.decoder-per-segment", false);
  MSE_DEBUG("TrackBuffer created for parent decoder %p", aParentDecoder);
}

TrackBuffer::~TrackBuffer()
{
  MOZ_COUNT_DTOR(TrackBuffer);
}

class MOZ_STACK_CLASS DecodersToInitialize final {
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

  bool NewDecoder(TimeUnit aTimestampOffset)
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
  mMetadataRequest.DisconnectIfExists();

  MOZ_ASSERT(mShutdownPromise.IsEmpty());
  nsRefPtr<ShutdownPromise> p = mShutdownPromise.Ensure(__func__);

  RefPtr<MediaTaskQueue> queue = mTaskQueue;
  mTaskQueue = nullptr;
  queue->BeginShutdown()
       ->Then(mParentDecoder->GetReader()->TaskQueue(), __func__, this,
              &TrackBuffer::ContinueShutdown, &TrackBuffer::ContinueShutdown);

  return p;
}

void
TrackBuffer::ContinueShutdown()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mDecoders.Length()) {
    mDecoders[0]->GetReader()->Shutdown()
                ->Then(mParentDecoder->GetReader()->TaskQueue(), __func__, this,
                       &TrackBuffer::ContinueShutdown, &TrackBuffer::ContinueShutdown);
    mShutdownDecoders.AppendElement(mDecoders[0]);
    mDecoders.RemoveElementAt(0);
    return;
  }

  MOZ_ASSERT(!mCurrentDecoder, "Detach() should have been called");
  mInitializedDecoders.Clear();
  mParentDecoder = nullptr;

  mShutdownPromise.Resolve(true, __func__);
}

bool
TrackBuffer::AppendData(MediaByteBuffer* aData, TimeUnit aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  mInputBuffer = aData;
  mTimestampOffset = aTimestampOffset;
  return true;
}

nsRefPtr<TrackBuffer::AppendPromise>
TrackBuffer::BufferAppend()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mInitializationPromise.IsEmpty());
  MOZ_ASSERT(mInputBuffer);

  if (mInputBuffer->IsEmpty()) {
    return AppendPromise::CreateAndResolve(false, __func__);
  }

  DecodersToInitialize decoders(this);
  nsRefPtr<AppendPromise> p = mInitializationPromise.Ensure(__func__);
  bool hadInitData = mParser->HasInitData();
  bool hadCompleteInitData = mParser->HasCompleteInitData();
  nsRefPtr<MediaByteBuffer> oldInit = mParser->InitData();
  bool newInitData = mParser->IsInitSegmentPresent(mInputBuffer);

  // TODO: Run more of the buffer append algorithm asynchronously.
  if (newInitData) {
    MSE_DEBUG("New initialization segment.");
  } else if (!hadInitData) {
    MSE_DEBUG("Non-init segment appended during initialization.");
    mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
    return p;
  }

  int64_t start = 0, end = 0;
  bool gotMedia = mParser->ParseStartAndEndTimestamps(mInputBuffer, start, end);
  bool gotInit = mParser->HasCompleteInitData();

  if (newInitData) {
    if (!gotInit) {
      // We need a new decoder, but we can't initialize it yet.
      nsRefPtr<SourceBufferDecoder> decoder =
        NewDecoder(mTimestampOffset);
      // The new decoder is stored in mDecoders/mCurrentDecoder, so we
      // don't need to do anything with 'decoder'. It's only a placeholder.
      if (!decoder) {
        mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
        return p;
      }
    } else {
      if (!decoders.NewDecoder(mTimestampOffset)) {
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

  mLastAppendRange = Interval<int64_t>();

  if (gotMedia) {
    if (mParser->IsMediaSegmentPresent(mInputBuffer) && mLastEndTimestamp &&
        (!mParser->TimestampsFuzzyEqual(start, mLastEndTimestamp.value()) ||
         mLastTimestampOffset != mTimestampOffset ||
         mDecoderPerSegment ||
         (mCurrentDecoder && mCurrentDecoder->WasTrimmed()))) {
      MSE_DEBUG("Data last=[%lld, %lld] overlaps [%lld, %lld]",
                mLastStartTimestamp, mLastEndTimestamp.value(), start, end);

      if (!newInitData) {
        // This data is earlier in the timeline than data we have already
        // processed or not continuous, so we must create a new decoder
        // to handle the decoding.
        if (!hadCompleteInitData ||
            !decoders.NewDecoder(mTimestampOffset)) {
          mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
          return p;
        }
        MSE_DEBUG("Decoder marked as initialized.");
        AppendDataToCurrentResource(oldInit, 0);
        mLastAppendRange = Interval<int64_t>(0, int64_t(oldInit->Length()));
      }
      mLastStartTimestamp = start;
    } else {
      MSE_DEBUG("Segment last=[%lld, %lld] [%lld, %lld]",
                mLastStartTimestamp,
                mLastEndTimestamp ? mLastEndTimestamp.value() : 0, start, end);
    }
    mLastEndTimestamp.reset();
    mLastEndTimestamp.emplace(end);
  }

  TimeUnit starttu{TimeUnit::FromMicroseconds(start)};

  if (gotMedia && starttu != mAdjustedTimestamp &&
      ((start < 0 && -start < FUZZ_TIMESTAMP_OFFSET && starttu < mAdjustedTimestamp) ||
       (start > 0 && (start < FUZZ_TIMESTAMP_OFFSET || starttu < mAdjustedTimestamp)))) {
    AdjustDecodersTimestampOffset(mAdjustedTimestamp - starttu);
    mAdjustedTimestamp = starttu;
  }

  int64_t offset = AppendDataToCurrentResource(mInputBuffer, end - start);
  if (offset < 0) {
    mInitializationPromise.Reject(NS_ERROR_FAILURE, __func__);
    return p;
  }

  mLastAppendRange = mLastAppendRange.IsEmpty()
    ? Interval<int64_t>(offset, offset + int64_t(mInputBuffer->Length()))
    : mLastAppendRange.Span(
        Interval<int64_t>(offset, offset + int64_t(mInputBuffer->Length())));

  if (decoders.Length()) {
    // We're going to have to wait for the decoder to initialize, the promise
    // will be resolved once initialization completes.
    return p;
  }

  nsRefPtr<TrackBuffer> self = this;

  ProxyMediaCall(mParentDecoder->GetReader()->TaskQueue(), this, __func__,
                 &TrackBuffer::UpdateBufferedRanges,
                 mLastAppendRange, /* aNotifyParent */ true)
      ->Then(mParentDecoder->GetReader()->TaskQueue(), __func__,
             [self] {
               self->mInitializationPromise.ResolveIfExists(self->HasInitSegment(), __func__);
             },
             [self] (nsresult) { MOZ_CRASH("Never called."); });

  return p;
}

int64_t
TrackBuffer::AppendDataToCurrentResource(MediaByteBuffer* aData, uint32_t aDuration)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mCurrentDecoder) {
    return -1;
  }

  SourceBufferResource* resource = mCurrentDecoder->GetResource();
  int64_t appendOffset = resource->GetLength();
  resource->AppendData(aData);
  mCurrentDecoder->SetRealMediaDuration(mCurrentDecoder->GetRealMediaDuration() + aDuration);

  return appendOffset;
}

nsRefPtr<TrackBuffer::BufferedRangesUpdatedPromise>
TrackBuffer::UpdateBufferedRanges(Interval<int64_t> aByteRange, bool aNotifyParent)
{
  if (aByteRange.Length()) {
    mCurrentDecoder->GetReader()->NotifyDataArrived(aByteRange);
  }

  // Recalculate and cache our new buffered range.
  {
    ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
    TimeIntervals buffered;

    for (auto& decoder : mInitializedDecoders) {
      TimeIntervals decoderBuffered(decoder->GetBuffered());
      mReadersBuffered[decoder] = decoderBuffered;
      buffered += decoderBuffered;
    }
    // mParser may not be initialized yet, and will only be so if we have a
    // buffered range.
    if (buffered.Length()) {
      buffered.SetFuzz(TimeUnit::FromMicroseconds(mParser->GetRoundingError()));
    }

    mBufferedRanges = buffered;
  }

  if (aNotifyParent) {
    nsRefPtr<MediaSourceDecoder> parent = mParentDecoder;
    nsCOMPtr<nsIRunnable> task =
      NS_NewRunnableFunction([parent] () {
        // XXX: Params make no sense to parent decoder as it relates to a
        // specific SourceBufferDecoder's data stream.  Pass bogus values here to
        // force parent decoder's state machine to recompute end time for
        // infinite length media.
        parent->NotifyDataArrived(0, 0, /* aThrottleUpdates = */ false);
        parent->NotifyBytesDownloaded();
      });
    AbstractThread::MainThread()->Dispatch(task.forget());
  }

  // Tell our reader that we have more data to ensure that playback starts if
  // required when data is appended.
  NotifyTimeRangesChanged();

  return BufferedRangesUpdatedPromise::CreateAndResolve(true, __func__);
}

void
TrackBuffer::NotifyTimeRangesChanged()
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethod(mParentDecoder->GetReader(),
                         &MediaSourceReader::NotifyTimeRangesChanged);
  mParentDecoder->GetReader()->TaskQueue()->Dispatch(task.forget());
}

void
TrackBuffer::NotifyReaderDataRemoved(MediaDecoderReader* aReader)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsRefPtr<TrackBuffer> self = this;
  nsRefPtr<MediaDecoderReader> reader = aReader;
  RefPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self, reader] () {
      reader->NotifyDataRemoved();
      self->UpdateBufferedRanges(Interval<int64_t>(), /* aNotifyParent */ false);
    });
  aReader->TaskQueue()->Dispatch(task.forget());
}

class DecoderSorter
{
public:
  explicit DecoderSorter(const TrackBuffer::DecoderBufferedMap& aMap)
    : mMap(aMap)
  {}

  bool LessThan(SourceBufferDecoder* aFirst, SourceBufferDecoder* aSecond) const
  {
    MOZ_ASSERT(mMap.find(aFirst) != mMap.end());
    MOZ_ASSERT(mMap.find(aSecond) != mMap.end());
    const TimeIntervals& first = mMap.find(aFirst)->second;
    const TimeIntervals& second = mMap.find(aSecond)->second;

    return first.GetStart() < second.GetStart();
  }

  bool Equals(SourceBufferDecoder* aFirst, SourceBufferDecoder* aSecond) const
  {
    MOZ_ASSERT(mMap.find(aFirst) != mMap.end());
    MOZ_ASSERT(mMap.find(aSecond) != mMap.end());
    const TimeIntervals& first = mMap.find(aFirst)->second;
    const TimeIntervals& second = mMap.find(aSecond)->second;

    return first.GetStart() == second.GetStart();
  }

  const TrackBuffer::DecoderBufferedMap& mMap;
};

TrackBuffer::EvictDataResult
TrackBuffer::EvictData(TimeUnit aPlaybackTime,
                       uint32_t aThreshold,
                       TimeUnit* aBufferStartTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  if (!mCurrentDecoder || mInitializedDecoders.IsEmpty()) {
    return EvictDataResult::CANT_EVICT;
  }

  int64_t totalSize = GetSize();

  int64_t toEvict = totalSize - aThreshold;
  if (toEvict <= 0) {
    return EvictDataResult::NO_DATA_EVICTED;
  }

  // Get a list of initialized decoders.
  nsTArray<nsRefPtr<SourceBufferDecoder>> decoders;
  decoders.AppendElements(mInitializedDecoders);
  const TimeUnit evictThresholdTime{TimeUnit::FromSeconds(MSE_EVICT_THRESHOLD_TIME)};

  // Find the reader currently being played with.
  SourceBufferDecoder* playingDecoder = nullptr;
  for (const auto& decoder : decoders) {
    if (mParentDecoder->IsActiveReader(decoder->GetReader())) {
      playingDecoder = decoder;
      break;
    }
  }
  TimeUnit playingDecoderStartTime{GetBuffered(playingDecoder).GetStart()};

  // First try to evict data before the current play position, starting
  // with the oldest decoder.
  for (uint32_t i = 0; i < decoders.Length() && toEvict > 0; ++i) {
    TimeIntervals buffered = GetBuffered(decoders[i]);

    MSE_DEBUG("Step1. decoder=%u/%u threshold=%u toEvict=%lld",
              i, decoders.Length(), aThreshold, toEvict);

    // To ensure we don't evict data past the current playback position
    // we apply a threshold of a few seconds back and evict data up to
    // that point.
    if (aPlaybackTime > evictThresholdTime) {
      TimeUnit time = aPlaybackTime - evictThresholdTime;
      bool isActive = decoders[i] == mCurrentDecoder ||
        mParentDecoder->IsActiveReader(decoders[i]->GetReader());
      if (!isActive && buffered.GetEnd() < time) {
        // The entire decoder is contained before our current playback time.
        // It can be fully evicted.
        MSE_DEBUG("evicting all bufferedEnd=%f "
                  "aPlaybackTime=%f time=%f, size=%lld",
                  buffered.GetEnd().ToSeconds(), aPlaybackTime.ToSeconds(),
                  time, decoders[i]->GetResource()->GetSize());
        toEvict -= decoders[i]->GetResource()->EvictAll();
      } else {
        int64_t playbackOffset =
          decoders[i]->ConvertToByteOffset(time.ToSeconds());
        MSE_DEBUG("evicting some bufferedEnd=%f "
                  "aPlaybackTime=%f time=%f, playbackOffset=%lld size=%lld",
                  buffered.GetEnd().ToSeconds(), aPlaybackTime.ToSeconds(),
                  time, playbackOffset, decoders[i]->GetResource()->GetSize());
        if (playbackOffset > 0) {
          if (decoders[i] == playingDecoder) {
            // This is an approximation only, likely pessimistic.
            playingDecoderStartTime = time;
          }
          ErrorResult rv;
          toEvict -= decoders[i]->GetResource()->EvictData(playbackOffset,
                                                           playbackOffset,
                                                           rv);
          if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            return EvictDataResult::CANT_EVICT;
          }
        }
      }
      NotifyReaderDataRemoved(decoders[i]->GetReader());
    }
  }

  // Evict all data from decoders we've likely already read from.
  for (uint32_t i = 0; i < decoders.Length() && toEvict > 0; ++i) {
    MSE_DEBUG("Step2. decoder=%u/%u threshold=%u toEvict=%lld",
              i, decoders.Length(), aThreshold, toEvict);
    if (decoders[i] == playingDecoder) {
      break;
    }
    if (decoders[i] == mCurrentDecoder) {
      continue;
    }
    // The buffered value is potentially stale should eviction occurred in
    // step 1. However this is only used for logging.
    TimeIntervals buffered = GetBuffered(decoders[i]);

    // Remove data from older decoders than the current one.
    MSE_DEBUG("evicting all "
              "bufferedStart=%f bufferedEnd=%f aPlaybackTime=%f size=%lld",
              buffered.GetStart().ToSeconds(), buffered.GetEnd().ToSeconds(),
              aPlaybackTime, decoders[i]->GetResource()->GetSize());
    toEvict -= decoders[i]->GetResource()->EvictAll();
    NotifyReaderDataRemoved(decoders[i]->GetReader());
  }

  // Evict all data from future decoders, starting furthest away from
  // current playback position.
  // We will ignore the currently playing decoder and the one playing after that
  // in order to ensure we give enough time to the DASH player to re-buffer
  // as necessary.
  // TODO: This step should be done using RangeRemoval:
  // Something like: RangeRemoval(aPlaybackTime + 60s, End);

  // Find the next decoder we're likely going to play with.
  nsRefPtr<SourceBufferDecoder> nextPlayingDecoder = nullptr;
  if (playingDecoder) {
    // The buffered value is potentially stale should eviction occurred in
    // step 1. However step 1 modified the start of the range value, and now
    // will use the end value.
    TimeIntervals buffered = GetBuffered(playingDecoder);
    nextPlayingDecoder =
      mParentDecoder->GetReader()->SelectDecoder(buffered.GetEnd().ToMicroseconds() + 1,
                                                 EOS_FUZZ_US,
                                                 this);
  }

  // Sort decoders by their start times.
  decoders.Sort(DecoderSorter{mReadersBuffered});

  for (int32_t i = int32_t(decoders.Length()) - 1; i >= 0 && toEvict > 0; --i) {
    MSE_DEBUG("Step3. decoder=%u/%u threshold=%u toEvict=%lld",
              i, decoders.Length(), aThreshold, toEvict);
    if (decoders[i] == playingDecoder || decoders[i] == nextPlayingDecoder ||
        decoders[i] == mCurrentDecoder) {
      continue;
    }
    // The buffered value is potentially stale should eviction occurred in
    // step 1 and 2. However step 3 is a last resort step where we will remove
    // all content and the buffered value is only used for logging.
    TimeIntervals buffered = GetBuffered(decoders[i]);

    MSE_DEBUG("evicting all "
              "bufferedStart=%f bufferedEnd=%f aPlaybackTime=%f size=%lld",
              buffered.GetStart().ToSeconds(), buffered.GetEnd().ToSeconds(),
              aPlaybackTime, decoders[i]->GetResource()->GetSize());
    toEvict -= decoders[i]->GetResource()->EvictAll();
    NotifyReaderDataRemoved(decoders[i]->GetReader());
  }

  RemoveEmptyDecoders(decoders);

  bool evicted = toEvict < (totalSize - aThreshold);
  if (evicted) {
    if (playingDecoder) {
      *aBufferStartTime =
        std::max(TimeUnit::FromSeconds(0), playingDecoderStartTime);
    } else {
      // We do not currently have data to play yet.
      // Avoid evicting anymore data to minimize rebuffering time.
      *aBufferStartTime = TimeUnit::FromSeconds(0.0);
    }
  }

  return evicted ?
    EvictDataResult::DATA_EVICTED :
    (HasOnlyIncompleteMedia() ? EvictDataResult::CANT_EVICT : EvictDataResult::NO_DATA_EVICTED);
}

void
TrackBuffer::RemoveEmptyDecoders(const nsTArray<nsRefPtr<mozilla::SourceBufferDecoder>>& aDecoders)
{
  nsRefPtr<TrackBuffer> self = this;
  nsTArray<nsRefPtr<mozilla::SourceBufferDecoder>> decoders(aDecoders);
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableFunction([self, decoders] () {
      if (!self->mParentDecoder) {
        return;
      }
      ReentrantMonitorAutoEnter mon(self->mParentDecoder->GetReentrantMonitor());

      // Remove decoders that have decoders data in them
      for (uint32_t i = 0; i < decoders.Length(); ++i) {
        if (decoders[i] == self->mCurrentDecoder ||
            self->mParentDecoder->IsActiveReader(decoders[i]->GetReader())) {
          continue;
        }
        TimeIntervals buffered = self->GetBuffered(decoders[i]);
        if (decoders[i]->GetResource()->GetSize() == 0 || !buffered.Length() ||
            buffered[0].IsEmpty()) {
          self->RemoveDecoder(decoders[i]);
        }
      }
    });
  AbstractThread::MainThread()->Dispatch(task.forget());
}

int64_t
TrackBuffer::GetSize()
{
  int64_t totalSize = 0;
  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    totalSize += mInitializedDecoders[i]->GetResource()->GetSize();
  }
  return totalSize;
}

bool
TrackBuffer::HasOnlyIncompleteMedia()
{
  if (!mCurrentDecoder) {
    return false;
  }
  TimeIntervals buffered = GetBuffered(mCurrentDecoder);
  MSE_DEBUG("mCurrentDecoder.size=%lld, start=%f end=%f",
            mCurrentDecoder->GetResource()->GetSize(),
            buffered.GetStart(), buffered.GetEnd());
  return mCurrentDecoder->GetResource()->GetSize() && !buffered.Length();
}

void
TrackBuffer::EvictBefore(TimeUnit aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    int64_t endOffset = mInitializedDecoders[i]->ConvertToByteOffset(aTime.ToSeconds());
    if (endOffset > 0) {
      MSE_DEBUG("decoder=%u offset=%lld",
                i, endOffset);
      ErrorResult rv;
      mInitializedDecoders[i]->GetResource()->EvictBefore(endOffset, rv);
      if (NS_WARN_IF(rv.Failed())) {
        rv.SuppressException();
        return;
      }
      NotifyReaderDataRemoved(mInitializedDecoders[i]->GetReader());
    }
  }
}

TimeIntervals
TrackBuffer::Buffered()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  return mBufferedRanges;
}

TimeIntervals
TrackBuffer::GetBuffered(SourceBufferDecoder* aDecoder)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  DecoderBufferedMap::const_iterator val = mReadersBuffered.find(aDecoder);

  if (val == mReadersBuffered.end()) {
    return TimeIntervals::Invalid();
  }
  return val->second;
}

already_AddRefed<SourceBufferDecoder>
TrackBuffer::NewDecoder(TimeUnit aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mParentDecoder);

  DiscardCurrentDecoder();

  nsRefPtr<SourceBufferDecoder> decoder =
    mParentDecoder->CreateSubDecoder(mType, (aTimestampOffset - mAdjustedTimestamp).ToMicroseconds());
  if (!decoder) {
    return nullptr;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  mCurrentDecoder = decoder;
  mDecoders.AppendElement(decoder);

  mLastStartTimestamp = 0;
  mLastEndTimestamp.reset();
  mLastTimestampOffset = aTimestampOffset;

  decoder->SetTaskQueue(decoder->GetReader()->TaskQueue());
  return decoder.forget();
}

bool
TrackBuffer::QueueInitializeDecoder(SourceBufferDecoder* aDecoder)
{
  // Bug 1153295: We must ensure that the nsIRunnable hold a strong reference
  // to aDecoder.
  static_assert(mozilla::IsBaseOf<nsISupports, SourceBufferDecoder>::value,
                "SourceBufferDecoder must be inheriting from nsISupports");
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                      &TrackBuffer::InitializeDecoder,
                                                      aDecoder);
  // We need to initialize the reader on its own task queue
  aDecoder->GetReader()->TaskQueue()->Dispatch(task.forget());
  return true;
}

// MetadataRecipient is a is used to pass extra values required by the
// MetadataPromise's target methods
class MetadataRecipient {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MetadataRecipient);

  MetadataRecipient(TrackBuffer* aOwner,
                    SourceBufferDecoder* aDecoder,
                    bool aWasEnded)
    : mOwner(aOwner)
    , mDecoder(aDecoder)
    , mWasEnded(aWasEnded) { }

  void OnMetadataRead(MetadataHolder* aMetadata)
  {
    mOwner->OnMetadataRead(aMetadata, mDecoder, mWasEnded);
  }

  void OnMetadataNotRead(ReadMetadataFailureReason aReason)
  {
    mOwner->OnMetadataNotRead(aReason, mDecoder);
  }

private:
  ~MetadataRecipient() {}
  nsRefPtr<TrackBuffer> mOwner;
  nsRefPtr<SourceBufferDecoder> mDecoder;
  bool mWasEnded;
};

void
TrackBuffer::InitializeDecoder(SourceBufferDecoder* aDecoder)
{
  if (!mParentDecoder) {
    MSE_DEBUG("decoder was shutdown. Aborting initialization.");
    return;
  }
  // ReadMetadata may block the thread waiting on data, so we must be able
  // to leave the monitor while we call it. For the rest of this function
  // we want to hold the monitor though, since we run on a different task queue
  // from the reader and interact heavily with it.
  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("append was cancelled. Aborting initialization.");
    // If we reached this point, the SourceBuffer would have disconnected
    // the promise. So no need to reject it.
    return;
  }

  // We may be shut down at any time by the reader on another thread. So we need
  // to check for this each time we acquire the monitor. If that happens, we
  // need to abort immediately, because the reader has forgotten about us, and
  // important pieces of our state (like mTaskQueue) have also been torn down.
  if (mShutdown) {
    MSE_DEBUG("was shut down. Aborting initialization.");
    return;
  }

  MOZ_ASSERT(aDecoder->GetReader()->OnTaskQueue());

  MediaDecoderReader* reader = aDecoder->GetReader();

  MSE_DEBUG("Initializing subdecoder %p reader %p",
            aDecoder, reader);

  reader->NotifyDataArrived(mLastAppendRange);

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
  nsRefPtr<MetadataRecipient> recipient =
    new MetadataRecipient(this, aDecoder, wasEnded);
  nsRefPtr<MediaDecoderReader::MetadataPromise> promise;
  {
    ReentrantMonitorAutoExit mon(mParentDecoder->GetReentrantMonitor());
    promise = reader->AsyncReadMetadata();
  }

  if (mShutdown) {
    MSE_DEBUG("was shut down while reading metadata. Aborting initialization.");
    return;
  }
  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("append was cancelled. Aborting initialization.");
    return;
  }

  mMetadataRequest.Begin(promise->Then(reader->TaskQueue(), __func__,
                                       recipient.get(),
                                       &MetadataRecipient::OnMetadataRead,
                                       &MetadataRecipient::OnMetadataNotRead));
}

void
TrackBuffer::OnMetadataRead(MetadataHolder* aMetadata,
                            SourceBufferDecoder* aDecoder,
                            bool aWasEnded)
{
  MOZ_ASSERT(aDecoder->GetReader()->OnTaskQueue());

  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  mMetadataRequest.Complete();

  if (mShutdown) {
    MSE_DEBUG("was shut down while reading metadata. Aborting initialization.");
    return;
  }
  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("append was cancelled. Aborting initialization.");
    return;
  }

  // Adding an empty buffer will reopen the SourceBufferResource
  if (!aWasEnded) {
    nsRefPtr<MediaByteBuffer> emptyBuffer = new MediaByteBuffer;
    aDecoder->GetResource()->AppendData(emptyBuffer);
  }
  // HACK END.

  MediaDecoderReader* reader = aDecoder->GetReader();
  reader->SetIdle();

  if (reader->IsWaitingOnCDMResource()) {
    mIsWaitingOnCDM = true;
  }

  aDecoder->SetTaskQueue(nullptr);

  // A MediaDataPromise is only resolved if MediaInfo.HasValidMedia() is true.
  MediaInfo mi = aMetadata->mInfo;

  if (mi.HasVideo()) {
    MSE_DEBUG("Reader %p video resolution=%dx%d",
              reader, mi.mVideo.mDisplay.width, mi.mVideo.mDisplay.height);
  }
  if (mi.HasAudio()) {
    MSE_DEBUG("Reader %p audio sampleRate=%d channels=%d",
              reader, mi.mAudio.mRate, mi.mAudio.mChannels);
  }

  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                      &TrackBuffer::CompleteInitializeDecoder,
                                                      aDecoder);
  if (NS_FAILED(NS_DispatchToMainThread(task))) {
    MSE_DEBUG("Failed to enqueue decoder initialization task");
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return;
  }
}

void
TrackBuffer::OnMetadataNotRead(ReadMetadataFailureReason aReason,
                               SourceBufferDecoder* aDecoder)
{
  MOZ_ASSERT(aDecoder->GetReader()->TaskQueue()->IsCurrentThreadIn());

  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  mMetadataRequest.Complete();

  if (mShutdown) {
    MSE_DEBUG("was shut down while reading metadata. Aborting initialization.");
    return;
  }
  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("append was cancelled. Aborting initialization.");
    return;
  }

  MediaDecoderReader* reader = aDecoder->GetReader();
  reader->SetIdle();

  aDecoder->SetTaskQueue(nullptr);

  MSE_DEBUG("Reader %p failed to initialize", reader);

  RemoveDecoder(aDecoder);
  mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
}

void
TrackBuffer::CompleteInitializeDecoder(SourceBufferDecoder* aDecoder)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!mParentDecoder) {
    MSE_DEBUG("was shutdown. Aborting initialization.");
    return;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mCurrentDecoder != aDecoder) {
    MSE_DEBUG("append was cancelled. Aborting initialization.");
    // If we reached this point, the SourceBuffer would have disconnected
    // the promise. So no need to reject it.
    return;
  }

  if (mShutdown) {
    MSE_DEBUG("was shut down. Aborting initialization.");
    return;
  }

  if (!RegisterDecoder(aDecoder)) {
    MSE_DEBUG("Reader %p not activated",
              aDecoder->GetReader());
    RemoveDecoder(aDecoder);
    mInitializationPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
    return;
  }

  int64_t duration = mInfo.mMetadataDuration.isSome()
    ? mInfo.mMetadataDuration.ref().ToMicroseconds() : -1;
  if (!duration) {
    // Treat a duration of 0 as infinity
    duration = -1;
  }
  mParentDecoder->SetInitialDuration(duration);

  // Tell our reader that we have more data to ensure that playback starts if
  // required when data is appended.
  NotifyTimeRangesChanged();

  MSE_DEBUG("Reader %p activated",
            aDecoder->GetReader());
  nsRefPtr<TrackBuffer> self = this;
  ProxyMediaCall(mParentDecoder->GetReader()->TaskQueue(), this, __func__,
                 &TrackBuffer::UpdateBufferedRanges,
                 Interval<int64_t>(), /* aNotifyParent */ true)
      ->Then(mParentDecoder->GetReader()->TaskQueue(), __func__,
             [self] {
               self->mInitializationPromise.ResolveIfExists(self->HasInitSegment(), __func__);
             },
             [self] (nsresult) { MOZ_CRASH("Never called."); });
}

bool
TrackBuffer::ValidateTrackFormats(const MediaInfo& aInfo)
{
  if (mInfo.HasAudio() != aInfo.HasAudio() ||
      mInfo.HasVideo() != aInfo.HasVideo()) {
    MSE_DEBUG("audio/video track mismatch");
    return false;
  }

  // TODO: Support dynamic audio format changes.
  if (mInfo.HasAudio() &&
      (mInfo.mAudio.mRate != aInfo.mAudio.mRate ||
       mInfo.mAudio.mChannels != aInfo.mAudio.mChannels)) {
    MSE_DEBUG("audio format mismatch");
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
    MSE_DEBUG("mismatched audio/video tracks");
    return false;
  }
  mInitializedDecoders.AppendElement(aDecoder);
  NotifyTimeRangesChanged();
  return true;
}

void
TrackBuffer::DiscardCurrentDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());
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
TrackBuffer::IsWaitingOnCDMResource()
{
  return mIsWaitingOnCDM;
}

bool
TrackBuffer::ContainsTime(int64_t aTime, int64_t aTolerance)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  TimeUnit time{TimeUnit::FromMicroseconds(aTime)};
  for (auto& decoder : mInitializedDecoders) {
    TimeIntervals r = GetBuffered(decoder);
    r.SetFuzz(TimeUnit::FromMicroseconds(aTolerance));
    if (r.Contains(time)) {
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
TrackBuffer::ResetParserState()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mParser->HasInitData() && !mParser->HasCompleteInitData()) {
    // We have an incomplete init segment pending. reset current parser and
    // discard the current decoder.
    mParser = ContainerParser::CreateForMIMEType(mType);
    DiscardCurrentDecoder();
  }
  mInputBuffer = nullptr;
}

void
TrackBuffer::AbortAppendData()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  nsRefPtr<SourceBufferDecoder> current = mCurrentDecoder;
  DiscardCurrentDecoder();

  if (mMetadataRequest.Exists() || !mInitializationPromise.IsEmpty()) {
    MOZ_ASSERT(current);
    RemoveDecoder(current);
  }
  // The SourceBuffer would have disconnected its promise.
  // However we must ensure that the MediaPromiseHolder handle all pending
  // promises.
  mInitializationPromise.RejectIfExists(NS_ERROR_ABORT, __func__);
}

const nsTArray<nsRefPtr<SourceBufferDecoder>>&
TrackBuffer::Decoders()
{
  // XXX assert OnDecodeTaskQueue
  return mInitializedDecoders;
}

#ifdef MOZ_EME
nsresult
TrackBuffer::SetCDMProxy(CDMProxy* aProxy)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  for (auto& decoder : mDecoders) {
    decoder->SetCDMProxy(aProxy);
  }

  mIsWaitingOnCDM = false;
  mParentDecoder->NotifyWaitingForResourcesStatusChanged();
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

class ReleaseDecoderTask : public nsRunnable {
public:
  explicit ReleaseDecoderTask(SourceBufferDecoder* aDecoder)
    : mDecoder(aDecoder)
  {
  }

  NS_IMETHOD Run() override final {
    mDecoder->GetReader()->BreakCycles();
    mDecoder = nullptr;
    return NS_OK;
  }

private:
  nsRefPtr<SourceBufferDecoder> mDecoder;
};

class DelayedDispatchToMainThread : public nsRunnable {
public:
  DelayedDispatchToMainThread(SourceBufferDecoder* aDecoder, TrackBuffer* aTrackBuffer)
    : mDecoder(aDecoder)
    , mTrackBuffer(aTrackBuffer)
  {
  }

  NS_IMETHOD Run() override final {
    // Shutdown the reader, and remove its reference to the decoder
    // so that it can't accidentally read it after the decoder
    // is destroyed.
    mDecoder->GetReader()->Shutdown();
    RefPtr<nsIRunnable> task = new ReleaseDecoderTask(mDecoder);
    mDecoder = nullptr;
    // task now holds the only ref to the decoder.
    NS_DispatchToMainThread(task);
    return NS_OK;
  }

private:
  nsRefPtr<SourceBufferDecoder> mDecoder;
  nsRefPtr<TrackBuffer> mTrackBuffer;
};

void
TrackBuffer::RemoveDecoder(SourceBufferDecoder* aDecoder)
{
  MSE_DEBUG("TrackBuffer(%p)::RemoveDecoder(%p, %p)", this, aDecoder, aDecoder->GetReader());
  RefPtr<nsIRunnable> task = new DelayedDispatchToMainThread(aDecoder, this);
  {
    ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
    // There should be no other references to the decoder. Assert that
    // we aren't using it in the MediaSourceReader.
    MOZ_ASSERT(!mParentDecoder->IsActiveReader(aDecoder->GetReader()));
    mInitializedDecoders.RemoveElement(aDecoder);
    mDecoders.RemoveElement(aDecoder);
    // Remove associated buffered range from our cache.
    mReadersBuffered.erase(aDecoder);
  }
  aDecoder->GetReader()->TaskQueue()->Dispatch(task.forget());
}

nsRefPtr<TrackBuffer::RangeRemovalPromise>
TrackBuffer::RangeRemoval(TimeUnit aStart, TimeUnit aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());

  TimeIntervals buffered = Buffered();
  TimeUnit bufferedStart = buffered.GetStart();
  TimeUnit bufferedEnd = buffered.GetEnd();

  if (!buffered.Length() || aStart > bufferedEnd || aEnd < bufferedStart) {
    // Nothing to remove.
    return RangeRemovalPromise::CreateAndResolve(false, __func__);
  }

  if (aStart > bufferedStart && aEnd < bufferedEnd) {
    // TODO. We only handle trimming and removal from the start.
    NS_WARNING("RangeRemoval unsupported arguments. "
               "Can only handle trimming (trim left or trim right");
    return RangeRemovalPromise::CreateAndResolve(false, __func__);
  }

  nsTArray<nsRefPtr<SourceBufferDecoder>> decoders;
  decoders.AppendElements(mInitializedDecoders);

  if (aStart <= bufferedStart && aEnd < bufferedEnd) {
    // Evict data from beginning.
    for (size_t i = 0; i < decoders.Length(); ++i) {
      TimeIntervals buffered = GetBuffered(decoders[i]);
      if (buffered.GetEnd() < aEnd) {
        // Can be fully removed.
        MSE_DEBUG("remove all bufferedEnd=%f size=%lld",
                  buffered.GetEnd().ToSeconds(),
                  decoders[i]->GetResource()->GetSize());
        decoders[i]->GetResource()->EvictAll();
      } else {
        int64_t offset = decoders[i]->ConvertToByteOffset(aEnd.ToSeconds());
        MSE_DEBUG("removing some bufferedEnd=%f offset=%lld size=%lld",
                  buffered.GetEnd().ToSeconds(), offset,
                  decoders[i]->GetResource()->GetSize());
        if (offset > 0) {
          ErrorResult rv;
          decoders[i]->GetResource()->EvictData(offset, offset, rv);
          if (NS_WARN_IF(rv.Failed())) {
            rv.SuppressException();
            return RangeRemovalPromise::CreateAndResolve(false, __func__);
          }
        }
      }
      NotifyReaderDataRemoved(decoders[i]->GetReader());
    }
  } else {
    // Only trimming existing buffers.
    for (size_t i = 0; i < decoders.Length(); ++i) {
      if (aStart <= buffered.GetStart()) {
        // It will be entirely emptied, can clear all data.
        decoders[i]->GetResource()->EvictAll();
      } else {
        decoders[i]->Trim(aStart.ToMicroseconds());
      }
      NotifyReaderDataRemoved(decoders[i]->GetReader());
    }
  }

  RemoveEmptyDecoders(decoders);

  nsRefPtr<RangeRemovalPromise> p = mRangeRemovalPromise.Ensure(__func__);

  // Make sure our buffered ranges got updated before resolving promise.
  nsRefPtr<TrackBuffer> self = this;
  ProxyMediaCall(mParentDecoder->GetReader()->TaskQueue(), this, __func__,
                 &TrackBuffer::UpdateBufferedRanges,
                 Interval<int64_t>(), /* aNotifyParent */ false)
    ->Then(mParentDecoder->GetReader()->TaskQueue(), __func__,
           [self] {
             self->mRangeRemovalPromise.ResolveIfExists(true, __func__);
           },
           [self] (nsresult) { MOZ_CRASH("Never called."); });

  return p;
}

void
TrackBuffer::AdjustDecodersTimestampOffset(TimeUnit aOffset)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mDecoders.Length(); i++) {
    mDecoders[i]->SetTimestampOffset(mDecoders[i]->GetTimestampOffset() + aOffset.ToMicroseconds());
  }
}

#undef MSE_DEBUG

} // namespace mozilla
