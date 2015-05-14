/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBuffer.h"

#include "AsyncEventRunner.h"
#include "MediaData.h"
#include "MediaSourceUtils.h"
#include "TrackBuffer.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prlog.h"
#include <time.h>
#include "TimeUnits.h"

struct JSContext;
class JSObject;

extern PRLogModuleInfo* GetMediaSourceLog();
extern PRLogModuleInfo* GetMediaSourceAPILog();

#define MSE_DEBUG(arg, ...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) PR_LOG(GetMediaSourceLog(), PR_LOG_DEBUG + 1, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))
#define MSE_API(arg, ...) PR_LOG(GetMediaSourceAPILog(), PR_LOG_DEBUG, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.get(), __func__, ##__VA_ARGS__))

namespace mozilla {

namespace dom {

class AppendDataRunnable : public nsRunnable {
public:
  AppendDataRunnable(SourceBuffer* aSourceBuffer,
                     MediaLargeByteBuffer* aData,
                     double aTimestampOffset,
                     uint32_t aUpdateID)
  : mSourceBuffer(aSourceBuffer)
  , mData(aData)
  , mTimestampOffset(aTimestampOffset)
  , mUpdateID(aUpdateID)
  {
  }

  NS_IMETHOD Run() override final {

    mSourceBuffer->AppendData(mData, mTimestampOffset, mUpdateID);

    return NS_OK;
  }

private:
  nsRefPtr<SourceBuffer> mSourceBuffer;
  nsRefPtr<MediaLargeByteBuffer> mData;
  double mTimestampOffset;
  uint32_t mUpdateID;
};

class RangeRemovalRunnable : public nsRunnable {
public:
  RangeRemovalRunnable(SourceBuffer* aSourceBuffer,
                       double aStart,
                       double aEnd)
  : mSourceBuffer(aSourceBuffer)
  , mStart(aStart)
  , mEnd(aEnd)
  { }

  NS_IMETHOD Run() override final {

    if (!mSourceBuffer->mUpdating) {
      // abort was called in between.
      return NS_OK;
    }
    mSourceBuffer->DoRangeRemoval(mStart, mEnd);
    mSourceBuffer->StopUpdating();

    return NS_OK;
  }

private:
  nsRefPtr<SourceBuffer> mSourceBuffer;
  double mStart;
  double mEnd;
};

void
SourceBuffer::SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetMode(aMode=%d)", aMode);
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (aMode == SourceBufferAppendMode::Sequence) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return;
  }
  MOZ_ASSERT(mMediaSource->ReadyState() != MediaSourceReadyState::Closed);
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  // TODO: Test append state.
  // TODO: If aMode is "sequence", set sequence start time.
  mAppendMode = aMode;
}

void
SourceBuffer::SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetTimestampOffset(aTimestampOffset=%f)", aTimestampOffset);
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  MOZ_ASSERT(mMediaSource->ReadyState() != MediaSourceReadyState::Closed);
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  // TODO: Test append state.
  // TODO: If aMode is "sequence", set sequence start time.
  mTimestampOffset = aTimestampOffset;
}

already_AddRefed<TimeRanges>
SourceBuffer::GetBuffered(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!IsAttached()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  double highestEndTime = mTrackBuffer->Buffered(ranges);
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    // Set the end time on the last range to highestEndTime by adding a
    // new range spanning the current end time to highestEndTime, which
    // Normalize() will then merge with the old last range.
    ranges->Add(ranges->GetEndTime(), highestEndTime);
    ranges->Normalize();
  }
  MSE_DEBUGV("ranges=%s", DumpTimeRanges(ranges).get());
  return ranges.forget();
}

void
SourceBuffer::SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetAppendWindowStart(aAppendWindowStart=%f)", aAppendWindowStart);
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (aAppendWindowStart < 0 || aAppendWindowStart >= mAppendWindowEnd) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  mAppendWindowStart = aAppendWindowStart;
}

