/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceReader.h"

#include "prlog.h"
#include "mozilla/dom/TimeRanges.h"
#include "DecoderTraits.h"
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
  , mLastAudioTime(0)
  , mLastVideoTime(0)
  , mPendingSeekTime(-1)
  , mWaitingForSeekData(false)
  , mAudioIsSeeking(false)
  , mVideoIsSeeking(false)
  , mTimeThreshold(-1)
  , mDropAudioBeforeThreshold(false)
  , mDropVideoBeforeThreshold(false)
  , mEnded(false)
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

size_t
MediaSourceReader::SizeOfVideoQueueInFrames()
{
  if (!mVideoReader) {
    MSE_DEBUG("MediaSourceReader(%p)::SizeOfVideoQueue called with no video reader", this);
    return 0;
  }
  return mVideoReader->SizeOfVideoQueueInFrames();
}

size_t
MediaSourceReader::SizeOfAudioQueueInFrames()
{
  if (!mAudioReader) {
    MSE_DEBUG("MediaSourceReader(%p)::SizeOfAudioQueue called with no audio reader", this);
    return 0;
  }
  return mAudioReader->SizeOfAudioQueueInFrames();
}

nsRefPtr<MediaDecoderReader::AudioDataPromise>
MediaSourceReader::RequestAudioData()
{
  nsRefPtr<AudioDataPromise> p = mAudioPromise.Ensure(__func__);
  MSE_DEBUGV("MediaSourceReader(%p)::RequestAudioData", this);
  if (!mAudioReader) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestAudioData called with no audio reader", this);
    mAudioPromise.Reject(DECODE_ERROR, __func__);
    return p;
  }
  if (mAudioIsSeeking) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestAudioData called mid-seek. Rejecting.", this);
    mAudioPromise.Reject(CANCELED, __func__);
    return p;
  }
  if (SwitchAudioReader(mLastAudioTime)) {
    mAudioReader->Seek(mLastAudioTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::RequestAudioDataComplete,
                       &MediaSourceReader::RequestAudioDataFailed);
  } else {
    RequestAudioDataComplete(0);
  }
  return p;
}

void
MediaSourceReader::RequestAudioDataComplete(int64_t aTime)
{
  mAudioReader->RequestAudioData()->Then(GetTaskQueue(), __func__, this,
                                         &MediaSourceReader::OnAudioDecoded,
                                         &MediaSourceReader::OnAudioNotDecoded);
}

void
MediaSourceReader::RequestAudioDataFailed(nsresult aResult)
{
  OnAudioNotDecoded(DECODE_ERROR);
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
      mAudioReader->RequestAudioData()->Then(GetTaskQueue(), __func__, this,
                                             &MediaSourceReader::OnAudioDecoded,
                                             &MediaSourceReader::OnAudioNotDecoded);
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

  mAudioPromise.Resolve(aSample, __func__);
}

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
static void
AdjustEndTime(int64_t* aEndTime, MediaDecoderReader* aReader)
{
  if (aReader) {
    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    aReader->GetBuffered(ranges);
    if (ranges->Length() > 0) {
      // End time is a double so we convert to nearest by adding 0.5.
      int64_t end = ranges->GetEndTime() * USECS_PER_S + 0.5;
      *aEndTime = std::max(*aEndTime, end);
    }
  }
}

