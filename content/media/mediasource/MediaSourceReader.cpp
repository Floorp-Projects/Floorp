/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceReader.h"

#include "prlog.h"
#include "mozilla/dom/TimeRanges.h"
#include "DecoderTraits.h"
#include "MediaDataDecodedListener.h"
#include "MediaDecoderOwner.h"
#include "MediaSourceDecoder.h"
#include "MediaSourceUtils.h"
#include "SourceBufferDecoder.h"
#include "TrackBuffer.h"

#ifdef MOZ_FMP4
#include "MP4Decoder.h"
#include "MP4Reader.h"
#endif

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

MediaSourceReader::MediaSourceReader(MediaSourceDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
  , mLastAudioTime(-1)
  , mLastVideoTime(-1)
  , mTimeThreshold(-1)
  , mDropAudioBeforeThreshold(false)
  , mDropVideoBeforeThreshold(false)
  , mEnded(false)
  , mAudioIsSeeking(false)
  , mVideoIsSeeking(false)
  , mHasEssentialTrackBuffers(false)
{
}

void
MediaSourceReader::PrepareInitialization()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("MediaSourceReader(%p)::PrepareInitialization trackBuffers=%u",
            this, mTrackBuffers.Length());
  mEssentialTrackBuffers.AppendElements(mTrackBuffers);
  mHasEssentialTrackBuffers = true;
  mDecoder->NotifyWaitingForResourcesStatusChanged();
}

bool
MediaSourceReader::IsWaitingMediaResources()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  for (uint32_t i = 0; i < mEssentialTrackBuffers.Length(); ++i) {
    if (!mEssentialTrackBuffers[i]->IsReady()) {
      return true;
    }
  }

  return !mHasEssentialTrackBuffers;
}

void
MediaSourceReader::RequestAudioData()
{
  MSE_DEBUGV("MediaSourceReader(%p)::RequestAudioData", this);
  if (!mAudioReader) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestAudioData called with no audio reader", this);
    GetCallback()->OnDecodeError();
    return;
  }
  mAudioIsSeeking = false;
  SwitchAudioReader(mLastAudioTime);
  mAudioReader->RequestAudioData();
}

void
MediaSourceReader::OnAudioDecoded(AudioData* aSample)
{
  MSE_DEBUGV("MediaSourceReader(%p)::OnAudioDecoded [mTime=%lld mDuration=%lld mDiscontinuity=%d]",
             this, aSample->mTime, aSample->mDuration, aSample->mDiscontinuity);
  if (mDropAudioBeforeThreshold) {
    if (aSample->mTime < mTimeThreshold) {
      MSE_DEBUG("MediaSourceReader(%p)::OnAudioDecoded mTime=%lld < mTimeThreshold=%lld",
                this, aSample->mTime, mTimeThreshold);
      delete aSample;
      mAudioReader->RequestAudioData();
      return;
    }
    mDropAudioBeforeThreshold = false;
  }

  // Any OnAudioDecoded callbacks received while mAudioIsSeeking must be not
  // update our last used timestamp, as these are emitted by the reader we're
  // switching away from.
  if (!mAudioIsSeeking) {
    mLastAudioTime = aSample->mTime + aSample->mDuration;
  }
  GetCallback()->OnAudioDecoded(aSample);
}

void
MediaSourceReader::OnAudioEOS()
{
  MSE_DEBUG("MediaSourceReader(%p)::OnAudioEOS reader=%p (decoders=%u)",
            this, mAudioReader.get(), mAudioTrack->Decoders().Length());
  if (SwitchAudioReader(mLastAudioTime)) {
    // Success! Resume decoding with next audio decoder.
    RequestAudioData();
  } else if (IsEnded()) {
    // End of stream.
    MSE_DEBUG("MediaSourceReader(%p)::OnAudioEOS reader=%p EOS (decoders=%u)",
              this, mAudioReader.get(), mAudioTrack->Decoders().Length());
    GetCallback()->OnAudioEOS();
  }
}

void
MediaSourceReader::RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold)
{
  MSE_DEBUGV("MediaSourceReader(%p)::RequestVideoData(%d, %lld)",
             this, aSkipToNextKeyframe, aTimeThreshold);
  if (!mVideoReader) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestVideoData called with no video reader", this);
    GetCallback()->OnDecodeError();
    return;
  }
  if (aSkipToNextKeyframe) {
    mTimeThreshold = aTimeThreshold;
    mDropAudioBeforeThreshold = true;
    mDropVideoBeforeThreshold = true;
  }
  mVideoIsSeeking = false;
  SwitchVideoReader(mLastVideoTime);
  mVideoReader->RequestVideoData(aSkipToNextKeyframe, aTimeThreshold);
}