void
SourceBuffer::SetAppendWindowEnd(double aAppendWindowEnd, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetAppendWindowEnd(aAppendWindowEnd=%f)", aAppendWindowEnd);
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (IsNaN(aAppendWindowEnd) ||
      aAppendWindowEnd <= mAppendWindowStart) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  mAppendWindowEnd = aAppendWindowEnd;
}

void
SourceBuffer::AppendBuffer(const ArrayBuffer& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("AppendBuffer(ArrayBuffer)");
  aData.ComputeLengthAndData();
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::AppendBuffer(const ArrayBufferView& aData, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("AppendBuffer(ArrayBufferView)");
  aData.ComputeLengthAndData();
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::Abort(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Abort()");
  if (!IsAttached()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  AbortBufferAppend();
  mTrackBuffer->ResetParserState();
  mAppendWindowStart = 0;
  mAppendWindowEnd = PositiveInfinity<double>();
  // Discard the current decoder so no new data will be added to it.
  MSE_DEBUG("Discarding decoder");
  mTrackBuffer->DiscardCurrentDecoder();
}

void
SourceBuffer::AbortBufferAppend()
{
  if (mUpdating) {
    mPendingAppend.DisconnectIfExists();
    // TODO: Abort segment parser loop, and stream append loop algorithms.
    // cancel any pending buffer append.
    mTrackBuffer->AbortAppendData();
    AbortUpdating();
  }
}

void
SourceBuffer::Remove(double aStart, double aEnd, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Remove(aStart=%f, aEnd=%f)", aStart, aEnd);
  if (!IsAttached()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (IsNaN(mMediaSource->Duration()) ||
      aStart < 0 || aStart > mMediaSource->Duration() ||
      aEnd <= aStart || IsNaN(aEnd)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }

  StartUpdating();
  nsCOMPtr<nsIRunnable> task = new RangeRemovalRunnable(this, aStart, aEnd);
  NS_DispatchToMainThread(task);
}

void
SourceBuffer::RangeRemoval(double aStart, double aEnd)
{
  StartUpdating();
  DoRangeRemoval(aStart, aEnd);
  nsCOMPtr<nsIRunnable> task =
    NS_NewRunnableMethod(this, &SourceBuffer::StopUpdating);
  NS_DispatchToMainThread(task);
}

void
SourceBuffer::DoRangeRemoval(double aStart, double aEnd)
{
  MSE_DEBUG("DoRangeRemoval(%f, %f)", aStart, aEnd);
  if (mTrackBuffer && !IsInfinite(aStart)) {
    mTrackBuffer->RangeRemoval(media::Microseconds::FromSeconds(aStart),
                               media::Microseconds::FromSeconds(aEnd));
  }
}

void
SourceBuffer::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Detach");
  AbortBufferAppend();
  if (mTrackBuffer) {
    mTrackBuffer->Detach();
  }
  mTrackBuffer = nullptr;
  mMediaSource = nullptr;
}

void
SourceBuffer::Ended()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsAttached());
  MSE_DEBUG("Ended");
  mTrackBuffer->EndCurrentDecoder();
}

SourceBuffer::SourceBuffer(MediaSource* aMediaSource, const nsACString& aType)
  : DOMEventTargetHelper(aMediaSource->GetParentObject())
  , mMediaSource(aMediaSource)
  , mAppendWindowStart(0)
  , mAppendWindowEnd(PositiveInfinity<double>())
  , mTimestampOffset(0)
  , mAppendMode(SourceBufferAppendMode::Segments)
  , mUpdating(false)
  , mActive(false)
  , mUpdateID(0)
  , mType(aType)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMediaSource);
  mEvictionThreshold = Preferences::GetUint("media.mediasource.eviction_threshold",
                                            75 * (1 << 20));
  mTrackBuffer = new TrackBuffer(aMediaSource->GetDecoder(), aType);
  MSE_DEBUG("Create mTrackBuffer=%p",
            mTrackBuffer.get());
}

SourceBuffer::~SourceBuffer()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mMediaSource);
  MSE_DEBUG("");
}

MediaSource*
SourceBuffer::GetParentObject() const
{
  return mMediaSource;
}

