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
#include "SharedDecoderManager.h"
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

// When a stream hits EOS it needs to decide what other stream to switch to. Due
// to inaccuracies is determining buffer end frames (Bug 1065207) and rounding
// issues we use a fuzz factor to determine the end time of this stream for
// switching to the new stream. This value is based on the end of frame
// default value used in Blink, kDefaultBufferDurationInMs.
#define EOS_FUZZ_US 125000

using mozilla::dom::TimeRanges;

namespace mozilla {

MediaSourceReader::MediaSourceReader(MediaSourceDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
  , mLastAudioTime(-1)
  , mLastVideoTime(-1)
  , mPendingSeekTime(-1)
  , mPendingStartTime(-1)
  , mPendingEndTime(-1)
  , mPendingCurrentTime(-1)
  , mWaitingForSeekData(false)
  , mPendingSeeks(0)
  , mSeekResult(NS_OK)
  , mTimeThreshold(-1)
  , mDropAudioBeforeThreshold(false)
  , mDropVideoBeforeThreshold(false)
  , mEnded(false)
  , mAudioIsSeeking(false)
  , mVideoIsSeeking(false)
  , mHasEssentialTrackBuffers(false)
#ifdef MOZ_FMP4
  , mSharedDecoderManager(new SharedDecoderManager())
#endif
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
    GetCallback()->OnNotDecoded(MediaData::AUDIO_DATA, DECODE_ERROR);
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
MediaSourceReader::RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold)
{
  MSE_DEBUGV("MediaSourceReader(%p)::RequestVideoData(%d, %lld)",
             this, aSkipToNextKeyframe, aTimeThreshold);
  if (!mVideoReader) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestVideoData called with no video reader", this);
    GetCallback()->OnNotDecoded(MediaData::VIDEO_DATA, DECODE_ERROR);
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
MediaSourceReader::OnNotDecoded(MediaData::Type aType, NotDecodedReason aReason)
{
  MSE_DEBUG("MediaSourceReader(%p)::OnNotDecoded aType=%u aReason=%u IsEnded: %d", this, aType, aReason, IsEnded());
  if (aReason == DECODE_ERROR || aReason == CANCELED) {
    GetCallback()->OnNotDecoded(aType, aReason);
    return;
  }
  // End of stream. Force switching past this stream to another reader by
  // switching to the end of the buffered range.
  MOZ_ASSERT(aReason == END_OF_STREAM);
  nsRefPtr<MediaDecoderReader> reader = aType == MediaData::AUDIO_DATA ?
                                          mAudioReader : mVideoReader;

  // Find the closest approximation to the end time for this stream.
  // mLast{Audio,Video}Time differs from the actual end time because of
  // Bug 1065207 - the duration of a WebM fragment is an estimate not the
  // actual duration. In the case of audio time an example of where they
  // differ would be the actual sample duration being small but the
  // previous sample being large. The buffered end time uses that last
  // sample duration as an estimate of the end time duration giving an end
  // time that is greater than mLastAudioTime, which is the actual sample
  // end time.
  // Reader switching is based on the buffered end time though so they can be
  // quite different. By using the EOS_FUZZ_US and the buffered end time we
  // attempt to account for this difference.
  int64_t* time = aType == MediaData::AUDIO_DATA ? &mLastAudioTime : &mLastVideoTime;
  if (reader) {
    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    reader->GetBuffered(ranges);
    if (ranges->Length() > 0) {
      // End time is a double so we convert to nearest by adding 0.5.
      int64_t end = ranges->GetEndTime() * USECS_PER_S + 0.5;
      *time = std::max(*time, end);
    }
  }

  // See if we can find a different reader that can pick up where we left off. We use the
  // EOS_FUZZ_US to allow for the fact that our end time can be inaccurate due to bug
  // 1065207 - the duration of a WebM frame is an estimate.
  if (aType == MediaData::AUDIO_DATA && SwitchAudioReader(*time + EOS_FUZZ_US)) {
    RequestAudioData();
    return;
  }
  if (aType == MediaData::VIDEO_DATA && SwitchVideoReader(*time + EOS_FUZZ_US)) {
    RequestVideoData(false, 0);
    return;
  }

  // If the entire MediaSource is done, generate an EndOfStream.
  if (IsEnded()) {
    GetCallback()->OnNotDecoded(aType, END_OF_STREAM);
    return;
  }

  // We don't have the data the caller wants. Tell that we're waiting for JS to
  // give us more data.
  GetCallback()->OnNotDecoded(aType, WAITING_FOR_DATA);
}

void
MediaSourceReader::Shutdown()
{
  MediaDecoderReader::Shutdown();
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->Shutdown();
  }
  mAudioTrack = nullptr;
  mAudioReader = nullptr;
  mVideoTrack = nullptr;
  mVideoReader = nullptr;
}

void
MediaSourceReader::BreakCycles()
{
  MediaDecoderReader::BreakCycles();

  // These were cleared in Shutdown().
  MOZ_ASSERT(!mAudioTrack);
  MOZ_ASSERT(!mAudioReader);
  MOZ_ASSERT(!mVideoTrack);
  MOZ_ASSERT(!mVideoReader);

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

  // MSE uses a start time of 0 everywhere. Set that immediately on the
  // subreader to make sure that it's always in a state where we can invoke
  // GetBuffered on it.
  {
    ReentrantMonitorAutoEnter mon(decoder->GetReentrantMonitor());
    reader->SetStartTime(0);
  }

  // This part is icky. It would be nicer to just give each subreader its own
  // task queue. Unfortunately though, Request{Audio,Video}Data implementations
  // currently assert that they're on "the decode thread", and so having
  // separate task queues makes MediaSource stuff unnecessarily cumbersome. We
  // should remove the need for these assertions (which probably involves making
  // all Request*Data implementations fully async), and then get rid of the
  // borrowing.
  reader->SetBorrowedTaskQueue(GetTaskQueue());

  // Set a callback on the subreader that forwards calls to this reader.
  // This reader will then forward them onto the state machine via this
  // reader's callback.
  RefPtr<MediaDataDecodedListener<MediaSourceReader>> callback =
    new MediaDataDecodedListener<MediaSourceReader>(this, reader->GetTaskQueue());
  reader->SetCallback(callback);

#ifdef MOZ_FMP4
  reader->SetSharedDecoderManager(mSharedDecoderManager);
#endif
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

void
MediaSourceReader::NotifyTimeRangesChanged()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mWaitingForSeekData) {
    //post a task to the state machine thread to call seek.
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(
        this, &MediaSourceReader::AttemptSeek));
    GetTaskQueue()->Dispatch(task.forget());
  }
}

