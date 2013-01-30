/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */

/* This Source Code Form Is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DASH - Dynamic Adaptive Streaming over HTTP
 *
 * DASH is an adaptive bitrate streaming technology where a multimedia file is
 * partitioned into one or more segments and delivered to a client using HTTP.
 *
 * see DASHDecoder.cpp for info on DASH interaction with the media engine.*/

#include "prlog.h"
#include "VideoUtils.h"
#include "SegmentBase.h"
#include "MediaDecoderStateMachine.h"
#include "DASHReader.h"
#include "MediaResource.h"
#include "DASHRepDecoder.h"
#include "WebMReader.h"
#include <algorithm>

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define LOG(msg, ...) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, \
                             ("%p [DASHRepDecoder] " msg, this, __VA_ARGS__))
#define LOG1(msg) PR_LOG(gMediaDecoderLog, PR_LOG_DEBUG, \
                         ("%p [DASHRepDecoder] " msg, this))
#else
#define LOG(msg, ...)
#define LOG1(msg)
#endif

MediaDecoderStateMachine*
DASHRepDecoder::CreateStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  // Do not create; just return current state machine.
  return mDecoderStateMachine;
}

nsresult
DASHRepDecoder::SetStateMachine(MediaDecoderStateMachine* aSM)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mDecoderStateMachine = aSM;
  return NS_OK;
}

void
DASHRepDecoder::SetResource(MediaResource* aResource)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mResource = aResource;
}

void
DASHRepDecoder::SetMPDRepresentation(Representation const * aRep)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mMPDRepresentation = aRep;
}

void
DASHRepDecoder::SetReader(WebMReader* aReader)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  mReader = aReader;
}

nsresult
DASHRepDecoder::Load(MediaResource* aResource,
                     nsIStreamListener** aListener,
                     MediaDecoder* aCloneDonor)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ENSURE_TRUE(mMPDRepresentation, NS_ERROR_NOT_INITIALIZED);

  // Get init range and index range from MPD.
  SegmentBase const * segmentBase = mMPDRepresentation->GetSegmentBase();
  NS_ENSURE_TRUE(segmentBase, NS_ERROR_NULL_POINTER);

  // Get and set init range.
  segmentBase->GetInitRange(&mInitByteRange.mStart, &mInitByteRange.mEnd);
  NS_ENSURE_TRUE(!mInitByteRange.IsNull(), NS_ERROR_NOT_INITIALIZED);
  mReader->SetInitByteRange(mInitByteRange);

  // Get and set index range.
  segmentBase->GetIndexRange(&mIndexByteRange.mStart, &mIndexByteRange.mEnd);
  NS_ENSURE_TRUE(!mIndexByteRange.IsNull(), NS_ERROR_NOT_INITIALIZED);
  mReader->SetIndexByteRange(mIndexByteRange);

  // Determine byte range to Open.
  // For small deltas between init and index ranges, we need to bundle the byte
  // range requests together in order to deal with |MediaCache|'s control of
  // seeking (see |MediaCache|::|Update|). |MediaCache| will not initiate a
  // |ChannelMediaResource|::|CacheClientSeek| for the INDEX byte range if the
  // delta between it and the INIT byte ranges is less than
  // |SEEK_VS_READ_THRESHOLD|. To get around this, request all metadata bytes
  // now so |MediaCache| can assume the bytes are en route.
  int64_t delta = std::max(mIndexByteRange.mStart, mInitByteRange.mStart)
                - std::min(mIndexByteRange.mEnd, mInitByteRange.mEnd);
  MediaByteRange byteRange;
  if (delta <= SEEK_VS_READ_THRESHOLD) {
    byteRange.mStart = std::min(mIndexByteRange.mStart, mInitByteRange.mStart);
    byteRange.mEnd = std::max(mIndexByteRange.mEnd, mInitByteRange.mEnd);
    // Loading everything in one chunk .
    mMetadataChunkCount = 1;
  } else {
    byteRange = mInitByteRange;
    // Loading in two chunks: init and index.
    mMetadataChunkCount = 2;
  }
  mCurrentByteRange = byteRange;
  return mResource->OpenByteRange(nullptr, byteRange);
}