void
MediaSourceReader::OnAudioNotDecoded(NotDecodedReason aReason)
{
  MSE_DEBUG("MediaSourceReader(%p)::OnAudioNotDecoded aReason=%u IsEnded: %d", this, aReason, IsEnded());
  if (aReason == DECODE_ERROR || aReason == CANCELED) {
    mAudioPromise.Reject(aReason, __func__);
    return;
  }

  // End of stream. Force switching past this stream to another reader by
  // switching to the end of the buffered range.
  MOZ_ASSERT(aReason == END_OF_STREAM);
  if (mAudioReader) {
    AdjustEndTime(&mLastAudioTime, mAudioReader);
  }

  // See if we can find a different reader that can pick up where we left off. We use the
  // EOS_FUZZ_US to allow for the fact that our end time can be inaccurate due to bug
  // 1065207.
  if (SwitchAudioReader(mLastAudioTime, EOS_FUZZ_US)) {
    mAudioReader->Seek(mLastAudioTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::RequestAudioDataComplete,
                       &MediaSourceReader::RequestAudioDataFailed);
    return;
  }

  // If the entire MediaSource is done, generate an EndOfStream.
  if (IsEnded()) {
    mAudioPromise.Reject(END_OF_STREAM, __func__);
    return;
  }

  // We don't have the data the caller wants. Tell that we're waiting for JS to
  // give us more data.
  mAudioPromise.Reject(WAITING_FOR_DATA, __func__);
}


nsRefPtr<MediaDecoderReader::VideoDataPromise>
MediaSourceReader::RequestVideoData(bool aSkipToNextKeyframe, int64_t aTimeThreshold)
{
  nsRefPtr<VideoDataPromise> p = mVideoPromise.Ensure(__func__);
  MSE_DEBUGV("MediaSourceReader(%p)::RequestVideoData(%d, %lld)",
             this, aSkipToNextKeyframe, aTimeThreshold);
  if (!mVideoReader) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestVideoData called with no video reader", this);
    mVideoPromise.Reject(DECODE_ERROR, __func__);
    return p;
  }
  if (aSkipToNextKeyframe) {
    mTimeThreshold = aTimeThreshold;
    mDropAudioBeforeThreshold = true;
    mDropVideoBeforeThreshold = true;
  }
  if (mVideoIsSeeking) {
    MSE_DEBUG("MediaSourceReader(%p)::RequestVideoData called mid-seek. Rejecting.", this);
    mVideoPromise.Reject(CANCELED, __func__);
    return p;
  }
  if (SwitchVideoReader(mLastVideoTime)) {
    mVideoReader->Seek(mLastVideoTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::RequestVideoDataComplete,
                       &MediaSourceReader::RequestVideoDataFailed);
  } else {
    mVideoReader->RequestVideoData(aSkipToNextKeyframe, aTimeThreshold)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::OnVideoDecoded, &MediaSourceReader::OnVideoNotDecoded);
  }

  return p;
}

void
MediaSourceReader::RequestVideoDataComplete(int64_t aTime)
{
  mVideoReader->RequestVideoData(false, 0)
              ->Then(GetTaskQueue(), __func__, this,
                     &MediaSourceReader::OnVideoDecoded, &MediaSourceReader::OnVideoNotDecoded);
}

void
MediaSourceReader::RequestVideoDataFailed(nsresult aResult)
{
  OnVideoNotDecoded(DECODE_ERROR);
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
      mVideoReader->RequestVideoData(false, 0)->Then(GetTaskQueue(), __func__, this,
                                                     &MediaSourceReader::OnVideoDecoded,
                                                     &MediaSourceReader::OnVideoNotDecoded);
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

  mVideoPromise.Resolve(aSample, __func__);
}

void
MediaSourceReader::OnVideoNotDecoded(NotDecodedReason aReason)
{
  MSE_DEBUG("MediaSourceReader(%p)::OnVideoNotDecoded aReason=%u IsEnded: %d", this, aReason, IsEnded());
  if (aReason == DECODE_ERROR || aReason == CANCELED) {
    mVideoPromise.Reject(aReason, __func__);
    return;
  }

  // End of stream. Force switching past this stream to another reader by
  // switching to the end of the buffered range.
  MOZ_ASSERT(aReason == END_OF_STREAM);
  if (mVideoReader) {
    AdjustEndTime(&mLastVideoTime, mVideoReader);
  }

  // See if we can find a different reader that can pick up where we left off. We use the
  // EOS_FUZZ_US to allow for the fact that our end time can be inaccurate due to bug
  // 1065207.
  if (SwitchVideoReader(mLastVideoTime, EOS_FUZZ_US)) {
    mVideoReader->Seek(mLastVideoTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::RequestVideoDataComplete,
                       &MediaSourceReader::RequestVideoDataFailed);
    return;
  }

  // If the entire MediaSource is done, generate an EndOfStream.
  if (IsEnded()) {
    mVideoPromise.Reject(END_OF_STREAM, __func__);
    return;
  }

  // We don't have the data the caller wants. Tell that we're waiting for JS to
  // give us more data.
  mVideoPromise.Reject(WAITING_FOR_DATA, __func__);
}

