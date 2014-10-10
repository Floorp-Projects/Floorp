/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TrackBuffer.h"

#include "ContainerParser.h"
#include "MediaSourceDecoder.h"
#include "SharedThreadPool.h"
#include "MediaTaskQueue.h"
#include "SourceBufferDecoder.h"
#include "SourceBufferResource.h"
#include "VideoUtils.h"
#include "mozilla/dom/TimeRanges.h"
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

namespace mozilla {

TrackBuffer::TrackBuffer(MediaSourceDecoder* aParentDecoder, const nsACString& aType)
  : mParentDecoder(aParentDecoder)
  , mType(aType)
  , mLastStartTimestamp(0)
  , mLastEndTimestamp(0)
{
  MOZ_COUNT_CTOR(TrackBuffer);
  mParser = ContainerParser::CreateForMIMEType(aType);
  mTaskQueue = new MediaTaskQueue(GetMediaDecodeThreadPool());
  aParentDecoder->AddTrackBuffer(this);
}

TrackBuffer::~TrackBuffer()
{
  MOZ_COUNT_DTOR(TrackBuffer);
}

class ReleaseDecoderTask : public nsRunnable {
public:
  explicit ReleaseDecoderTask(SourceBufferDecoder* aDecoder)
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
  // End the SourceBufferResource associated with mCurrentDecoder, which will
  // unblock any decoder initialization in ReadMetadata().
  DiscardDecoder();

  // Finish any decoder initialization, which may add to mInitializedDecoders.
  // Shutdown waits for any pending events, which may require the monitor,
  // so we must not hold the monitor during this call.
  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();
  mTaskQueue->Shutdown();
  mTaskQueue = nullptr;

  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->Shutdown();
  }
  mInitializedDecoders.Clear();
  NS_DispatchToMainThread(new ReleaseDecoderTask(mDecoders));
  MOZ_ASSERT(mDecoders.IsEmpty());
  mParentDecoder = nullptr;
}

bool
TrackBuffer::AppendData(const uint8_t* aData, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  // TODO: Run more of the buffer append algorithm asynchronously.
  if (mParser->IsInitSegmentPresent(aData, aLength)) {
    MSE_DEBUG("TrackBuffer(%p)::AppendData: New initialization segment.", this);
    if (!NewDecoder()) {
      return false;
    }
  } else if (!mParser->HasInitData()) {
    MSE_DEBUG("TrackBuffer(%p)::AppendData: Non-init segment appended during initialization.", this);
    return false;
  }

  int64_t start, end;
  if (mParser->ParseStartAndEndTimestamps(aData, aLength, start, end)) {
    if (mParser->IsMediaSegmentPresent(aData, aLength) &&
        !mParser->TimestampsFuzzyEqual(start, mLastEndTimestamp)) {
      MSE_DEBUG("TrackBuffer(%p)::AppendData: Data last=[%lld, %lld] overlaps [%lld, %lld]",
                this, mLastStartTimestamp, mLastEndTimestamp, start, end);

      // This data is earlier in the timeline than data we have already
      // processed, so we must create a new decoder to handle the decoding.
      if (!NewDecoder()) {
        return false;
      }
      MSE_DEBUG("TrackBuffer(%p)::AppendData: Decoder marked as initialized.", this);
      const nsTArray<uint8_t>& initData = mParser->InitData();
      AppendDataToCurrentResource(initData.Elements(), initData.Length());
      mLastStartTimestamp = start;
    }
    mLastEndTimestamp = end;
    MSE_DEBUG("TrackBuffer(%p)::AppendData: Segment last=[%lld, %lld] [%lld, %lld]",
              this, mLastStartTimestamp, mLastEndTimestamp, start, end);
  }

  if (!AppendDataToCurrentResource(aData, aLength)) {
    return false;
  }

  // Schedule the state machine thread to ensure playback starts if required
  // when data is appended.
  mParentDecoder->ScheduleStateMachineThread();
  return true;
}

bool
TrackBuffer::AppendDataToCurrentResource(const uint8_t* aData, uint32_t aLength)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mCurrentDecoder) {
    return false;
  }

  SourceBufferResource* resource = mCurrentDecoder->GetResource();
  int64_t appendOffset = resource->GetLength();
  resource->AppendData(aData, aLength);
  // XXX: For future reference: NDA call must run on the main thread.
  mCurrentDecoder->NotifyDataArrived(reinterpret_cast<const char*>(aData),
                                     aLength, appendOffset);
  mParentDecoder->NotifyTimeRangesChanged();

  return true;
}

bool
TrackBuffer::EvictData(uint32_t aThreshold)
{
  MOZ_ASSERT(NS_IsMainThread());

  int64_t totalSize = 0;
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    totalSize += mDecoders[i]->GetResource()->GetSize();
  }

  int64_t toEvict = totalSize - aThreshold;
  if (toEvict <= 0) {
    return false;
  }

  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    MSE_DEBUG("TrackBuffer(%p)::EvictData decoder=%u threshold=%u toEvict=%lld",
              this, i, aThreshold, toEvict);
    toEvict -= mDecoders[i]->GetResource()->EvictData(toEvict);
  }
  return toEvict < (totalSize - aThreshold);
}