void
DASHRepDecoder::NotifyDownloadEnded(nsresult aStatus)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  if (!mMainDecoder) {
    if (!mShuttingDown) {
      LOG("Error! Main Decoder is null before shutdown: mMainDecoder [%p] ",
          mMainDecoder.get());
      DecodeError();
    }
    return;
  }

  if (NS_SUCCEEDED(aStatus)) {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    // Decrement counter as metadata chunks are downloaded.
    // Note: Reader gets next chunk download via |ChannelMediaResource|:|Seek|.
    if (mMetadataChunkCount > 0) {
      LOG("Metadata chunk [%d] downloaded: range requested [%lld - %lld] "
          "subsegmentIdx [%d]",
          mMetadataChunkCount,
          mCurrentByteRange.mStart, mCurrentByteRange.mEnd, mSubsegmentIdx);
      mMetadataChunkCount--;
    } else {
      LOG("Byte range downloaded: status [%x] range requested [%lld - %lld] "
          "subsegmentIdx [%d]",
          aStatus, mCurrentByteRange.mStart, mCurrentByteRange.mEnd,
          mSubsegmentIdx);
      if ((uint32_t)mSubsegmentIdx == mByteRanges.Length()-1) {
        mResource->NotifyLastByteRange();
      }
      // Notify main decoder that a DATA byte range is downloaded.
      mMainDecoder->NotifyDownloadEnded(this, aStatus, mSubsegmentIdx);
    }
  } else if (aStatus == NS_BINDING_ABORTED) {
    LOG("Media download has been cancelled by the user: aStatus [%x].",
        aStatus);
    if (mMainDecoder) {
      mMainDecoder->LoadAborted();
    }
    return;
  } else if (aStatus != NS_BASE_STREAM_CLOSED) {
    LOG("Network error trying to download MPD: aStatus [%x].", aStatus);
    NetworkError();
  }
}

void
DASHRepDecoder::OnReadMetadataCompleted()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  // If shutting down, just return silently.
  if (mShuttingDown) {
    LOG1("Shutting down! Ignoring OnReadMetadataCompleted().");
    return;
  }

  LOG1("Metadata has been read.");

  // Metadata loaded and read for this stream; ok to populate byte ranges.
  nsresult rv = PopulateByteRanges();
  if (NS_FAILED(rv) || mByteRanges.IsEmpty()) {
    LOG("Error populating byte ranges [%x]", rv);
    DecodeError();
    return;
  }

  mMainDecoder->OnReadMetadataCompleted(this);
}

nsresult
DASHRepDecoder::PopulateByteRanges()
{
  NS_ASSERTION(OnDecodeThread(), "Should be on decode thread.");

  // Should not be called during shutdown.
  NS_ENSURE_FALSE(mShuttingDown, NS_ERROR_UNEXPECTED);

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (!mByteRanges.IsEmpty()) {
    return NS_OK;
  }
  NS_ENSURE_TRUE(mReader, NS_ERROR_NULL_POINTER);
  LOG1("Populating byte range array.");
  return mReader->GetSubsegmentByteRanges(mByteRanges);
}

void
DASHRepDecoder::LoadNextByteRange()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  NS_ASSERTION(mResource, "Error: resource is reported as null!");

  // Return silently if shutting down.
  if (mShuttingDown) {
    LOG1("Shutting down! Ignoring LoadNextByteRange().");
    return;
  }

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  NS_ASSERTION(mMainDecoder, "Error: main decoder is null!");
  NS_ASSERTION(mMainDecoder->IsDecoderAllowedToDownloadData(this),
               "Should not be called on non-active decoders!");

  // Cannot have empty byte ranges.
  if (mByteRanges.IsEmpty()) {
    LOG1("Error getting list of subsegment byte ranges.");
    DecodeError();
    return;
  }

  // Get byte range for subsegment.
  int32_t subsegmentIdx = mMainDecoder->GetSubsegmentIndex(this);
  NS_ASSERTION(0 <= subsegmentIdx,
               "Subsegment index should be >= 0 for active decoders");
  if (subsegmentIdx >= 0 && (uint32_t)subsegmentIdx < mByteRanges.Length()) {
    mCurrentByteRange = mByteRanges[subsegmentIdx];
    mSubsegmentIdx = subsegmentIdx;
  } else {
    mCurrentByteRange.Clear();
    mSubsegmentIdx = -1;
    LOG("End of subsegments: index [%d] out of range.", subsegmentIdx);
    return;
  }

  // Request a seek for the first reader. Required so that the reader is
  // primed to start here, and will block subsequent subsegment seeks unless
  // the subsegment has been read.
  if (subsegmentIdx == 0) {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    mReader->RequestSeekToSubsegment(0);
  }

  // Query resource for cached ranges; only download if it's not there.
  if (IsSubsegmentCached(mSubsegmentIdx)) {
    LOG("Subsegment [%d] bytes [%lld] to [%lld] already cached. No need to "
        "download.", mSubsegmentIdx,
        mCurrentByteRange.mStart, mCurrentByteRange.mEnd);
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &DASHRepDecoder::DoNotifyDownloadEnded);
    nsresult rv = NS_DispatchToMainThread(event);
    if (NS_FAILED(rv)) {
      LOG("Error notifying subsegment [%d] cached: rv[0x%x].",
          mSubsegmentIdx, rv);
      NetworkError();
    }
    return;
  }

  // Open byte range corresponding to subsegment.
  nsresult rv = mResource->OpenByteRange(nullptr, mCurrentByteRange);
  if (NS_FAILED(rv)) {
    LOG("Error opening byte range [%lld - %lld]: subsegmentIdx [%d] rv [%x].",
        mCurrentByteRange.mStart, mCurrentByteRange.mEnd, mSubsegmentIdx, rv);
    NetworkError();
    return;
  }
}