void
MediaSourceReader::Seek(int64_t aTime, int64_t aStartTime, int64_t aEndTime,
                        int64_t aCurrentTime)
{
  MSE_DEBUG("MediaSourceReader(%p)::Seek(aTime=%lld, aStart=%lld, aEnd=%lld, aCurrent=%lld)",
            this, aTime, aStartTime, aEndTime, aCurrentTime);

  if (IsShutdown()) {
    GetCallback()->OnSeekCompleted(NS_ERROR_FAILURE);
    return;
  }

  // Store pending seek target in case the track buffers don't contain
  // the desired time and we delay doing the seek.
  mPendingSeekTime = aTime;
  mPendingStartTime = aStartTime;
  mPendingEndTime = aEndTime;
  mPendingCurrentTime = aCurrentTime;

  // Only increment the number of expected OnSeekCompleted
  // notifications if we weren't already waiting for AttemptSeek
  // to complete (and they would have been accounted for already).
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

    if (!mWaitingForSeekData) {
      mWaitingForSeekData = true;
      if (mAudioTrack) {
        mPendingSeeks++;
      }
      if (mVideoTrack) {
        mPendingSeeks++;
      }
    }
  }

  AttemptSeek();
}

void
MediaSourceReader::OnSeekCompleted(nsresult aResult)
{
  mPendingSeeks--;
  // Keep the most recent failed result (if any)
  if (NS_FAILED(aResult)) {
    mSeekResult = aResult;
  }
  // Only dispatch the final event onto the state machine
  // since it's only expecting one response.
  if (!mPendingSeeks) {
    GetCallback()->OnSeekCompleted(mSeekResult);
    mSeekResult = NS_OK;
  }
}