nsRefPtr<ShutdownPromise>
MediaSourceReader::Shutdown()
{
  mSeekPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);

  MOZ_ASSERT(mMediaSourceShutdownPromise.IsEmpty());
  nsRefPtr<ShutdownPromise> p = mMediaSourceShutdownPromise.Ensure(__func__);

  ContinueShutdown();
  return p;
}

void
MediaSourceReader::ContinueShutdown()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mTrackBuffers.Length()) {
    mTrackBuffers[0]->Shutdown()->Then(GetTaskQueue(), __func__, this,
                                       &MediaSourceReader::ContinueShutdown,
                                       &MediaSourceReader::ContinueShutdown);
    mShutdownTrackBuffers.AppendElement(mTrackBuffers[0]);
    mTrackBuffers.RemoveElementAt(0);
    return;
  }

  mAudioTrack = nullptr;
  mAudioReader = nullptr;
  mVideoTrack = nullptr;
  mVideoReader = nullptr;

  MOZ_ASSERT(mAudioPromise.IsEmpty());
  MOZ_ASSERT(mVideoPromise.IsEmpty());

  mAudioWaitPromise.RejectIfExists(WaitForDataRejectValue(MediaData::AUDIO_DATA, WaitForDataRejectValue::SHUTDOWN), __func__);
  mVideoWaitPromise.RejectIfExists(WaitForDataRejectValue(MediaData::VIDEO_DATA, WaitForDataRejectValue::SHUTDOWN), __func__);

  MediaDecoderReader::Shutdown()->ChainTo(mMediaSourceShutdownPromise.Steal(), __func__);
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
  MOZ_ASSERT(!mTrackBuffers.Length());

  for (uint32_t i = 0; i < mShutdownTrackBuffers.Length(); ++i) {
    mShutdownTrackBuffers[i]->BreakCycles();
  }
  mShutdownTrackBuffers.Clear();
}

already_AddRefed<MediaDecoderReader>
MediaSourceReader::SelectReader(int64_t aTarget,
                                int64_t aError,
                                const nsTArray<nsRefPtr<SourceBufferDecoder>>& aTrackDecoders)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();

  // Consider decoders in order of newest to oldest, as a newer decoder
  // providing a given buffered range is expected to replace an older one.
  for (int32_t i = aTrackDecoders.Length() - 1; i >= 0; --i) {
    nsRefPtr<MediaDecoderReader> newReader = aTrackDecoders[i]->GetReader();

    nsRefPtr<dom::TimeRanges> ranges = new dom::TimeRanges();
    aTrackDecoders[i]->GetBuffered(ranges);
    if (ranges->Find(double(aTarget) / USECS_PER_S,
                     double(aError) / USECS_PER_S) == dom::TimeRanges::NoIndex) {
      MSE_DEBUGV("MediaSourceReader(%p)::SelectReader(%lld) newReader=%p target not in ranges=%s",
                 this, aTarget, newReader.get(), DumpTimeRanges(ranges).get());
      continue;
    }

    return newReader.forget();
  }

  return nullptr;
}