JSObject*
SourceBuffer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return SourceBufferBinding::Wrap(aCx, this, aGivenProto);
}

void
SourceBuffer::DispatchSimpleEvent(const char* aName)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("Dispatch event '%s'", aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBuffer::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("Queuing event '%s'", aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<SourceBuffer>(this, aName);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void
SourceBuffer::StartUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mUpdating);
  mUpdating = true;
  mUpdateID++;
  QueueAsyncSimpleEvent("updatestart");
}

void
SourceBuffer::StopUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mUpdating) {
    // The buffer append algorithm has been interrupted by abort().
    //
    // If the sequence appendBuffer(), abort(), appendBuffer() occurs before
    // the first StopUpdating() runnable runs, then a second StopUpdating()
    // runnable will be scheduled, but still only one (the first) will queue
    // events.
    return;
  }
  mUpdating = false;
  QueueAsyncSimpleEvent("update");
  QueueAsyncSimpleEvent("updateend");
}

void
SourceBuffer::AbortUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mUpdating);
  mUpdating = false;
  QueueAsyncSimpleEvent("abort");
  QueueAsyncSimpleEvent("updateend");
}

void
SourceBuffer::CheckEndTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Check if we need to update mMediaSource duration
  double endTime = GetBufferedEnd();
  double duration = mMediaSource->Duration();
  if (endTime > duration) {
    mMediaSource->SetDuration(endTime, MSRangeRemovalAction::SKIP);
  }
}

void
SourceBuffer::AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv)
{
  MSE_DEBUG("AppendData(aLength=%u)", aLength);

  nsRefPtr<MediaLargeByteBuffer> data = PrepareAppend(aData, aLength, aRv);
  if (!data) {
    return;
  }
  StartUpdating();

  MOZ_ASSERT(mAppendMode == SourceBufferAppendMode::Segments,
             "We don't handle timestampOffset for sequence mode yet");
  nsCOMPtr<nsIRunnable> task =
    new AppendDataRunnable(this, data, mTimestampOffset, mUpdateID);
  NS_DispatchToMainThread(task);
}

void
SourceBuffer::AppendData(MediaLargeByteBuffer* aData, double aTimestampOffset,
                         uint32_t aUpdateID)
{
  if (!mUpdating || aUpdateID != mUpdateID) {
    // The buffer append algorithm has been interrupted by abort().
    //
    // If the sequence appendBuffer(), abort(), appendBuffer() occurs before
    // the first StopUpdating() runnable runs, then a second StopUpdating()
    // runnable will be scheduled, but still only one (the first) will queue
    // events.
    return;
  }

  MOZ_ASSERT(mMediaSource);
  MOZ_ASSERT(!mPendingAppend.Exists());

  if (!aData->Length()) {
    StopUpdating();
    return;
  }

  mPendingAppend.Begin(mTrackBuffer->AppendData(aData, aTimestampOffset * USECS_PER_S)
                       ->RefableThen(AbstractThread::MainThread(), __func__, this,
                                     &SourceBuffer::AppendDataCompletedWithSuccess,
                                     &SourceBuffer::AppendDataErrored));
}

void
SourceBuffer::AppendDataCompletedWithSuccess(bool aGotMedia)
{
  mPendingAppend.Complete();
  if (!mUpdating) {
    // The buffer append algorithm has been interrupted by abort().
    return;
  }

  if (mTrackBuffer->HasInitSegment()) {
    if (!mActive) {
      mActive = true;
      mMediaSource->SourceBufferIsActive(this);
      mMediaSource->QueueInitializationEvent();
    }
  }

  if (aGotMedia) {
    CheckEndTime();
  }

  StopUpdating();
}

void
SourceBuffer::AppendDataErrored(nsresult aError)
{
  mPendingAppend.Complete();
  switch (aError) {
    case NS_ERROR_ABORT:
      // Nothing further to do as the trackbuffer has been shutdown.
      // or append was aborted and abort() has handled all the events.
      break;
    default:
      AppendError(true);
      break;
  }
}