void
MediaSourceReader::AttemptSeek()
{
  // Make sure we don't hold the monitor while calling into the reader
  // Seek methods since it can deadlock.
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    if (!mWaitingForSeekData || !TrackBuffersContainTime(mPendingSeekTime)) {
      return;
    }
  }

  ResetDecode();
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->ResetDecode();
  }

  // Decoding discontinuity upon seek, reset last times to seek target.
  mLastAudioTime = mPendingSeekTime;
  mLastVideoTime = mPendingSeekTime;

  if (mAudioTrack) {
    mAudioIsSeeking = true;
    SwitchAudioReader(mPendingSeekTime);
    mAudioReader->Seek(mPendingSeekTime,
                       mPendingStartTime,
                       mPendingEndTime,
                       mPendingCurrentTime);
    MSE_DEBUG("MediaSourceReader(%p)::Seek audio reader=%p", this, mAudioReader.get());
  }
  if (mVideoTrack) {
    mVideoIsSeeking = true;
    SwitchVideoReader(mPendingSeekTime);
    mVideoReader->Seek(mPendingSeekTime,
                       mPendingStartTime,
                       mPendingEndTime,
                       mPendingCurrentTime);
    MSE_DEBUG("MediaSourceReader(%p)::Seek video reader=%p", this, mVideoReader.get());
  }
  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mWaitingForSeekData = false;
  }
}

nsresult
MediaSourceReader::GetBuffered(dom::TimeRanges* aBuffered)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(aBuffered->Length() == 0);
  if (mTrackBuffers.IsEmpty()) {
    return NS_OK;
  }

  double highestEndTime = 0;

  nsTArray<nsRefPtr<TimeRanges>> activeRanges;
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    nsRefPtr<TimeRanges> r = new TimeRanges();
    mTrackBuffers[i]->Buffered(r);
    activeRanges.AppendElement(r);
    highestEndTime = std::max(highestEndTime, activeRanges.LastElement()->GetEndTime());
  }

  TimeRanges* intersectionRanges = aBuffered;
  intersectionRanges->Add(0, highestEndTime);

  for (uint32_t i = 0; i < activeRanges.Length(); ++i) {
    TimeRanges* sourceRanges = activeRanges[i];

    if (IsEnded()) {
      // Set the end time on the last range to highestEndTime by adding a
      // new range spanning the current end time to highestEndTime, which
      // Normalize() will then merge with the old last range.
      sourceRanges->Add(sourceRanges->GetEndTime(), highestEndTime);
      sourceRanges->Normalize();
    }

    intersectionRanges->Intersection(sourceRanges);
  }

  MSE_DEBUG("MediaSourceReader(%p)::GetBuffered ranges=%s", this, DumpTimeRanges(intersectionRanges).get());
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
    static_cast<MediaSourceDecoder*>(mDecoder)->SetDecodedDuration(maxDuration);
  }

  *aInfo = mInfo;
  *aTags = nullptr; // TODO: Handle metadata.

  return NS_OK;
}

void
MediaSourceReader::ReadUpdatedMetadata(MediaInfo* aInfo)
{
  if (mAudioTrack) {
    MOZ_ASSERT(mAudioTrack->IsReady());
    mAudioReader = mAudioTrack->Decoders()[0]->GetReader();

    const MediaInfo& info = mAudioReader->GetMediaInfo();
    MOZ_ASSERT(info.HasAudio());
    mInfo.mAudio = info.mAudio;
  }

  if (mVideoTrack) {
    MOZ_ASSERT(mVideoTrack->IsReady());
    mVideoReader = mVideoTrack->Decoders()[0]->GetReader();

    const MediaInfo& info = mVideoReader->GetMediaInfo();
    MOZ_ASSERT(info.HasVideo());
    mInfo.mVideo = info.mVideo;
  }
  *aInfo = mInfo;
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