void
TrackBuffer::EvictBefore(double aTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    int64_t endOffset = mDecoders[i]->ConvertToByteOffset(aTime);
    if (endOffset > 0) {
      MSE_DEBUG("TrackBuffer(%p)::EvictBefore decoder=%u offset=%lld", this, i, endOffset);
      mDecoders[i]->GetResource()->EvictBefore(endOffset);
    }
  }
}

double
TrackBuffer::Buffered(dom::TimeRanges* aRanges)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  MOZ_ASSERT(NS_IsMainThread());

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
  MOZ_ASSERT(mParentDecoder);

  DiscardDecoder();

  nsRefPtr<SourceBufferDecoder> decoder = mParentDecoder->CreateSubDecoder(mType);
  if (!decoder) {
    return false;
  }
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  mCurrentDecoder = decoder;
  mDecoders.AppendElement(decoder);

  mLastStartTimestamp = 0;
  mLastEndTimestamp = 0;

  decoder->SetTaskQueue(mTaskQueue);
  return QueueInitializeDecoder(decoder);
}

bool
TrackBuffer::QueueInitializeDecoder(SourceBufferDecoder* aDecoder)
{
  RefPtr<nsIRunnable> task =
    NS_NewRunnableMethodWithArg<SourceBufferDecoder*>(this,
                                                      &TrackBuffer::InitializeDecoder,
                                                      aDecoder);
  if (NS_FAILED(mTaskQueue->Dispatch(task))) {
    MSE_DEBUG("MediaSourceReader(%p): Failed to enqueue decoder initialization task", this);
    RemoveDecoder(aDecoder);
    return false;
  }
  return true;
}

void
TrackBuffer::InitializeDecoder(SourceBufferDecoder* aDecoder)
{
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  // ReadMetadata may block the thread waiting on data, so it must not be
  // called with the monitor held.
  mParentDecoder->GetReentrantMonitor().AssertNotCurrentThreadIn();

  MediaDecoderReader* reader = aDecoder->GetReader();
  MSE_DEBUG("TrackBuffer(%p): Initializing subdecoder %p reader %p",
            this, aDecoder, reader);

  MediaInfo mi;
  nsAutoPtr<MetadataTags> tags; // TODO: Handle metadata.
  nsresult rv = reader->ReadMetadata(&mi, getter_Transfers(tags));
  reader->SetIdle();

  if (NS_SUCCEEDED(rv) && reader->IsWaitingOnCDMResource()) {
    ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
    mWaitingDecoders.AppendElement(aDecoder);
    return;
  }

  aDecoder->SetTaskQueue(nullptr);

  if (NS_FAILED(rv) || (!mi.HasVideo() && !mi.HasAudio())) {
    // XXX: Need to signal error back to owning SourceBuffer.
    MSE_DEBUG("TrackBuffer(%p): Reader %p failed to initialize rv=%x audio=%d video=%d",
              this, reader, rv, mi.HasAudio(), mi.HasVideo());
    RemoveDecoder(aDecoder);
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

  if (!RegisterDecoder(aDecoder)) {
    // XXX: Need to signal error back to owning SourceBuffer.
    MSE_DEBUG("TrackBuffer(%p): Reader %p not activated", this, reader);
    RemoveDecoder(aDecoder);
    return;
  }
  MSE_DEBUG("TrackBuffer(%p): Reader %p activated", this, reader);
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
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
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
  return mParser->HasInitData();
}

bool
TrackBuffer::IsReady()
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  MOZ_ASSERT((mInfo.HasAudio() || mInfo.HasVideo()) || mInitializedDecoders.IsEmpty());
  return mParser->HasInitData() && (mInfo.HasAudio() || mInfo.HasVideo());
}

bool
TrackBuffer::ContainsTime(int64_t aTime)
{
  ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
  for (uint32_t i = 0; i < mInitializedDecoders.Length(); ++i) {
    nsRefPtr<dom::TimeRanges> r = new dom::TimeRanges();
    mInitializedDecoders[i]->GetBuffered(r);
    if (r->Find(double(aTime) / USECS_PER_S) != dom::TimeRanges::NoIndex) {
      return true;
    }
  }

  return false;
}

void
TrackBuffer::BreakCycles()
{
  for (uint32_t i = 0; i < mDecoders.Length(); ++i) {
    mDecoders[i]->GetReader()->BreakCycles();
  }
  NS_DispatchToMainThread(new ReleaseDecoderTask(mDecoders));
  MOZ_ASSERT(mDecoders.IsEmpty());

  // These are cleared in Shutdown()
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

void
TrackBuffer::RemoveDecoder(SourceBufferDecoder* aDecoder)
{
  RefPtr<nsIRunnable> task = new ReleaseDecoderTask(aDecoder);
  {
    ReentrantMonitorAutoEnter mon(mParentDecoder->GetReentrantMonitor());
    MOZ_ASSERT(!mInitializedDecoders.Contains(aDecoder));
    mDecoders.RemoveElement(aDecoder);
    if (mCurrentDecoder == aDecoder) {
      DiscardDecoder();
    }
  }
  // At this point, task should be holding the only reference to aDecoder.
  NS_DispatchToMainThread(task);
}

} // namespace mozilla
