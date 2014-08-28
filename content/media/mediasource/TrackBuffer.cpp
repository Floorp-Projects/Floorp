/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackBuffer.h"

#include "MediaSourceDecoder.h"
#include "SharedThreadPool.h"
#include "MediaTaskQueue.h"
#include "SourceBufferDecoder.h"
#include "SourceBufferResource.h"
#include "VideoUtils.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/MediaSourceBinding.h"
#include "mozilla/dom/TimeRanges.h"
#include "nsError.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"
#include "prlog.h"

struct JSContext;
class JSObject;

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

namespace mozilla {

TrackBuffer::TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType)
  : mParentDecoder(aParentDecoder)
  , mType(aType)
  , mLastStartTimestamp(0)
  , mLastEndTimestamp(UnspecifiedNaN<double>())
  , mHasAudio(false)
  , mHasVideo(false)
{
  MOZ_COUNT_CTOR(TrackBuffer);
  mTaskQueue = new MediaTaskQueue(GetMediaDecodeThreadPool());
  aParentDecoder->AddTrackBuffer(this);
}

TrackBuffer::~TrackBuffer()
{
  MOZ_COUNT_DTOR(TrackBuffer);
}

class ReleaseDecoderTask : public nsRunnable {
public:
  explicit ReleaseDecoderTask(nsRefPtr<SourceBufferDecoder> aDecoder)
  {
    mDecoders.AppendElement(aDecoder);
  }

  explicit ReleaseDecoderTask(nsTArray<nsRefPtr<SourceBufferDecoder>>& aDecoders)
  {
    mDecoders.SwapElements(aDecoders);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE MOZ_FINAL {
    mDecoders.Clear();
    return NS_OK;
  }

private:
  nsTArray<nsRefPtr<SourceBufferDecoder>> mDecoders;
};

void
TrackBuffer::Shutdown()
{
  // Shutdown waits for any pending events, which may require the monitor,
  // so we must not hold the monitor during this call.
  mTaskQueue->Shutdown();

  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  DiscardDecoder();
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->Shutdown();
  }
  NS_DispatchToMainThread(new ReleaseDecoderTask(mDecoders));
  MOZ_ASSERT(mDecoders.IsEmpty());
}

void
TrackBuffer::AppendData(const uint8_t* aData, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mCurrentDecoder);

  SourceBufferResource* resource = mCurrentDecoder->GetResource();
  // XXX: For future reference: NDA call must run on the main thread.
  mCurrentDecoder->NotifyDataArrived(reinterpret_cast<const char*>(aData),
                                     aLength, resource->GetLength());
  resource->AppendData(aData, aLength);
}

bool
TrackBuffer::EvictData(uint32_t aThreshold)
{
  MOZ_ASSERT(NS_IsMainThread());
  // XXX Call EvictData on mDecoders?
  return mCurrentDecoder->GetResource()->EvictData(aThreshold);
}

void
TrackBuffer::EvictBefore(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  // XXX Call EvictBefore on mDecoders?
  int64_t endOffset = mCurrentDecoder->ConvertToByteOffset(aTime);
  if (endOffset > 0) {
    mCurrentDecoder->GetResource()->EvictBefore(endOffset);
  }
  MSE_DEBUG("TrackBuffer(%p)::EvictBefore offset=%lld", this, endOffset);
}

double
TrackBuffer::Buffered(dom::TimeRanges* aRanges)
{
  MOZ_ASSERT(NS_IsMainThread());
  // XXX check default if mDecoders empty?
  double highestEndTime = 0;

  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mDecoders[i]->GetBuffered(r);
    if (r->Length() > 0) {
      highestEndTime = std::max(highestEndTime, r->GetEndTime());
      aRanges->Union(r);
    }
  }

  return highestEndTime;
}

bool
TrackBuffer::NewDecoder()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mCurrentDecoder && mParentDecoder);

  nsRefPtr<SourceBufferDecoder> decoder = mParentDecoder->CreateSubDecoder(mType);
  if (!decoder) {
    return false;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  mCurrentDecoder = decoder;

  mLastStartTimestamp = 0;
  mLastEndTimestamp = UnspecifiedNaN<double>();

  return QueueInitializeDecoder(decoder);
}

