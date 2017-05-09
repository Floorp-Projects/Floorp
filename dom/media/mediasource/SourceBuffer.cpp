/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBuffer.h"

#include "AsyncEventRunner.h"
#include "MediaData.h"
#include "MediaSourceDemuxer.h"
#include "MediaSourceUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "mozilla/Logging.h"
#include <time.h>
#include "TimeUnits.h"

// GetCurrentTime is defined in winbase.h as zero argument macro forwarding to
// GetTickCount() and conflicts with MediaDecoder::GetCurrentTime implementation.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

struct JSContext;
class JSObject;

extern mozilla::LogModule* GetMediaSourceLog();
extern mozilla::LogModule* GetMediaSourceAPILog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.OriginalString().Data(), __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.OriginalString().Data(), __func__, ##__VA_ARGS__))
#define MSE_API(arg, ...) MOZ_LOG(GetMediaSourceAPILog(), mozilla::LogLevel::Debug, ("SourceBuffer(%p:%s)::%s: " arg, this, mType.OriginalString().Data(), __func__, ##__VA_ARGS__))

namespace mozilla {

using media::TimeUnit;
typedef SourceBufferAttributes::AppendState AppendState;

namespace dom {

void
SourceBuffer::SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_API("SetMode(aMode=%" PRIu32 ")", static_cast<uint32_t>(aMode));
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mCurrentAttributes.mGenerateTimestamps &&
      aMode == SourceBufferAppendMode::Segments) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }
  MOZ_ASSERT(mMediaSource->ReadyState() != MediaSourceReadyState::Closed);
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  if (mCurrentAttributes.GetAppendState() == AppendState::PARSING_MEDIA_SEGMENT){
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  if (aMode == SourceBufferAppendMode::Sequence) {
    // Will set GroupStartTimestamp to GroupEndTimestamp.
    mCurrentAttributes.RestartGroupStartTimestamp();
  }

  mCurrentAttributes.SetAppendMode(aMode);
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
  if (mCurrentAttributes.GetAppendState() == AppendState::PARSING_MEDIA_SEGMENT){
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  mCurrentAttributes.SetApparentTimestampOffset(aTimestampOffset);
  if (mCurrentAttributes.GetAppendMode() == SourceBufferAppendMode::Sequence) {
    mCurrentAttributes.SetGroupStartTimestamp(mCurrentAttributes.GetTimestampOffset());
  }
}

TimeRanges*
SourceBuffer::GetBuffered(ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  // http://w3c.github.io/media-source/index.html#widl-SourceBuffer-buffered
  // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
  if (!IsAttached()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  bool rangeChanged = true;
  media::TimeIntervals intersection = mTrackBuffersManager->Buffered();
  MSE_DEBUGV("intersection=%s", DumpTimeRanges(intersection).get());
  if (mBuffered) {
    media::TimeIntervals currentValue(mBuffered);
    rangeChanged = (intersection != currentValue);
    MSE_DEBUGV("currentValue=%s", DumpTimeRanges(currentValue).get());
  }
  // 5. If intersection ranges does not contain the exact same range information as the current value of this attribute, then update the current value of this attribute to intersection ranges.
  if (rangeChanged) {
    mBuffered = new TimeRanges(ToSupports(this));
    intersection.ToTimeRanges(mBuffered);
  }
  // 6. Return the current value of this attribute.
  return mBuffered;
}

media::TimeIntervals
SourceBuffer::GetTimeIntervals()
{
  return mTrackBuffersManager->Buffered();
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
  if (aAppendWindowStart < 0 ||
      aAppendWindowStart >= mCurrentAttributes.GetAppendWindowEnd()) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }
  mCurrentAttributes.SetAppendWindowStart(aAppendWindowStart);
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
      aAppendWindowEnd <= mCurrentAttributes.GetAppendWindowStart()) {
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }
  mCurrentAttributes.SetAppendWindowEnd(aAppendWindowEnd);
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
  if (mPendingRemoval.Exists()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  AbortBufferAppend();
  ResetParserState();
  mCurrentAttributes.SetAppendWindowStart(0);
  mCurrentAttributes.SetAppendWindowEnd(PositiveInfinity<double>());
}

void
SourceBuffer::AbortBufferAppend()
{
  if (mUpdating) {
    mCompletionPromise.DisconnectIfExists();
    if (mPendingAppend.Exists()) {
      mPendingAppend.Disconnect();
      mTrackBuffersManager->AbortAppendData();
    }
    AbortUpdating();
  }
}

void
SourceBuffer::ResetParserState()
{
  mTrackBuffersManager->ResetParserState(mCurrentAttributes);
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
    aRv.Throw(NS_ERROR_DOM_TYPE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }

  RangeRemoval(aStart, aEnd);
}

void
SourceBuffer::RangeRemoval(double aStart, double aEnd)
{
  StartUpdating();

  RefPtr<SourceBuffer> self = this;
    mTrackBuffersManager->RangeRemoval(TimeUnit::FromSeconds(aStart),
                                       TimeUnit::FromSeconds(aEnd))
      ->Then(mAbstractMainThread, __func__,
             [self] (bool) {
               self->mPendingRemoval.Complete();
               self->StopUpdating();
             },
             []() { MOZ_ASSERT(false); })
      ->Track(mPendingRemoval);
}

void
SourceBuffer::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  MSE_DEBUG("Detach");
  if (!mMediaSource) {
    MSE_DEBUG("Already detached");
    return;
  }
  AbortBufferAppend();
  if (mTrackBuffersManager) {
    mTrackBuffersManager->Detach();
    mMediaSource->GetDecoder()->GetDemuxer()->DetachSourceBuffer(
      mTrackBuffersManager.get());
  }
  mTrackBuffersManager = nullptr;
  mMediaSource = nullptr;
}