void
MediaSourceReader::OnVideoDecoded(VideoData* aSample)
{
  MSE_DEBUGV("MediaSourceReader(%p)::OnVideoDecoded [mTime=%lld mDuration=%lld mDiscontinuity=%d]",
             this, aSample->mTime, aSample->mDuration, aSample->mDiscontinuity);
  if (mDropVideoBeforeThreshold) {
    if (aSample->mTime < mTimeThreshold) {
      MSE_DEBUG("MediaSourceReader(%p)::OnVideoDecoded mTime=%lld < mTimeThreshold=%lld",
                this, aSample->mTime, mTimeThreshold);
      delete aSample;
      mVideoReader->RequestVideoData(false, 0);
      return;
    }
    mDropVideoBeforeThreshold = false;
  }

  // Any OnVideoDecoded callbacks received while mVideoIsSeeking must be not
  // update our last used timestamp, as these are emitted by the reader we're
  // switching away from.
  if (!mVideoIsSeeking) {
    mLastVideoTime = aSample->mTime + aSample->mDuration;
  }
  GetCallback()->OnVideoDecoded(aSample);
}

void
MediaSourceReader::OnVideoEOS()
{
  // End of stream. See if we can switch to another video decoder.
  MSE_DEBUG("MediaSourceReader(%p)::OnVideoEOS reader=%p (decoders=%u)",
            this, mVideoReader.get(), mVideoTrack->Decoders().Length());
  if (SwitchVideoReader(mLastVideoTime)) {
    // Success! Resume decoding with next video decoder.
    RequestVideoData(false, 0);
  } else if (IsEnded()) {
    // End of stream.
    MSE_DEBUG("MediaSourceReader(%p)::OnVideoEOS reader=%p EOS (decoders=%u)",
              this, mVideoReader.get(), mVideoTrack->Decoders().Length());
    GetCallback()->OnVideoEOS();
  }
}

void
MediaSourceReader::OnDecodeError()
{
  MSE_DEBUG("MediaSourceReader(%p)::OnDecodeError", this);
  GetCallback()->OnDecodeError();
}

void
MediaSourceReader::Shutdown()
{
  MediaDecoderReader::Shutdown();
  mAudioTrack = nullptr;
  mAudioReader = nullptr;
  mVideoTrack = nullptr;
  mVideoReader = nullptr;
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->Shutdown();
  }
  mTrackBuffers.Clear();
}

void
MediaSourceReader::BreakCycles()
{
  MediaDecoderReader::BreakCycles();
  mAudioTrack = nullptr;
  mAudioReader = nullptr;
  mVideoTrack = nullptr;
  mVideoReader = nullptr;
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->BreakCycles();
  }
  mTrackBuffers.Clear();
}

already_AddRefed<MediaDecoderReader>
MediaSourceReader::SelectReader(int64_t aTarget,
                                const nsTArray<nsRefPtr<SourceBufferDecoder>>& aTrackDecoders)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  // Consider decoders in order of newest to oldest, as a newer decoder
  // providing a given buffered range is expected to replace an older one.
  for (int32_t i = aTrackDecoders.Length() - 1; i >= 0; --i) {
    nsRefPtr<MediaDecoderReader> newReader = aTrackDecoders[i]->GetReader();

    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    aTrackDecoders[i]->GetBuffered(ranges);
    if (ranges->Find(double(aTarget) / USECS_PER_S) == dom::TimeRanges::NoIndex) {
      MSE_DEBUGV("MediaSourceReader(%p)::SelectReader(%lld) newReader=%p target not in ranges=%s",
                 this, aTarget, newReader.get(), DumpTimeRanges(ranges).get());
      continue;
    }

    return newReader.forget();
  }

  return nullptr;
}

bool
MediaSourceReader::SwitchAudioReader(int64_t aTarget)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding an audio track after ReadMetadata.
  if (!mAudioTrack) {
    return false;
  }
  nsRefPtr<MediaDecoderReader> newReader = SelectReader(aTarget, mAudioTrack->Decoders());
  if (newReader && newReader != mAudioReader) {
    mAudioReader->SetIdle();
    mAudioReader = newReader;
    MSE_DEBUGV("MediaSourceReader(%p)::SwitchAudioReader switched reader to %p", this, mAudioReader.get());
    return true;
  }
  return false;
}