bool
DASHRepDecoder::IsSubsegmentCached(int32_t aSubsegmentIdx)
{
  GetReentrantMonitor().AssertCurrentThreadIn();

  MediaByteRange byteRange = mByteRanges[aSubsegmentIdx];
  int64_t start = mResource->GetNextCachedData(byteRange.mStart);
  int64_t end = mResource->GetCachedDataEnd(byteRange.mStart);
  return (start == byteRange.mStart &&
          end >= byteRange.mEnd);
}

void
DASHRepDecoder::DoNotifyDownloadEnded()
{
  NotifyDownloadEnded(NS_OK);
}

nsresult
DASHRepDecoder::GetByteRangeForSeek(int64_t const aOffset,
                                    MediaByteRange& aByteRange)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  // Only check data ranges if they're available and if this decoder is active,
  // i.e. inactive rep decoders should only load metadata.
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());

  for (uint32_t i = 0; i < mByteRanges.Length(); i++) {
    NS_ENSURE_FALSE(mByteRanges[i].IsNull(), NS_ERROR_NOT_INITIALIZED);
    // Check if |aOffset| lies within the current data range.
    if (mByteRanges[i].mStart <= aOffset && aOffset <= mByteRanges[i].mEnd) {
      if (mMainDecoder->IsDecoderAllowedToDownloadSubsegment(this, i)) {
        mCurrentByteRange = aByteRange = mByteRanges[i];
        mSubsegmentIdx = i;
        // XXX Hack: should be setting subsegment outside this function, but
        // need to review seeking for multiple switches anyhow.
        mMainDecoder->SetSubsegmentIndex(this, i);
        LOG("Getting DATA range [%d] for seek offset [%lld]: "
            "bytes [%lld] to [%lld]",
            i, aOffset, aByteRange.mStart, aByteRange.mEnd);
        return NS_OK;
      }
      break;
    }
  }
  // Don't allow metadata downloads once they're loaded and byte ranges have
  // been populated.
  bool canDownloadMetadata = mByteRanges.IsEmpty();
  if (canDownloadMetadata) {
    // Check metadata ranges; init range.
    if (mInitByteRange.mStart <= aOffset && aOffset <= mInitByteRange.mEnd) {
      mCurrentByteRange = aByteRange = mInitByteRange;
      mSubsegmentIdx = 0;
        LOG("Getting INIT range for seek offset [%lld]: bytes [%lld] to "
            "[%lld]", aOffset, aByteRange.mStart, aByteRange.mEnd);
      return NS_OK;
    }
    // ... index range.
    if (mIndexByteRange.mStart <= aOffset && aOffset <= mIndexByteRange.mEnd) {
      mCurrentByteRange = aByteRange = mIndexByteRange;
      mSubsegmentIdx = 0;
      LOG("Getting INDEXES range for seek offset [%lld]: bytes [%lld] to "
          "[%lld]", aOffset, aByteRange.mStart, aByteRange.mEnd);
      return NS_OK;
    }
  } else {
    LOG1("Metadata should be read; inhibiting further metadata downloads.");
  }

  // If no byte range is found by this stage, clear the parameter and return.
  aByteRange.Clear();
  if (mByteRanges.IsEmpty() || !canDownloadMetadata) {
    // Assume mByteRanges will be populated after metadata is read.
    LOG("Data ranges not populated [%s]; metadata download restricted [%s]: "
        "offset[%lld].",
        (mByteRanges.IsEmpty() ? "yes" : "no"),
        (canDownloadMetadata ? "no" : "yes"), aOffset);
    return NS_ERROR_NOT_AVAILABLE;
  } else {
    // Cannot seek to an unknown offset.
    // XXX Revisit this for dynamic MPD profiles if MPD is regularly updated.
    LOG("Error! Offset [%lld] is in an unknown range!", aOffset);
    return NS_ERROR_ILLEGAL_VALUE;
  }
}

