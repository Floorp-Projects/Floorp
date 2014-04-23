/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SourceBuffer.h"

#include "AsyncEventRunner.h"
#include "DecoderTraits.h"
#include "MediaDecoder.h"
#include "MediaSourceDecoder.h"
#include "SourceBufferResource.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prlog.h"
#include "SubBufferDecoder.h"

struct JSContext;
class JSObject;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaSourceLog;
#define MSE_DEBUG(...) PR_LOG(gMediaSourceLog, PR_LOG_DEBUG, (__VA_ARGS__))
#else
#define MSE_DEBUG(...)
#endif

namespace mozilla {

class MediaResource;
class ReentrantMonitor;

namespace layers {

class ImageContainer;

} // namespace layers

ReentrantMonitor&
SubBufferDecoder::GetReentrantMonitor()
{
  return mParentDecoder->GetReentrantMonitor();
}

bool
SubBufferDecoder::OnStateMachineThread() const
{
  return mParentDecoder->OnStateMachineThread();
}

bool
SubBufferDecoder::OnDecodeThread() const
{
  return mParentDecoder->OnDecodeThread();
}

SourceBufferResource*
SubBufferDecoder::GetResource() const
{
  return static_cast<SourceBufferResource*>(mResource.get());
}

void
SubBufferDecoder::SetMediaDuration(int64_t aDuration)
{
  mMediaDuration = aDuration;
}

void
SubBufferDecoder::UpdateEstimatedMediaDuration(int64_t aDuration)
{
  //mParentDecoder->UpdateEstimatedMediaDuration(aDuration);
}

void
SubBufferDecoder::SetMediaSeekable(bool aMediaSeekable)
{
  //mParentDecoder->SetMediaSeekable(aMediaSeekable);
}

void
SubBufferDecoder::SetTransportSeekable(bool aTransportSeekable)
{
  //mParentDecoder->SetTransportSeekable(aTransportSeekable);
}

layers::ImageContainer*
SubBufferDecoder::GetImageContainer()
{
  return mParentDecoder->GetImageContainer();
}

int64_t
SubBufferDecoder::ConvertToByteOffset(double aTime)
{
  // Uses a conversion based on (aTime/duration) * length.  For the
  // purposes of eviction this should be adequate since we have the
  // byte threshold as well to ensure data actually gets evicted and
  // we ensure we don't evict before the current playable point.
  if (mMediaDuration == -1) {
    return -1;
  }
  int64_t length = GetResource()->GetLength();
  MOZ_ASSERT(length > 0);
  int64_t offset = (aTime / (double(mMediaDuration) / USECS_PER_S)) * length;
  return offset;
}

class ContainerParser {
public:
  virtual ~ContainerParser() {}

  virtual bool IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    return false;
  }
};

class WebMContainerParser : public ContainerParser {
public:
  bool IsInitSegmentPresent(const uint8_t* aData, uint32_t aLength)
  {
    // XXX: This is overly primitive, needs to collect data as it's appended
    // to the SB and handle, rather than assuming everything is present in a
    // single aData segment.
    // 0x1a45dfa3 // EBML
    // ...
    // DocType == "webm"
    // ...
    // 0x18538067 // Segment (must be "unknown" size)
    // 0x1549a966 // -> Segment Info
    // 0x1654ae6b // -> One or more Tracks
    if (aLength >= 4 &&
        aData[0] == 0x1a && aData[1] == 0x45 && aData[2] == 0xdf && aData[3] == 0xa3) {
      return true;
    }
    return false;
  }
};