bool
TrackBuffer::QueueInitializeDecoder(nsRefPtr<SourceBufferDecoder> aDecoder)
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<nsRefPtr<SourceBufferDecoder>>(this,
                                                               &TrackBuffer::InitializeDecoder,
                                                               aDecoder);
  aDecoder->SetTaskQueue(mTaskQueue);
  if (NS_FAILED(mTaskQueue->Dispatch(task))) {
    MSE_DEBUG("MediaSourceReader(%p): Failed to enqueue decoder initialization task", this);
    return false;
  }
  return true;
}

void
TrackBuffer::InitializeDecoder(nsRefPtr<SourceBufferDecoder> aDecoder)
{
  // ReadMetadata may block the thread waiting on data, so it must not be
  // called with the monitor held.
  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();

  MediaDecoderReader* reader = aDecoder->GetReader();
  MSE_DEBUG("TrackBuffer(%p): Initializing subdecoder %p reader %p",
            this, aDecoder.get(), reader);

  MediaInfo mi;
  nsAutoPtr<MetadataTags> tags; // TODO: Handle metadata.
  nsresult rv = reader->ReadMetadata(&mi, getter_Transfers(tags));
  reader->SetIdle();
  if (NS_FAILED(rv) || (!mi.HasVideo() && !mi.HasAudio())) {
    // XXX: Need to signal error back to owning SourceBuffer.
    bool hasAudio = NS_SUCCEEDED(rv) && mi.HasAudio();
    bool hasVideo = NS_SUCCEEDED(rv) && mi.HasVideo();
    MSE_DEBUG("TrackBuffer(%p): Reader %p failed to initialize rv=%x audio=%d video=%d",
              this, reader, rv, hasAudio, hasVideo);
    aDecoder->SetTaskQueue(nullptr);
    NS_DispatchToMainThread(new ReleaseDecoderTask(aDecoder));
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

  MSE_DEBUG("TrackBuffer(%p): Reader %p activated", this, reader);
  RegisterDecoder(aDecoder);
}

void
TrackBuffer::RegisterDecoder(nsRefPtr<SourceBufferDecoder> aDecoder)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  aDecoder->SetTaskQueue(nullptr);
  const MediaInfo& info = aDecoder->GetReader()->GetMediaInfo();
  // Initialize the track info since this is the first decoder.
  if (mDecoders.IsEmpty()) {
    mHasAudio = info.HasAudio();
    mHasVideo = info.HasVideo();
    mParentDecoder->OnTrackBufferConfigured(this);
  } else if ((info.HasAudio() && !mHasAudio) || (info.HasVideo() && !mHasVideo)) {
    MSE_DEBUG("TrackBuffer(%p)::RegisterDecoder with mismatched audio/video tracks", this);
  }
  mDecoders.AppendElement(aDecoder);
}

void
TrackBuffer::DiscardDecoder()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  if (mCurrentDecoder) {
    mCurrentDecoder->GetResource()->Ended();
  }
  mCurrentDecoder = nullptr;
}

void
TrackBuffer::Detach()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mCurrentDecoder) {
    DiscardDecoder();
  }
}

bool
TrackBuffer::HasInitSegment()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  return mHasAudio || mHasVideo;
}

void
TrackBuffer::LastTimestamp(double& aStart, double& aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  aStart = mLastStartTimestamp;
  aEnd = mLastEndTimestamp;
}

void
TrackBuffer::SetLastStartTimestamp(double aStart)
{
  MOZ_ASSERT(NS_IsMainThread());
  mLastStartTimestamp = aStart;
}

void
TrackBuffer::SetLastEndTimestamp(double aEnd)
{
  MOZ_ASSERT(NS_IsMainThread());
  mLastEndTimestamp = aEnd;
}

bool
TrackBuffer::ContainsTime(double aTime)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mDecoders[i]->GetBuffered(r);
    if (r->Find(aTime) != dom::TimeRanges::NoIndex) {
      return true;
    }
  }

  return false;
}

bool
TrackBuffer::HasAudio()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  return mHasAudio;
}

bool
TrackBuffer::HasVideo()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  return mHasVideo;
}

void
TrackBuffer::BreakCycles()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->BreakCycles();
  }
  mDecoders.Clear();
}

void
TrackBuffer::ResetDecode()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->ResetDecode();
  }
}

const nsTArray<nsRefPtr<SourceBufferDecoder>>&
TrackBuffer::Decoders()
{
  // XXX assert OnDecodeThread
  return mDecoders;
}

} // namespace mozilla