bool
MediaSourceReader::HaveData(int64_t aTarget, MediaData::Type aType)
{
  TrackBuffer* trackBuffer = aType == MediaData::AUDIO_DATA ? mAudioTrack : mVideoTrack;
  MOZ_ASSERT(trackBuffer);
  nsRefPtr<MediaDecoderReader> reader = SelectReader(aTarget, EOS_FUZZ_US, trackBuffer->Decoders());
  return !!reader;
}

bool
MediaSourceReader::SwitchAudioReader(int64_t aTarget, int64_t aError)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding an audio track after ReadMetadata.
  if (!mAudioTrack) {
    return false;
  }
  nsRefPtr<MediaDecoderReader> newReader = SelectReader(aTarget, aError, mAudioTrack->Decoders());
  if (newReader && newReader != mAudioReader) {
    mAudioReader->SetIdle();
    mAudioReader = newReader;
    MSE_DEBUGV("MediaSourceReader(%p)::SwitchAudioReader switched reader to %p", this, mAudioReader.get());
    return true;
  }
  return false;
}

bool
MediaSourceReader::SwitchVideoReader(int64_t aTarget, int64_t aError)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding a video track after ReadMetadata.
  if (!mVideoTrack) {
    return false;
  }
  nsRefPtr<MediaDecoderReader> newReader = SelectReader(aTarget, aError, mVideoTrack->Decoders());
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
MediaSourceReader::CreateSubDecoder(const nsACString& aType, int64_t aTimestampOffset)
{
  if (IsShutdown()) {
    return nullptr;
  }
  MOZ_ASSERT(GetTaskQueue());
  nsRefPtr<SourceBufferDecoder> decoder =
    new SourceBufferDecoder(new SourceBufferResource(aType), mDecoder, aTimestampOffset);
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

nsRefPtr<MediaDecoderReader::SeekPromise>
MediaSourceReader::Seek(int64_t aTime, int64_t aIgnored /* Used only for ogg which is non-MSE */)
{
  MSE_DEBUG("MediaSourceReader(%p)::Seek(aTime=%lld, aEnd=%lld, aCurrent=%lld)",
            this, aTime);

  MOZ_ASSERT(mSeekPromise.IsEmpty());
  nsRefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);

  if (IsShutdown()) {
    mSeekPromise.Reject(NS_ERROR_FAILURE, __func__);
    return p;
  }

  // Store pending seek target in case the track buffers don't contain
  // the desired time and we delay doing the seek.
  mPendingSeekTime = aTime;

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mWaitingForSeekData = true;
  }

  AttemptSeek();
  return p;
}

void
MediaSourceReader::CancelSeek()
{
  MOZ_ASSERT(OnDecodeThread());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mWaitingForSeekData) {
    mSeekPromise.Reject(NS_OK, __func__);
    mWaitingForSeekData = false;
    mPendingSeekTime = -1;
  } else if (mVideoIsSeeking) {
    // NB: Currently all readers have sync Seeks(), so this is a no-op.
    mVideoReader->CancelSeek();
  } else if (mAudioIsSeeking) {
    // NB: Currently all readers have sync Seeks(), so this is a no-op.
    mAudioReader->CancelSeek();
  } else {
    MOZ_ASSERT(mSeekPromise.IsEmpty());
  }
}

void
MediaSourceReader::OnVideoSeekCompleted(int64_t aTime)
{
  mPendingSeekTime = aTime;
  MOZ_ASSERT(mVideoIsSeeking);
  MOZ_ASSERT(!mAudioIsSeeking);
  mVideoIsSeeking = false;

  if (mAudioTrack) {
    mAudioIsSeeking = true;
    SwitchAudioReader(mPendingSeekTime);
    mAudioReader->Seek(mPendingSeekTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::OnAudioSeekCompleted,
                       &MediaSourceReader::OnSeekFailed);
    MSE_DEBUG("MediaSourceReader(%p)::Seek audio reader=%p", this, mAudioReader.get());
    return;
  }
  mSeekPromise.Resolve(mPendingSeekTime, __func__);
}