namespace dom {

void
SourceBuffer::SetMode(SourceBufferAppendMode aMode, ErrorResult& aRv)
{
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
  mAppendMode = aMode;
}

void
SourceBuffer::SetTimestampOffset(double aTimestampOffset, ErrorResult& aRv)
{
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
  if (!IsAttached()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }
  nsRefPtr<TimeRanges> ranges = new TimeRanges();
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    nsRefPtr<TimeRanges> r = new TimeRanges();
    mDecoders[i]->GetBuffered(r);
    if (r->Length() > 0) {
      MSE_DEBUG("%p GetBuffered decoder=%u Length=%u Start=%f End=%f", this, i, r->Length(),
                r->GetStartTime(), r->GetEndTime());
      ranges->Add(r->GetStartTime(), r->GetEndTime());
    } else {
      MSE_DEBUG("%p GetBuffered decoder=%u Length=%u", this, i, r->Length());
    }
  }
  ranges->Normalize();
  MSE_DEBUG("%p GetBuffered Length=%u Start=%f End=%f", this, ranges->Length(),
            ranges->GetStartTime(), ranges->GetEndTime());
  return ranges.forget();
}

void
SourceBuffer::SetAppendWindowStart(double aAppendWindowStart, ErrorResult& aRv)
{
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
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::AppendBuffer(const ArrayBufferView& aData, ErrorResult& aRv)
{
  AppendData(aData.Data(), aData.Length(), aRv);
}

void
SourceBuffer::Abort(ErrorResult& aRv)
{
  MSE_DEBUG("%p Abort()", this);
  if (!IsAttached()) {
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
  mAppendWindowStart = 0;
  mAppendWindowEnd = PositiveInfinity<double>();

  MSE_DEBUG("%p Abort: Discarding decoders.", this);
  if (mCurrentDecoder) {
    mCurrentDecoder->GetResource()->Ended();
    mCurrentDecoder = nullptr;
  }
}

void
SourceBuffer::Remove(double aStart, double aEnd, ErrorResult& aRv)
{
  MSE_DEBUG("%p Remove(Start=%f End=%f)", this, aStart, aEnd);
  if (!IsAttached() || mUpdating ||
      mMediaSource->ReadyState() != MediaSourceReadyState::Open) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (aStart < 0 || aStart > mMediaSource->Duration() ||
      aEnd <= aStart) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return;
  }
  StartUpdating();
  /// TODO: Run coded frame removal algorithm asynchronously (would call StopUpdating()).
  StopUpdating();
}

void
SourceBuffer::Detach()
{
  Ended();
  mDecoders.Clear();
  mCurrentDecoder = nullptr;
  mMediaSource = nullptr;
}

void
SourceBuffer::Ended()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetResource()->Ended();
  }
}

SourceBuffer::SourceBuffer(MediaSource* aMediaSource, const nsACString& aType)
  : DOMEventTargetHelper(aMediaSource->GetParentObject())
  , mMediaSource(aMediaSource)
  , mType(aType)
  , mAppendWindowStart(0)
  , mAppendWindowEnd(PositiveInfinity<double>())
  , mTimestampOffset(0)
  , mAppendMode(SourceBufferAppendMode::Segments)
  , mUpdating(false)
{
  MOZ_ASSERT(aMediaSource);
  if (mType.EqualsIgnoreCase("video/webm") || mType.EqualsIgnoreCase("audio/webm")) {
    mParser = new WebMContainerParser();
  } else {
    // XXX: Plug in parsers for MPEG4, etc. here.
    mParser = new ContainerParser();
  }
}

already_AddRefed<SourceBuffer>
SourceBuffer::Create(MediaSource* aMediaSource, const nsACString& aType)
{
  nsRefPtr<SourceBuffer> sourceBuffer = new SourceBuffer(aMediaSource, aType);
  return sourceBuffer.forget();
}

SourceBuffer::~SourceBuffer()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetResource()->Ended();
  }
}

MediaSource*
SourceBuffer::GetParentObject() const
{
  return mMediaSource;
}

JSObject*
SourceBuffer::WrapObject(JSContext* aCx)
{
  return SourceBufferBinding::Wrap(aCx, this);
}

void
SourceBuffer::DispatchSimpleEvent(const char* aName)
{
  MSE_DEBUG("%p Dispatching event %s to SourceBuffer", this, aName);
  DispatchTrustedEvent(NS_ConvertUTF8toUTF16(aName));
}

void
SourceBuffer::QueueAsyncSimpleEvent(const char* aName)
{
  MSE_DEBUG("%p Queuing event %s to SourceBuffer", this, aName);
  nsCOMPtr<nsIRunnable> event = new AsyncEventRunner<SourceBuffer>(this, aName);
  NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
}