bool
MediaSourceReader::SwitchVideoReader(int64_t aTarget)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding a video track after ReadMetadata.
  if (!mVideoTrack) {
    return false;
  }
  nsRefPtr<MediaDecoderReader> newReader = SelectReader(aTarget, mVideoTrack->Decoders());
  if (newReader && newReader != mVideoReader) {
    mVideoReader->SetIdle();
    mVideoReader = newReader;
    MSE_DEBUGV("MediaSourceReader(%p)::SwitchVideoReader switched reader to %p", this, mVideoReader.get());
    return true;
  }
  return false;
}

MediaDecoderReader*
CreateReaderForType(const nsACString& aType, AbstractMediaDecoder* aDecoder)
{
#ifdef MOZ_FMP4
  // The MP4Reader that supports fragmented MP4 and uses
  // PlatformDecoderModules is hidden behind prefs for regular video
  // elements, but we always want to use it for MSE, so instantiate it
  // directly here.
  if ((aType.LowerCaseEqualsLiteral("video/mp4") ||
       aType.LowerCaseEqualsLiteral("audio/mp4")) &&
      MP4Decoder::IsEnabled()) {
    return new MP4Reader(aDecoder);
  }
#endif
  return DecoderTraits::CreateReader(aType, aDecoder);
}

already_AddRefed<SourceBufferDecoder>
MediaSourceReader::CreateSubDecoder(const nsACString& aType)
{
  if (IsShutdown()) {
    return nullptr;
  }
  MOZ_ASSERT(GetTaskQueue());
  nsRefPtr<SourceBufferDecoder> decoder =
    new SourceBufferDecoder(new SourceBufferResource(aType), mDecoder);
  nsRefPtr<MediaDecoderReader> reader(CreateReaderForType(aType, decoder));
  if (!reader) {
    return nullptr;
  }
  // Set a callback on the subreader that forwards calls to this reader.
  // This reader will then forward them onto the state machine via this
  // reader's callback.
  RefPtr<MediaDataDecodedListener<MediaSourceReader>> callback =
    new MediaDataDecodedListener<MediaSourceReader>(this, GetTaskQueue());
  reader->SetCallback(callback);
  reader->SetTaskQueue(GetTaskQueue());
  reader->Init(nullptr);

  MSE_DEBUG("MediaSourceReader(%p)::CreateSubDecoder subdecoder %p subreader %p",
            this, decoder.get(), reader.get());
  decoder->SetReader(reader);
#ifdef MOZ_EME
  decoder->SetCDMProxy(mCDMProxy);
#endif
  return decoder.forget();
}

void
MediaSourceReader::AddTrackBuffer(TrackBuffer* aTrackBuffer)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("MediaSourceReader(%p)::AddTrackBuffer %p", this, aTrackBuffer);
  mTrackBuffers.AppendElement(aTrackBuffer);
}

void
MediaSourceReader::RemoveTrackBuffer(TrackBuffer* aTrackBuffer)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("MediaSourceReader(%p)::RemoveTrackBuffer %p", this, aTrackBuffer);
  mTrackBuffers.RemoveElement(aTrackBuffer);
  if (mAudioTrack == aTrackBuffer) {
    mAudioTrack = nullptr;
  }
  if (mVideoTrack == aTrackBuffer) {
    mVideoTrack = nullptr;
  }
}

void
MediaSourceReader::OnTrackBufferConfigured(TrackBuffer* aTrackBuffer, const MediaInfo& aInfo)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(aTrackBuffer->IsReady());
  MOZ_ASSERT(mTrackBuffers.Contains(aTrackBuffer));
  if (aInfo.HasAudio() && !mAudioTrack) {
    MSE_DEBUG("MediaSourceReader(%p)::OnTrackBufferConfigured %p audio", this, aTrackBuffer);
    mAudioTrack = aTrackBuffer;
  }
  if (aInfo.HasVideo() && !mVideoTrack) {
    MSE_DEBUG("MediaSourceReader(%p)::OnTrackBufferConfigured %p video", this, aTrackBuffer);
    mVideoTrack = aTrackBuffer;
  }
  mDecoder->NotifyWaitingForResourcesStatusChanged();
}

void
MediaSourceReader::WaitForTimeRange(int64_t aTime)
{
  MSE_DEBUG("MediaSourceReader(%p)::WaitForTimeRange(%lld)", this, aTime);
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  // Loop until we have the requested time range in the active TrackBuffers.
  // Ideally, this wait loop would use an async request and callback
  // instead.  Bug 1056441 covers that change.
  while (!TrackBuffersContainTime(aTime) && !IsShutdown() && !IsEnded()) {
    MSE_DEBUG("MediaSourceReader(%p)::WaitForTimeRange(%lld) waiting", this, aTime);
    mon.Wait();
  }
}

