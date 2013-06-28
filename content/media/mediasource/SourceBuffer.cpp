/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBuffer.h"

#include "nsContentUtils.h"

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define LOG(type, msg) PR_LOG(gMediaSourceLog, type, msg)
#else
#define LOG(type, msg)
#endif

namespace mozilla {
namespace dom {

void
SourceBuffer::SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv)
{
  if (!mAttached || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  // TODO:: Test append state.
  // TODO:: If aMode is "sequence", set sequence start time.
  mAppendMode = aMode;
}

void
SourceBuffer::SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv)
{
  if (!mAttached || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
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
  if (!mAttached) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  // TODO: Populate ranges.
  return ranges.forget();
}

void
SourceBuffer::SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv)
{
  if (!mAttached || mUpdating) {
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
  if (!mAttached || mUpdating) {
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
SourceBuffer::AppendBuffer(ArrayBuffer& aData, ErrorResult& aRv)
{
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::AppendBuffer(ArrayBufferView& aData, ErrorResult& aRv)
{
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::Abort(ErrorResult& aRv)
{
  if (!mAttached) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mUpdating) {
    // TODO: Abort segment parser loop, buffer append, and stream append loop algorithms.
    AbortUpdating();
  }
  // TODO: Run reset parser algorithm.
  // XXX: Need to run these two resets through setters?
  mAppendWindowStart = 0;
  mAppendWindowEnd = PositiveInfinity();
}

void
SourceBuffer::Remove(double aStart, double aEnd, ErrorResult& aRv)
{
  if (aStart < 0 || aStart > mMediaSource->Duration() ||
      aEnd <= aStart) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  if (!mAttached || mUpdating ||
      mMediaSource->ReadyState() != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  StartUpdating();
  /// TODO: Run coded frame removal algorithm asynchronously (would call StopUpdating()).
  StopUpdating();
}

void
SourceBuffer::Attach()
{
  MOZ_ASSERT(!mAttached);
  mAttached = true;
}

void
SourceBuffer::Detach()
{
  MOZ_ASSERT(mAttached);
  mAttached = false;
}

SourceBuffer::SourceBuffer(MediaSource* aMediaSource)
  : nsDOMEventTargetHelper(aMediaSource->GetParentObject())
  , mMediaSource(aMediaSource)
  , mAppendWindowStart(0)
  , mAppendWindowEnd(PositiveInfinity())
  , mTimestampOffset(0)
  , mAppendMode(SourceBufferAppendMode::Segments)
  , mUpdating(false)
  , mAttached(false)
{
  MOZ_ASSERT(aMediaSource);
}

MediaSource*
SourceBuffer::GetParentObject() const
{
  return mMediaSource;
}

JSObject*
SourceBuffer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return SourceBufferBinding::Wrap(aCx, aScope, this);
}

void
SourceBuffer::DispatchSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Dispatching event %s to SourceBuffer", this, aName));
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBuffer::QueueAsyncSimpleEvent(const char* aName)
{
  LOG(PR_LOG_DEBUG, ("%p Queuing event %s to SourceBuffer", this, aName));
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunnner<SourceBuffer>(this, aName);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

void
SourceBuffer::StartUpdating()
{
  MOZ_ASSERT(!mUpdating);
  mUpdating = true;
  QueueAsyncSimpleEvent("updatestart");
}

void
SourceBuffer::StopUpdating()
{
  MOZ_ASSERT(mUpdating);
  mUpdating = false;
  QueueAsyncSimpleEvent("update");
  QueueAsyncSimpleEvent("updateend");
}

void
SourceBuffer::AbortUpdating()
{
  MOZ_ASSERT(mUpdating);
  mUpdating = false;
  QueueAsyncSimpleEvent("abort");
  QueueAsyncSimpleEvent("updateend");
}

void
SourceBuffer::AppendData(const uint8_t* aData, uint32_t aLength, ErrorResult& aRv)
{
  if (!mAttached || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  // TODO: Run coded frame eviction algorithm.
  // TODO: Test buffer full flag.
  mMediaSource->AppendData(aData, aLength, aRv); // XXX: Appending to input buffer.
  StartUpdating();
  // TODO: Run buffer append algorithm asynchronously (would call StopUpdating()).
  StopUpdating();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(SourceBuffer, nsDOMEventTargetHelper, mMediaSource)

NS_IMPL_ADDREF_INHERITED(SourceBuffer, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBuffer, nsDOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBuffer)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

} // namespace dom
} // namespace mozilla