void
SourceBuffer::Ended()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(IsAttached());
  MSE_DEBUG("Ended");
  mTrackBuffersManager->Ended();
}

SourceBuffer::SourceBuffer(MediaSource* aMediaSource,
                           const MediaContainerType& aType)
  : DOMEventTargetHelper(aMediaSource->GetParentObject())
  , mMediaSource(aMediaSource)
  , mAbstractMainThread(aMediaSource->AbstractMainThread())
  , mCurrentAttributes(aType.Type() == MEDIAMIMETYPE("audio/mpeg") ||
                       aType.Type() == MEDIAMIMETYPE("audio/aac"))
  , mUpdating(false)
  , mActive(false)
  , mType(aType)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMediaSource);

  mTrackBuffersManager =
    new TrackBuffersManager(aMediaSource->GetDecoder(), aType);

  MSE_DEBUG("Create mTrackBuffersManager=%p",
            mTrackBuffersManager.get());

  ErrorResult dummy;
  if (mCurrentAttributes.mGenerateTimestamps) {
    SetMode(SourceBufferAppendMode::Sequence, dummy);
  } else {
    SetMode(SourceBufferAppendMode::Segments, dummy);
  }
  mMediaSource->GetDecoder()->GetDemuxer()->AttachSourceBuffer(
    mTrackBuffersManager.get());
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
  mAbstractMainThread->Dispatch(event.forget());
}

void
SourceBuffer::StartUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mUpdating);
  mUpdating = true;
  QueueAsyncSimpleEvent("updatestart");
}

void
SourceBuffer::StopUpdating()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mUpdating) {
    // The buffer append or range removal algorithm  has been interrupted by
    // abort().
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
  mUpdating = false;
  QueueAsyncSimpleEvent("abort");
  QueueAsyncSimpleEvent("updateend");
}

void
SourceBuffer::CheckEndTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Check if we need to update mMediaSource duration
  double endTime = mCurrentAttributes.GetGroupEndTimestamp().ToSeconds();
  double duration = mMediaSource->Duration();
  if (endTime > duration) {
    mMediaSource->SetDuration(endTime);
  }
}

void
SourceBuffer::AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv)
{
  MSE_DEBUG("AppendData(aLength=%u)", aLength);

  RefPtr<MediaByteBuffer> data = PrepareAppend(aData, aLength, aRv);
  if (!data) {
    return;
  }
  StartUpdating();

  mTrackBuffersManager->AppendData(data.forget(), mCurrentAttributes)
    ->Then(mAbstractMainThread, __func__, this,
           &SourceBuffer::AppendDataCompletedWithSuccess,
           &SourceBuffer::AppendDataErrored)
    ->Track(mPendingAppend);
}