bool
SourceBuffer::InitNewDecoder()
{
  MediaSourceDecoder* parentDecoder = mMediaSource->GetDecoder();
  nsRefPtr<SubBufferDecoder> decoder = parentDecoder->CreateSubDecoder(mType);
  if (!decoder) {
    return false;
  }
  mDecoders.AppendElement(decoder);
  // XXX: At this point, we really want to push through any remaining
  // processing for the old decoder and discard it, rather than hanging on
  // to all of them in mDecoders.
  mCurrentDecoder = decoder;
  return true;
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
  MSE_DEBUG("%p AppendBuffer(Data=%u bytes)", this, aLength);
  if (!IsAttached() || mUpdating) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }
  if (mMediaSource->ReadyState() == MediaSourceReadyState::Ended) {
    mMediaSource->SetReadyState(MediaSourceReadyState::Open);
  }
  // TODO: Run coded frame eviction algorithm.
  // TODO: Test buffer full flag.
  StartUpdating();
  // TODO: Run buffer append algorithm asynchronously (would call StopUpdating()).
  if (mParser->IsInitSegmentPresent(aData, aLength) || !mCurrentDecoder) {
    MSE_DEBUG("%p AppendBuffer: New initialization segment, switching decoders.", this);
    if (mCurrentDecoder) {
      mCurrentDecoder->GetResource()->Ended();
    }
    if (!InitNewDecoder()) {
      aRv.Throw(NS_ERROR_FAILURE); // XXX: Review error handling.
      return;
    }
  }
  // XXX: For future reference: NDA call must run on the main thread.
  mCurrentDecoder->NotifyDataArrived(reinterpret_cast<const char*>(aData),
                                     aLength,
                                     mCurrentDecoder->GetResource()->GetLength());
  mCurrentDecoder->GetResource()->AppendData(aData, aLength);

  // Eviction uses a byte threshold. If the buffer is greater than the
  // number of bytes then data is evicted. The time range for this
  // eviction is reported back to the media source. It will then
  // evict data before that range across all SourceBuffer's it knows
  // about.
  const int evict_threshold = 1000000;
  bool evicted = mCurrentDecoder->GetResource()->EvictData(evict_threshold);
  if (evicted) {
    double start = 0.0;
    double end = 0.0;
    GetBufferedStartEndTime(&start, &end);

    // We notify that we've evicted from the time range 0 through to
    // the current start point.
    mMediaSource->NotifyEvicted(0.0, start);
  }
  StopUpdating();

  // Schedule the state machine thread to ensure playback starts
  // if required when data is appended.
  mMediaSource->GetDecoder()->ScheduleStateMachineThread();
}

void
SourceBuffer::GetBufferedStartEndTime(double* aStart, double* aEnd)
{
  ErrorResult dummy;
  nsRefPtr<TimeRanges> ranges = GetBuffered(dummy);
  if (!ranges || ranges->Length() == 0) {
    *aStart = *aEnd = 0.0;
    return;
  }
  *aStart = ranges->Start(0, dummy);
  *aEnd = ranges->End(ranges->Length() - 1, dummy);
}

void
SourceBuffer::Evict(double aStart, double aEnd)
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    // Need to map time to byte offset then evict
    int64_t end = mDecoders[i]->ConvertToByteOffset(aEnd);
    if (end <= 0) {
      NS_WARNING("SourceBuffer::Evict failed");
      continue;
    }
    mDecoders[i]->GetResource()->EvictBefore(end);
  }
}

bool
SourceBuffer::ContainsTime(double aTime)
{
  double start = 0.0;
  double end = 0.0;
  GetBufferedStartEndTime(&start, &end);
  return aTime >= start && aTime <= end;
}

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(SourceBuffer, DOMEventTargetHelper,
                                     mMediaSource)

NS_IMPL_ADDREF_INHERITED(SourceBuffer, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SourceBuffer, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(SourceBuffer)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

} // namespace dom

} // namespace mozilla