void
MediaSourceReader::OnAudioSeekCompleted(int64_t aTime)
{
  mPendingSeekTime = aTime;
  MOZ_ASSERT(mAudioIsSeeking);
  MOZ_ASSERT(!mVideoIsSeeking);
  mAudioIsSeeking = false;

  mSeekPromise.Resolve(mPendingSeekTime, __func__);
  mPendingSeekTime = -1;
}

void
MediaSourceReader::OnSeekFailed(nsresult aResult)
{
  MOZ_ASSERT(mVideoIsSeeking || mAudioIsSeeking);

  if (mVideoIsSeeking) {
    MOZ_ASSERT(!mAudioIsSeeking);
    mVideoIsSeeking = false;
  }

  if (mAudioIsSeeking) {
    MOZ_ASSERT(!mVideoIsSeeking);
    mAudioIsSeeking = false;
  }

  mPendingSeekTime = -1;
  mSeekPromise.Reject(aResult, __func__);
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
    mWaitingForSeekData = false;
  }

  ResetDecode();
  for (uint32_t i = 0; i < mTrackBuffers.Length(); ++i) {
    mTrackBuffers[i]->ResetDecode();
  }

  // Decoding discontinuity upon seek, reset last times to seek target.
  mLastAudioTime = mPendingSeekTime;
  mLastVideoTime = mPendingSeekTime;

  if (mVideoTrack) {
    mVideoIsSeeking = true;
    SwitchVideoReader(mPendingSeekTime);
    mVideoReader->Seek(mPendingSeekTime, 0)
                ->Then(GetTaskQueue(), __func__, this,
                       &MediaSourceReader::OnVideoSeekCompleted,
                       &MediaSourceReader::OnSeekFailed);
    MSE_DEBUG("MediaSourceReader(%p)::Seek video reader=%p", this, mVideoReader.get());
  } else {
    OnVideoSeekCompleted(mPendingSeekTime);
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

nsRefPtr<MediaDecoderReader::WaitForDataPromise>
MediaSourceReader::WaitForData(MediaData::Type aType)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsRefPtr<WaitForDataPromise> p = WaitPromise(aType).Ensure(__func__);
  MaybeNotifyHaveData();
  return p;
}

void
MediaSourceReader::MaybeNotifyHaveData()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  bool haveAudio = false, haveVideo = false;
  if (!mAudioIsSeeking && mAudioTrack && HaveData(mLastAudioTime, MediaData::AUDIO_DATA)) {
    haveAudio = true;
    WaitPromise(MediaData::AUDIO_DATA).ResolveIfExists(MediaData::AUDIO_DATA, __func__);
  }
  if (!mVideoIsSeeking && mVideoTrack && HaveData(mLastVideoTime, MediaData::VIDEO_DATA)) {
    haveVideo = true;
    WaitPromise(MediaData::VIDEO_DATA).ResolveIfExists(MediaData::VIDEO_DATA, __func__);
  }
  MSE_DEBUG("MediaSourceReader(%p)::MaybeNotifyHaveData haveAudio=%d, haveVideo=%d", this,
            haveAudio, haveVideo);
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  MSE_DEBUG("MediaSourceReader(%p)::ReadMetadata tracks=%u/%u audio=%p video=%p",
            this, mEssentialTrackBuffers.Length(), mTrackBuffers.Length(),
            mAudioTrack.get(), mVideoTrack.get());

  mEssentialTrackBuffers.Clear();
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

  if (!maxDuration) {
    // Treat a duration of 0 as infinity
    maxDuration = -1;
  }
  static_cast<MediaSourceDecoder*>(mDecoder)->SetDecodedDuration(maxDuration);

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

bool
MediaSourceReader::IsActiveReader(MediaDecoderReader* aReader)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  return aReader == mVideoReader.get() || aReader == mAudioReader.get();
}

} // namespace mozilla