void
SourceBuffer::AppendDataCompletedWithSuccess(const SourceBufferTask::AppendBufferResult& aResult)
{
  MOZ_ASSERT(mUpdating);
  mPendingAppend.Complete();

  if (aResult.first()) {
    if (!mActive) {
      mActive = true;
      MSE_DEBUG("Init segment received");
      RefPtr<SourceBuffer> self = this;
      mMediaSource->SourceBufferIsActive(this)
        ->Then(mAbstractMainThread, __func__,
               [self, this]() {
                 MSE_DEBUG("Complete AppendBuffer operation");
                 mCompletionPromise.Complete();
                 StopUpdating();
               })
        ->Track(mCompletionPromise);
    }
  }
  if (mActive) {
    // Tell our parent decoder that we have received new data.
    mMediaSource->GetDecoder()->NotifyDataArrived();
    // Send progress event.
    mMediaSource->GetDecoder()->NotifyBytesDownloaded();
  }

  mCurrentAttributes = aResult.second();

  CheckEndTime();

  if (!mCompletionPromise.Exists()) {
    StopUpdating();
  }
}

void
SourceBuffer::AppendDataErrored(const MediaResult& aError)
{
  MOZ_ASSERT(mUpdating);
  mPendingAppend.Complete();

  switch (aError.Code()) {
    case NS_ERROR_DOM_MEDIA_CANCELED:
      // Nothing further to do as the trackbuffer has been shutdown.
      // or append was aborted and abort() has handled all the events.
      break;
    default:
      AppendError(aError);
      break;
  }
}

void
SourceBuffer::AppendError(const MediaResult& aDecodeError)
{
  MOZ_ASSERT(NS_IsMainThread());

  ResetParserState();

  mUpdating = false;

  QueueAsyncSimpleEvent("error");
  QueueAsyncSimpleEvent("updateend");

  MOZ_ASSERT(NS_FAILED(aDecodeError));

  mMediaSource->EndOfStream(aDecodeError);
}

already_AddRefed<MediaByteBuffer>
SourceBuffer::PrepareAppend(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv)
{
  typedef TrackBuffersManager::EvictDataResult Result;

  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  // If the HTMLMediaElement.error attribute is not null, then throw an
  // InvalidStateError exception and abort these steps.
  if (!mMediaSource->GetDecoder() ||
      mMediaSource->GetDecoder()->OwnerHasError()) {
    MSE_DEBUG("HTMLMediaElement.error is not null");
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }

  // Eviction uses a byte threshold. If the buffer is greater than the
  // number of bytes then data is evicted.
  // TODO: Drive evictions off memory pressure notifications.
  // TODO: Consider a global eviction threshold  rather than per TrackBuffer.
  // Give a chance to the TrackBuffersManager to evict some data if needed.
  Result evicted =
    mTrackBuffersManager->EvictData(TimeUnit::FromSeconds(mMediaSource->GetDecoder()->GetCurrentTime()),
                                    aLength);

  // See if we have enough free space to append our new data.
  if (evicted == Result::BUFFER_FULL) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return nullptr;
  }

  RefPtr<MediaByteBuffer> data = new MediaByteBuffer();
  if (!data->AppendElements(aData, aLength, fallible)) {
    aRv.Throw(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR);
    return nullptr;
  }
  return data.forget();
}

double
SourceBuffer::GetBufferedStart()
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult dummy;
  RefPtr<TimeRanges> ranges = GetBuffered(dummy);
  return ranges->Length() > 0 ? ranges->GetStartTime() : 0;
}

double
SourceBuffer::GetBufferedEnd()
{
  MOZ_ASSERT(NS_IsMainThread());
  ErrorResult dummy;
  RefPtr<TimeRanges> ranges = GetBuffered(dummy);
  return ranges->Length() > 0 ? ranges->GetEndTime() : 0;
}

double
SourceBuffer::HighestStartTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mTrackBuffersManager
         ? mTrackBuffersManager->HighestStartTime().ToSeconds()
         : 0.0;
}

double
SourceBuffer::HighestEndTime()
{
  MOZ_ASSERT(NS_IsMainThread());
  return mTrackBuffersManager
         ? mTrackBuffersManager->HighestEndTime().ToSeconds()
         : 0.0;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(SourceBuffer)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(SourceBuffer)
  // Tell the TrackBuffer to end its current SourceBufferResource.
  TrackBuffersManager* manager = tmp->mTrackBuffersManager;
  if (manager) {
    manager->Detach();
  }
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMediaSource)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mBuffered)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(SourceBuffer,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMediaSource)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mBuffered)
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