void
DASHRepDecoder::PrepareForSwitch()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  // Ensure that the media cache writes any data held in its partial block.
  mResource->FlushCache();
}

void
DASHRepDecoder::NetworkError()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mMainDecoder) { mMainDecoder->NetworkError(); }
}

void
DASHRepDecoder::SetDuration(double aDuration)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mMainDecoder) { mMainDecoder->SetDuration(aDuration); }
}

void
DASHRepDecoder::SetInfinite(bool aInfinite)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mMainDecoder) { mMainDecoder->SetInfinite(aInfinite); }
}

void
DASHRepDecoder::SetMediaSeekable(bool aMediaSeekable)
{
  NS_ASSERTION(NS_IsMainThread() || OnDecodeThread(),
               "Should be on main thread or decode thread.");
  if (mMainDecoder) { mMainDecoder->SetMediaSeekable(aMediaSeekable); }
}

void
DASHRepDecoder::Progress(bool aTimer)
{
  if (mMainDecoder) { mMainDecoder->Progress(aTimer); }
}

void
DASHRepDecoder::NotifyDataArrived(const char* aBuffer,
                                  uint32_t aLength,
                                  int64_t aOffset)
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");

  LOG("Data bytes [%lld - %lld] arrived via buffer [%p].",
      aOffset, aOffset+aLength, aBuffer);
  // Notify reader directly, since call to |MediaDecoderStateMachine|::
  // |NotifyDataArrived| will go to |DASHReader|::|NotifyDataArrived|, which
  // has no way to forward the notification to the correct sub-reader.
  if (mReader) {
    mReader->NotifyDataArrived(aBuffer, aLength, aOffset);
  }
  // Forward to main decoder which will notify state machine.
  if (mMainDecoder) {
    mMainDecoder->NotifyDataArrived(aBuffer, aLength, aOffset);
  }
}

void
DASHRepDecoder::NotifyBytesDownloaded()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mMainDecoder) { mMainDecoder->NotifyBytesDownloaded(); }
}

void
DASHRepDecoder::NotifySuspendedStatusChanged()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
  if (mMainDecoder) { mMainDecoder->NotifySuspendedStatusChanged(); }
}

bool
DASHRepDecoder::OnStateMachineThread() const
{
  return (mMainDecoder ? mMainDecoder->OnStateMachineThread() : false);
}

bool
DASHRepDecoder::OnDecodeThread() const
{
  return (mMainDecoder ? mMainDecoder->OnDecodeThread() : false);
}

ReentrantMonitor&
DASHRepDecoder::GetReentrantMonitor()
{
  NS_ASSERTION(mMainDecoder, "Can't get monitor if main decoder is null!");
  if (mMainDecoder) {
    return mMainDecoder->GetReentrantMonitor();
  } else {
    // XXX If mMainDecoder is gone, most likely we're past shutdown and
    // a waiting function has been wakened. Just return this decoder's own
    // monitor and let the function complete.
    return MediaDecoder::GetReentrantMonitor();
  }
}

mozilla::layers::ImageContainer*
DASHRepDecoder::GetImageContainer()
{
  return (mMainDecoder ? mMainDecoder->GetImageContainer() : nullptr);
}

void
DASHRepDecoder::DecodeError()
{
  if (NS_IsMainThread()) {
    MediaDecoder::DecodeError();
  } else {
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &MediaDecoder::DecodeError);
    nsresult rv = NS_DispatchToMainThread(event, NS_DISPATCH_NORMAL);
    if (NS_FAILED(rv)) {
      LOG("Error dispatching DecodeError event to main thread: rv[%x]", rv);
    }
  }
}

void
DASHRepDecoder::ReleaseStateMachine()
{
  NS_ASSERTION(NS_IsMainThread(), "Must be on main thread.");

  // Since state machine owns mReader, remove reference to it.
  mReader = nullptr;

  MediaDecoder::ReleaseStateMachine();
}

void DASHRepDecoder::StopProgressUpdates()
{
  NS_ENSURE_TRUE_VOID(mMainDecoder);
  MediaDecoder::StopProgressUpdates();
}

void DASHRepDecoder::StartProgressUpdates()
{
  NS_ENSURE_TRUE_VOID(mMainDecoder);
  MediaDecoder::StartProgressUpdates();
}

} // namespace mozilla