void
SourceBuffer::AppendError(bool aDecoderError)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mUpdating) {
    // The buffer append algorithm has been interrupted by abort().
    return;
  }
  mTrackBuffer->ResetParserState();

  mUpdating = false;

  QueueAsyncSimpleEvent("error");
  QueueAsyncSimpleEvent("updateend");

  if (aDecoderError) {
    Optional<MediaSourceEndOfStreamError> decodeError(
      MediaSourceEndOfStreamError::Decode);
    ErrorResult dummy;
    mMediaSource->EndOfStream(decodeError, dummy);
  }
}

already_AddRefed<MediaLargeByteBuffer>
SourceBuffer::PrepareAppend(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv)
{
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }

  // Eviction uses a byte threshold. If the buffer is greater than the
  // number of bytes then data is evicted. The time range for this
  // eviction is reported back to the media source. It will then
  // evict data before that range across all SourceBuffers it knows
  // about.
  // TODO: Make the eviction threshold smaller for audio-only streams.
  // TODO: Drive evictions off memory pressure notifications.
  // TODO: Consider a global eviction threshold  rather than per TrackBuffer.
  double newBufferStartTime = 0.0;
  // Attempt to evict the amount of data we are about to add by lowering the
  // threshold.
  uint32_t toEvict =
    (mEvictionThreshold > aLength) ? mEvictionThreshold - aLength : aLength;
  bool evicted =
    mTrackBuffer->EvictData(mMediaSource->GetDecoder()->GetCurrentTime(),
                            toEvict, &newBufferStartTime);
  if (evicted) {
    MSE_DEBUG("AppendData Evict; current buffered start=%f",
              GetBufferedStart());

    // We notify that we've evicted from the time range 0 through to
    // the current start point.
    mMediaSource->NotifyEvicted(0.0, newBufferStartTime);
  }

  // See if we have enough free space to append our new data.
  // As we can only evict once we have playable data, we must give a chance
  // to the DASH player to provide a complete media segment.
  if (aLength > mEvictionThreshold ||
      ((mTrackBuffer->GetSize() > mEvictionThreshold - aLength) &&
       !mTrackBuffer->HasOnlyIncompleteMedia())) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return nullptr;
  }

  nsRefPtr<MediaLargeByteBuffer> data = new MediaLargeByteBuffer();
  if (!data->AppendElements(aData, aLength)) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return nullptr;
  }
  // TODO: Test buffer full flag.
  return data.forget();
}

double
SourceBuffer::GetBufferedStart()
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult dummy;
  nsRefPtr<TimeRanges> ranges = GetBuffered(dummy);
  return ranges->Length() > 0 ? ranges->GetStartTime() : 0;
}

double
SourceBuffer::GetBufferedEnd()
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult dummy;
  nsRefPtr<TimeRanges> ranges = GetBuffered(dummy);
  return ranges->Length() > 0 ? ranges->GetEndTime() : 0;
}

void
SourceBuffer::Evict(double aStart, double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Evict(aStart=%f, aEnd=%f)", aStart, aEnd);
  double currentTime = mMediaSource->GetDecoder()->GetCurrentTime();
  double evictTime = aEnd;
  const double safety_threshold = 5;
  if (currentTime + safety_threshold >= evictTime) {
    evictTime -= safety_threshold;
  }
  mTrackBuffer->EvictBefore(evictTime);
}

#if defined(DEBUG)
void
SourceBuffer::Dump(const char* aPath)
{
  if (mTrackBuffer) {
    mTrackBuffer->Dump(aPath);
  }
}
#endif

NS_IMPL_CYCLE_COLLECTION_CLASS(SourceBuffer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SourceBuffer)
  // Tell the TrackBuffer to end its current SourceBufferResource.
  TrackBuffer* track = tmp->mTrackBuffer;
  if (track) {
    track->Detach();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMediaSource)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SourceBuffer,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaSource)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(SourceBuffer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBuffer, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBuffer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

#undef MSE_DEBUG
#undef MSE_DEBUGV
#undef MSE_API

} // namespace dom

} // namespace mozilla