bool
MediaSourceReader::TrackBuffersContainTime(int64_t aTime)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mAudioTrack && !mAudioTrack->ContainsTime(aTime)) {
    return false;
  }
  if (mVideoTrack && !mVideoTrack->ContainsTime(aTime)) {
    return false;
  }
  return true;
}

nsresult
MediaSourceReader::Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime)
{
  MSE_DEBUG("MediaSourceReader(%p)::Seek(aTime=%lld, aStart=%lld, aEnd=%lld, aCurrent=%lld)",
            this, aTime, aStartTime, aEndTime, aCurrentTime);

  ResetDecode();
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->ResetDecode();
  }

  // Decoding discontinuity upon seek, reset last times to seek target.
  mLastAudioTime = aTime;
  mLastVideoTime = aTime;

  WaitForTimeRange(aTime);

  if (IsShutdown()) {
    return NS_ERROR_FAILURE;
  }

  if (mAudioTrack) {
    mAudioIsSeeking = true;
    SwitchAudioReader(aTime);
    nsresult rv = mAudioReader->Seek(aTime, aStartTime, aEndTime, aCurrentTime);
    MSE_DEBUG("MediaSourceReader(%p)::Seek audio reader=%p rv=%x", this, mAudioReader.get(), rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  if (mVideoTrack) {
    mVideoIsSeeking = true;
    SwitchVideoReader(aTime);
    nsresult rv = mVideoReader->Seek(aTime, aStartTime, aEndTime, aCurrentTime);
    MSE_DEBUG("MediaSourceReader(%p)::Seek video reader=%p rv=%x", this, mVideoReader.get(), rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata tracks=%u/%u audio=%p video=%p",
            this, mEssentialTrackBuffers.Length(), mTrackBuffers.Length(),
            mAudioTrack.get(), mVideoTrack.get());

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mEssentialTrackBuffers.Clear();
  }
  if (!mAudioTrack && !mVideoTrack) {
    MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata missing track: mAudioTrack=%p mVideoTrack=%p",
              this, mAudioTrack.get(), mVideoTrack.get());
    return NS_ERROR_FAILURE;
  }

  int64_t maxDuration = -1;

  if (mAudioTrack) {
    MOZ_ASSERT(mAudioTrack->IsReady());
    mAudioReader = mAudioTrack->Decoders()[0]->GetReader();

    const MediaInfo& info = mAudioReader->GetMediaInfo();
    MOZ_ASSERT(info.HasAudio());
    mInfo.mAudio = info.mAudio;
    maxDuration = std::max(maxDuration, mAudioReader->GetDecoder()->GetMediaDuration());
    MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata audio reader=%p maxDuration=%lld",
              this, mAudioReader.get(), maxDuration);
  }

  if (mVideoTrack) {
    MOZ_ASSERT(mVideoTrack->IsReady());
    mVideoReader = mVideoTrack->Decoders()[0]->GetReader();

    const MediaInfo& info = mVideoReader->GetMediaInfo();
    MOZ_ASSERT(info.HasVideo());
    mInfo.mVideo = info.mVideo;
    maxDuration = std::max(maxDuration, mVideoReader->GetDecoder()->GetMediaDuration());
    MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata video reader=%p maxDuration=%lld",
              this, mVideoReader.get(), maxDuration);
  }

  if (maxDuration != -1) {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mDecoder->SetMediaDuration(maxDuration);
    nsRefPtr<nsIRunnable> task (
      NS_NewRunnableMethodWithArg<double>(static_cast<MediaSourceDecoder*>(mDecoder),
                                          &MediaSourceDecoder::SetMediaSourceDuration,
                                          static_cast<double>(maxDuration) / USECS_PER_S));
    NS_DispatchToMainThread(task);
  }

  *aInfo = mInfo;
  *aTags = nullptr; // TODO: Handle metadata.

  return NS_OK;
}

void
MediaSourceReader::Ended()
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEnded = true;
}

bool
MediaSourceReader::IsEnded()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  return mEnded;
}

#ifdef MOZ_EME
nsresult
MediaSourceReader::SetCDMProxy(CDMProxy* aProxy)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  mCDMProxy = aProxy;
  for (size_t i = 0; i < mTrackBuffers.Length(); i++) {
    nsresult rv = mTrackBuffers[i]->SetCDMProxy(aProxy);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}
#endif

} // namespace mozilla
