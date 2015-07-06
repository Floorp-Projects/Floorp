/* -*- mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "MediaSourceReader.h"

#include <cmath>
#include "mozilla/Logging.h"
#include "DecoderTraits.h"
#include "MediaDecoderOwner.h"
#include "MediaFormatReader.h"
#include "MediaSourceDecoder.h"
#include "MediaSourceUtils.h"
#include "SourceBufferDecoder.h"
#include "TrackBuffer.h"
#include "nsPrintfCString.h"

#ifdef MOZ_FMP4
#include "SharedDecoderManager.h"
#include "MP4Decoder.h"
#include "MP4Demuxer.h"
#include "MP4Reader.h"
#endif

#ifdef MOZ_WEBM
#include "WebMReader.h"
#endif

extern PRLogModuleInfo* GetMediaSourceLog();

#define MSE_DEBUG(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Debug, ("MediaSourceReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))
#define MSE_DEBUGV(arg, ...) MOZ_LOG(GetMediaSourceLog(), mozilla::LogLevel::Verbose, ("MediaSourceReader(%p)::%s: " arg, this, __func__, ##__VA_ARGS__))

// When a stream hits EOS it needs to decide what other stream to switch to. Due
// to inaccuracies is determining buffer end frames (Bug 1065207) and rounding
// issues we use a fuzz factor to determine the end time of this stream for
// switching to the new stream. This value is based on the end of frame
// default value used in Blink, kDefaultBufferDurationInMs.
#define EOS_FUZZ_US 125000

namespace mozilla {

MediaSourceReader::MediaSourceReader(MediaSourceDecoder* aDecoder)
  : MediaDecoderReader(aDecoder)
  , mLastAudioTime(0)
  , mLastVideoTime(0)
  , mForceVideoDecodeAhead(false)
  , mOriginalSeekTime(-1)
  , mPendingSeekTime(-1)
  , mWaitingForSeekData(false)
  , mSeekToEnd(false)
  , mTimeThreshold(0)
  , mDropAudioBeforeThreshold(false)
  , mDropVideoBeforeThreshold(false)
  , mAudioDiscontinuity(false)
  , mVideoDiscontinuity(false)
  , mEnded(false)
  , mMediaSourceDuration(0)
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
  MSE_DEBUG("trackBuffers=%u", mTrackBuffers.Length());
  mEssentialTrackBuffers.AppendElements(mTrackBuffers);
  mHasEssentialTrackBuffers = true;
  if (!IsWaitingMediaResources()) {
    mDecoder->NotifyWaitingForResourcesStatusChanged();
  }
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

bool
MediaSourceReader::IsWaitingOnCDMResource()
{
#ifdef MOZ_EME
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MOZ_ASSERT(!IsWaitingMediaResources());

  if (!mInfo.IsEncrypted()) {
    return false;
  }

  // We'll need to wait on the CDMProxy being added, and it having received
  // notification from the child GMP of its capabilities; whether it can
  // decode, or whether we need to decode on our side.
  if (!mCDMProxy) {
    return true;
  }

  {
    CDMCaps::AutoLock caps(mCDMProxy->Capabilites());
    return !caps.AreCapsKnown();
  }

#else
  return false;
#endif
}

size_t
MediaSourceReader::SizeOfVideoQueueInFrames()
{
  if (!GetVideoReader()) {
    MSE_DEBUG("called with no video reader");
    return 0;
  }
  return GetVideoReader()->SizeOfVideoQueueInFrames();
}

size_t
MediaSourceReader::SizeOfAudioQueueInFrames()
{
  if (!GetAudioReader()) {
    MSE_DEBUG("called with no audio reader");
    return 0;
  }
  return GetAudioReader()->SizeOfAudioQueueInFrames();
}

nsRefPtr<MediaDecoderReader::AudioDataPromise>
MediaSourceReader::RequestAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty(), "No sample requests allowed while seeking");
  MOZ_DIAGNOSTIC_ASSERT(mAudioPromise.IsEmpty(), "No duplicate sample requests");
  nsRefPtr<AudioDataPromise> p = mAudioPromise.Ensure(__func__);
  MSE_DEBUGV("mLastAudioTime=%lld", mLastAudioTime);
  if (!mAudioTrack) {
    MSE_DEBUG("called with no audio track");
    mAudioPromise.Reject(DECODE_ERROR, __func__);
    return p;
  }
  if (IsSeeking()) {
    MSE_DEBUG("called mid-seek. Rejecting.");
    mAudioPromise.Reject(CANCELED, __func__);
    return p;
  }
  MOZ_DIAGNOSTIC_ASSERT(!mAudioSeekRequest.Exists());

  SwitchSourceResult ret = SwitchAudioSource(&mLastAudioTime);
  switch (ret) {
    case SOURCE_NEW:
      GetAudioReader()->ResetDecode();
      mAudioSeekRequest.Begin(GetAudioReader()->Seek(GetReaderAudioTime(mLastAudioTime), 0)
                              ->Then(TaskQueue(), __func__, this,
                                     &MediaSourceReader::CompleteAudioSeekAndDoRequest,
                                     &MediaSourceReader::CompleteAudioSeekAndRejectPromise));
      break;
    case SOURCE_NONE:
      if (!mLastAudioTime) {
        // This is the first call to RequestAudioData.
        // Fallback to using decoder with earliest data.
        mAudioSourceDecoder = FirstDecoder(MediaData::AUDIO_DATA);
      }
      if (mLastAudioTime || !mAudioSourceDecoder) {
        CheckForWaitOrEndOfStream(MediaData::AUDIO_DATA, mLastAudioTime);
        break;
      }
      // Fallback to getting first frame from first decoder.
    default:
      DoAudioRequest();
      break;
  }
  return p;
}

void MediaSourceReader::DoAudioRequest()
{
  mAudioRequest.Begin(GetAudioReader()->RequestAudioData()
                      ->Then(TaskQueue(), __func__, this,
                             &MediaSourceReader::OnAudioDecoded,
                             &MediaSourceReader::OnAudioNotDecoded));
}

void
MediaSourceReader::OnAudioDecoded(AudioData* aSample)
{
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking());
  mAudioRequest.Complete();

  int64_t ourTime = aSample->mTime + mAudioSourceDecoder->GetTimestampOffset();
  if (aSample->mDiscontinuity) {
    mAudioDiscontinuity = true;
  }

  MSE_DEBUGV("[mTime=%lld mDuration=%lld mDiscontinuity=%d]",
             ourTime, aSample->mDuration, aSample->mDiscontinuity);
  if (mDropAudioBeforeThreshold) {
    if (ourTime < mTimeThreshold) {
      MSE_DEBUG("mTime=%lld < mTimeThreshold=%lld",
                ourTime, mTimeThreshold);
      mAudioRequest.Begin(GetAudioReader()->RequestAudioData()
                          ->Then(TaskQueue(), __func__, this,
                                 &MediaSourceReader::OnAudioDecoded,
                                 &MediaSourceReader::OnAudioNotDecoded));
      return;
    }
    mDropAudioBeforeThreshold = false;
  }

  // Adjust the sample time into our reference.
  nsRefPtr<AudioData> newSample =
    AudioData::TransferAndUpdateTimestampAndDuration(aSample,
                                                     ourTime,
                                                     aSample->mDuration);
  mLastAudioTime = newSample->GetEndTime();
  if (mAudioDiscontinuity) {
    newSample->mDiscontinuity = true;
    mAudioDiscontinuity = false;
  }

  mAudioPromise.Resolve(newSample, __func__);
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
AdjustEndTime(int64_t* aEndTime, SourceBufferDecoder* aDecoder)
{
  if (aDecoder) {
    media::TimeIntervals ranges = aDecoder->GetBuffered();
    if (ranges.Length()) {
      int64_t end = ranges.GetEnd().ToMicroseconds();
      *aEndTime = std::max(*aEndTime, end);
    }
  }
}

void
MediaSourceReader::OnAudioNotDecoded(NotDecodedReason aReason)
{
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking());
  mAudioRequest.Complete();

  MSE_DEBUG("aReason=%u IsEnded: %d", aReason, IsEnded());
  if (aReason == CANCELED) {
    mAudioPromise.Reject(CANCELED, __func__);
    return;
  }

  // If End of stream. Force switching past this stream to another reader by
  // switching to the end of the buffered range.
  int64_t lastAudioTime = mLastAudioTime;
  if (aReason == END_OF_STREAM && mAudioSourceDecoder) {
    AdjustEndTime(&mLastAudioTime, mAudioSourceDecoder);
  }

  SwitchSourceResult result = SwitchAudioSource(&mLastAudioTime);
  // See if we can find a different source that can pick up where we left off.
  if (result == SOURCE_NEW) {
    GetAudioReader()->ResetDecode();
    mAudioSeekRequest.Begin(GetAudioReader()->Seek(GetReaderAudioTime(mLastAudioTime), 0)
                            ->Then(TaskQueue(), __func__, this,
                                   &MediaSourceReader::CompleteAudioSeekAndDoRequest,
                                   &MediaSourceReader::CompleteAudioSeekAndRejectPromise));
    return;
  }

  // If we got a DECODE_ERROR and we have buffered data in the requested range
  // then it must be a genuine decoding error.
  // Otherwise we can assume that the data was either evicted or explicitely
  // removed from the source buffer and we should wait for new data.
  if (aReason == DECODE_ERROR && result != SOURCE_NONE) {
    mAudioPromise.Reject(DECODE_ERROR, __func__);
    return;
  }

  CheckForWaitOrEndOfStream(MediaData::AUDIO_DATA, mLastAudioTime);

  if (mLastAudioTime - lastAudioTime >= EOS_FUZZ_US) {
    // No decoders are available to switch to. We will re-attempt from the last
    // failing position.
    mLastAudioTime = lastAudioTime;
  }
}

nsRefPtr<MediaDecoderReader::VideoDataPromise>
MediaSourceReader::RequestVideoData(bool aSkipToNextKeyframe,
                                    int64_t aTimeThreshold,
                                    bool aForceDecodeAhead)
{
  MOZ_ASSERT(OnTaskQueue());
  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty(), "No sample requests allowed while seeking");
  MOZ_DIAGNOSTIC_ASSERT(mVideoPromise.IsEmpty(), "No duplicate sample requests");
  nsRefPtr<VideoDataPromise> p = mVideoPromise.Ensure(__func__);
  MSE_DEBUGV("RequestVideoData(%d, %lld), mLastVideoTime=%lld",
             aSkipToNextKeyframe, aTimeThreshold, mLastVideoTime);
  if (!mVideoTrack) {
    MSE_DEBUG("called with no video track");
    mVideoPromise.Reject(DECODE_ERROR, __func__);
    return p;
  }
  if (aSkipToNextKeyframe) {
    mTimeThreshold = aTimeThreshold;
    mDropAudioBeforeThreshold = true;
    mDropVideoBeforeThreshold = true;
  }
  if (IsSeeking()) {
    MSE_DEBUG("called mid-seek. Rejecting.");
    mVideoPromise.Reject(CANCELED, __func__);
    return p;
  }
  MOZ_DIAGNOSTIC_ASSERT(!mVideoSeekRequest.Exists());
  mForceVideoDecodeAhead = aForceDecodeAhead;

  SwitchSourceResult ret = SwitchVideoSource(&mLastVideoTime);
  switch (ret) {
    case SOURCE_NEW:
      GetVideoReader()->ResetDecode();
      mVideoSeekRequest.Begin(GetVideoReader()->Seek(GetReaderVideoTime(mLastVideoTime), 0)
                             ->Then(TaskQueue(), __func__, this,
                                    &MediaSourceReader::CompleteVideoSeekAndDoRequest,
                                    &MediaSourceReader::CompleteVideoSeekAndRejectPromise));
      break;
    case SOURCE_NONE:
      if (!mLastVideoTime) {
        // This is the first call to RequestVideoData.
        // Fallback to using decoder with earliest data.
        mVideoSourceDecoder = FirstDecoder(MediaData::VIDEO_DATA);
      }
      if (mLastVideoTime || !mVideoSourceDecoder) {
        CheckForWaitOrEndOfStream(MediaData::VIDEO_DATA, mLastVideoTime);
        break;
      }
      // Fallback to getting first frame from first decoder.
    default:
      DoVideoRequest();
      break;
  }

  return p;
}

void
MediaSourceReader::DoVideoRequest()
{
  mVideoRequest.Begin(GetVideoReader()->RequestVideoData(mDropVideoBeforeThreshold,
                                                         GetReaderVideoTime(mTimeThreshold),
                                                         mForceVideoDecodeAhead)
                      ->Then(TaskQueue(), __func__, this,
                             &MediaSourceReader::OnVideoDecoded,
                             &MediaSourceReader::OnVideoNotDecoded));
}

void
MediaSourceReader::OnVideoDecoded(VideoData* aSample)
{
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking());
  mVideoRequest.Complete();

  // Adjust the sample time into our reference.
  int64_t ourTime = aSample->mTime + mVideoSourceDecoder->GetTimestampOffset();
  if (aSample->mDiscontinuity) {
    mVideoDiscontinuity = true;
  }

  MSE_DEBUGV("[mTime=%lld mDuration=%lld mDiscontinuity=%d]",
             ourTime, aSample->mDuration, aSample->mDiscontinuity);
  if (mDropVideoBeforeThreshold) {
    if (ourTime < mTimeThreshold) {
      MSE_DEBUG("mTime=%lld < mTimeThreshold=%lld",
                ourTime, mTimeThreshold);
      DoVideoRequest();
      return;
    }
    mDropVideoBeforeThreshold = false;
    mTimeThreshold = 0;
  }

  // Adjust the sample time into our reference.
  nsRefPtr<VideoData> newSample =
    VideoData::ShallowCopyUpdateTimestampAndDuration(aSample,
                                                     ourTime,
                                                     aSample->mDuration);

  mLastVideoTime = newSample->GetEndTime();
  if (mVideoDiscontinuity) {
    newSample->mDiscontinuity = true;
    mVideoDiscontinuity = false;
  }

  mVideoPromise.Resolve(newSample, __func__);
}

void
MediaSourceReader::OnVideoNotDecoded(NotDecodedReason aReason)
{
  MOZ_DIAGNOSTIC_ASSERT(!IsSeeking());
  mVideoRequest.Complete();

  MSE_DEBUG("aReason=%u IsEnded: %d", aReason, IsEnded());

  if (aReason == CANCELED) {
    mVideoPromise.Reject(CANCELED, __func__);
    return;
  }

  // if End of stream. Force switching past this stream to another reader by
  // switching to the end of the buffered range.
  int64_t lastVideoTime = mLastVideoTime;
  if (aReason == END_OF_STREAM && mVideoSourceDecoder) {
    AdjustEndTime(&mLastVideoTime, mVideoSourceDecoder);
  }

  // See if we can find a different reader that can pick up where we left off.
  SwitchSourceResult result = SwitchVideoSource(&mLastVideoTime);
  if (result == SOURCE_NEW) {
    GetVideoReader()->ResetDecode();
    mVideoSeekRequest.Begin(GetVideoReader()->Seek(GetReaderVideoTime(mLastVideoTime), 0)
                           ->Then(TaskQueue(), __func__, this,
                                  &MediaSourceReader::CompleteVideoSeekAndDoRequest,
                                  &MediaSourceReader::CompleteVideoSeekAndRejectPromise));
    return;
  }

  // If we got a DECODE_ERROR and we have buffered data in the requested range
  // then it must be a genuine decoding error.
  // Otherwise we can assume that the data was either evicted or explicitely
  // removed from the source buffer and we should wait for new data.
  if (aReason == DECODE_ERROR && result != SOURCE_NONE) {
    mVideoPromise.Reject(DECODE_ERROR, __func__);
    return;
  }

  CheckForWaitOrEndOfStream(MediaData::VIDEO_DATA, mLastVideoTime);

  if (mLastVideoTime - lastVideoTime >= EOS_FUZZ_US) {
    // No decoders are available to switch to. We will re-attempt from the last
    // failing position.
    mLastVideoTime = lastVideoTime;
  }
}

void
MediaSourceReader::CheckForWaitOrEndOfStream(MediaData::Type aType, int64_t aTime)
{
  // If the entire MediaSource is done, generate an EndOfStream.
  if (IsNearEnd(aType, aTime)) {
    if (aType == MediaData::AUDIO_DATA) {
      mAudioPromise.Reject(END_OF_STREAM, __func__);
    } else {
      mVideoPromise.Reject(END_OF_STREAM, __func__);
    }
    return;
  }

  if (aType == MediaData::AUDIO_DATA) {
    // We don't have the data the caller wants. Tell that we're waiting for JS to
    // give us more data.
    mAudioPromise.Reject(WAITING_FOR_DATA, __func__);
  } else {
    mVideoPromise.Reject(WAITING_FOR_DATA, __func__);
  }
}

nsRefPtr<ShutdownPromise>
MediaSourceReader::Shutdown()
{
  mSeekPromise.RejectIfExists(NS_ERROR_FAILURE, __func__);
  // Any previous requests we've been waiting on are now unwanted.
  mAudioRequest.DisconnectIfExists();
  mVideoRequest.DisconnectIfExists();

  // Additionally, reject any outstanding promises _we_ made that we might have
  // been waiting on the above to fulfill.
  mAudioPromise.RejectIfExists(CANCELED, __func__);
  mVideoPromise.RejectIfExists(CANCELED, __func__);

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
    mTrackBuffers[0]->Shutdown()->Then(TaskQueue(), __func__, this,
                                       &MediaSourceReader::ContinueShutdown,
                                       &MediaSourceReader::ContinueShutdown);
    mShutdownTrackBuffers.AppendElement(mTrackBuffers[0]);
    mTrackBuffers.RemoveElementAt(0);
    return;
  }

  mAudioTrack = nullptr;
  mAudioSourceDecoder = nullptr;
  mVideoTrack = nullptr;
  mVideoSourceDecoder = nullptr;

#ifdef MOZ_FMP4
  if (mSharedDecoderManager) {
    mSharedDecoderManager->Shutdown();
    mSharedDecoderManager = nullptr;
  }
#endif

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
  MOZ_ASSERT(!mAudioSourceDecoder);
  MOZ_ASSERT(!mVideoTrack);
  MOZ_ASSERT(!mVideoSourceDecoder);
  MOZ_ASSERT(!mTrackBuffers.Length());

  for (uint32_t i = 0; i < mShutdownTrackBuffers.Length(); ++i) {
    mShutdownTrackBuffers[i]->BreakCycles();
  }
  mShutdownTrackBuffers.Clear();
}

already_AddRefed<SourceBufferDecoder>
MediaSourceReader::SelectDecoder(int64_t aTarget,
                                 int64_t aTolerance,
                                 TrackBuffer* aTrackBuffer)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  media::TimeUnit target{media::TimeUnit::FromMicroseconds(aTarget)};
  media::TimeUnit tolerance{media::TimeUnit::FromMicroseconds(aTolerance + aTarget)};

  const nsTArray<nsRefPtr<SourceBufferDecoder>>& decoders{aTrackBuffer->Decoders()};

  // aTolerance gives a slight bias toward the start of a range only.
  // Consider decoders in order of newest to oldest, as a newer decoder
  // providing a given buffered range is expected to replace an older one.
  for (int32_t i = decoders.Length() - 1; i >= 0; --i) {
    nsRefPtr<SourceBufferDecoder> newDecoder = decoders[i];
    media::TimeIntervals ranges = aTrackBuffer->GetBuffered(newDecoder);
    for (uint32_t j = 0; j < ranges.Length(); j++) {
      if (target < ranges.End(j) && tolerance >= ranges.Start(j)) {
        return newDecoder.forget();
      }
    }
    MSE_DEBUGV("SelectDecoder(%lld fuzz:%lld) newDecoder=%p (%d/%d) target not in ranges=%s",
               aTarget, aTolerance, newDecoder.get(), i+1,
               decoders.Length(), DumpTimeRanges(ranges).get());
  }

  return nullptr;
}

bool
MediaSourceReader::HaveData(int64_t aTarget, MediaData::Type aType)
{
  TrackBuffer* trackBuffer = aType == MediaData::AUDIO_DATA ? mAudioTrack : mVideoTrack;
  MOZ_ASSERT(trackBuffer);
  nsRefPtr<SourceBufferDecoder> decoder = SelectDecoder(aTarget, EOS_FUZZ_US, trackBuffer);
  return !!decoder;
}

MediaSourceReader::SwitchSourceResult
MediaSourceReader::SwitchAudioSource(int64_t* aTarget)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding an audio track after ReadMetadata.
  if (!mAudioTrack) {
    return SOURCE_NONE;
  }

  // We first search without the tolerance and then search with it, so that, in
  // the case of perfectly-aligned data, we don't prematurely jump to a new
  // reader and skip the last few samples of the current one.
  bool usedFuzz = false;
  nsRefPtr<SourceBufferDecoder> newDecoder =
    SelectDecoder(*aTarget, /* aTolerance = */ 0, mAudioTrack);
  if (!newDecoder) {
    newDecoder = SelectDecoder(*aTarget, EOS_FUZZ_US, mAudioTrack);
    usedFuzz = true;
  }
  if (GetAudioReader() && mAudioSourceDecoder != newDecoder) {
    GetAudioReader()->SetIdle();
  }
  if (!newDecoder) {
    mAudioSourceDecoder = nullptr;
    return SOURCE_NONE;
  }
  if (newDecoder == mAudioSourceDecoder) {
    return SOURCE_EXISTING;
  }
  mAudioSourceDecoder = newDecoder;
  if (usedFuzz) {
    // A decoder buffered range is continuous. We would have failed the exact
    // search but succeeded the fuzzy one if our target was shortly before
    // start time.
    media::TimeIntervals ranges = newDecoder->GetBuffered();
    int64_t startTime = ranges.GetStart().ToMicroseconds();
    if (*aTarget < startTime) {
      *aTarget = startTime;
    }
  }
  MSE_DEBUGV("switched decoder to %p (fuzz:%d)",
             mAudioSourceDecoder.get(), usedFuzz);
  return SOURCE_NEW;
}

MediaSourceReader::SwitchSourceResult
MediaSourceReader::SwitchVideoSource(int64_t* aTarget)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  // XXX: Can't handle adding a video track after ReadMetadata.
  if (!mVideoTrack) {
    return SOURCE_NONE;
  }

  // We first search without the tolerance and then search with it, so that, in
  // the case of perfectly-aligned data, we don't prematurely jump to a new
  // reader and skip the last few samples of the current one.
  bool usedFuzz = false;
  nsRefPtr<SourceBufferDecoder> newDecoder =
    SelectDecoder(*aTarget, /* aTolerance = */ 0, mVideoTrack);
  if (!newDecoder) {
    newDecoder = SelectDecoder(*aTarget, EOS_FUZZ_US, mVideoTrack);
    usedFuzz = true;
  }
  if (GetVideoReader() && mVideoSourceDecoder != newDecoder) {
    GetVideoReader()->SetIdle();
  }
  if (!newDecoder) {
    mVideoSourceDecoder = nullptr;
    return SOURCE_NONE;
  }
  if (newDecoder == mVideoSourceDecoder) {
    return SOURCE_EXISTING;
  }
  mVideoSourceDecoder = newDecoder;
  if (usedFuzz) {
    // A decoder buffered range is continuous. We would have failed the exact
    // search but succeeded the fuzzy one if our target was shortly before
    // start time.
    media::TimeIntervals ranges = newDecoder->GetBuffered();
    int64_t startTime = ranges.GetStart().ToMicroseconds();
    if (*aTarget < startTime) {
      *aTarget = startTime;
    }
  }
  MSE_DEBUGV("switched decoder to %p (fuzz:%d)",
             mVideoSourceDecoder.get(), usedFuzz);
  return SOURCE_NEW;
}

void
MediaSourceReader::ReleaseMediaResources()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (GetVideoReader()) {
    GetVideoReader()->ReleaseMediaResources();
  }
}

MediaDecoderReader*
CreateReaderForType(const nsACString& aType, AbstractMediaDecoder* aDecoder,
                    MediaTaskQueue* aBorrowedTaskQueue)
{
#ifdef MOZ_FMP4
  // The MP4Reader that supports fragmented MP4 and uses
  // PlatformDecoderModules is hidden behind prefs for regular video
  // elements, but we always want to use it for MSE, so instantiate it
  // directly here.
  if ((aType.LowerCaseEqualsLiteral("video/mp4") ||
       aType.LowerCaseEqualsLiteral("audio/mp4")) &&
      MP4Decoder::IsEnabled() && aDecoder) {
    bool useFormatDecoder =
      Preferences::GetBool("media.mediasource.format-reader.mp4", true);
    MediaDecoderReader* reader = useFormatDecoder ?
      static_cast<MediaDecoderReader*>(new MediaFormatReader(aDecoder, new MP4Demuxer(aDecoder->GetResource()), aBorrowedTaskQueue)) :
      static_cast<MediaDecoderReader*>(new MP4Reader(aDecoder, aBorrowedTaskQueue));
    return reader;
  }
#endif

#ifdef MOZ_WEBM
  if (DecoderTraits::IsWebMType(aType)) {
    return new WebMReader(aDecoder, aBorrowedTaskQueue);
  }
#endif

  return nullptr;
}

already_AddRefed<SourceBufferDecoder>
MediaSourceReader::CreateSubDecoder(const nsACString& aType, int64_t aTimestampOffset)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mDecoder->IsShutdown()) {
    return nullptr;
  }

  // The task queue borrowing is icky. It would be nicer to just give each subreader
  // its own task queue. Unfortunately though, Request{Audio,Video}Data implementations
  // currently assert that they're on "the decode thread", and so having
  // separate task queues makes MediaSource stuff unnecessarily cumbersome. We
  // should remove the need for these assertions (which probably involves making
  // all Request*Data implementations fully async), and then get rid of the
  // borrowing.
  nsRefPtr<SourceBufferDecoder> decoder =
    new SourceBufferDecoder(new SourceBufferResource(aType), mDecoder, aTimestampOffset);
  nsRefPtr<MediaDecoderReader> reader(CreateReaderForType(aType, decoder, TaskQueue()));
  if (!reader) {
    return nullptr;
  }

  // MSE uses a start time of 0 everywhere. Set that immediately on the
  // subreader to make sure that it's always in a state where we can invoke
  // GetBuffered on it.
  reader->DispatchSetStartTime(0);

#ifdef MOZ_FMP4
  reader->SetSharedDecoderManager(mSharedDecoderManager);
#endif
  reader->Init(nullptr);

  MSE_DEBUG("subdecoder %p subreader %p",
            decoder.get(), reader.get());
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
  MSE_DEBUG("AddTrackBuffer(%p)", aTrackBuffer);
  mTrackBuffers.AppendElement(aTrackBuffer);
}

void
MediaSourceReader::RemoveTrackBuffer(TrackBuffer* aTrackBuffer)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("RemoveTrackBuffer(%p)", aTrackBuffer);
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
    MSE_DEBUG("%p audio", aTrackBuffer);
    mAudioTrack = aTrackBuffer;
  }
  if (aInfo.HasVideo() && !mVideoTrack) {
    MSE_DEBUG("%p video", aTrackBuffer);
    mVideoTrack = aTrackBuffer;
  }

  if (!IsWaitingMediaResources()) {
    mDecoder->NotifyWaitingForResourcesStatusChanged();
  }
}

bool
MediaSourceReader::TrackBuffersContainTime(int64_t aTime)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mAudioTrack && !mAudioTrack->ContainsTime(aTime, EOS_FUZZ_US)) {
    return false;
  }
  if (mVideoTrack && !mVideoTrack->ContainsTime(aTime, EOS_FUZZ_US)) {
    return false;
  }
  return true;
}

void
MediaSourceReader::NotifyTimeRangesChanged()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (mWaitingForSeekData) {
    //post a task to the decode queue to try to complete the pending seek.
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(
        this, &MediaSourceReader::AttemptSeek));
    TaskQueue()->Dispatch(task.forget());
  } else {
    MaybeNotifyHaveData();
  }
}

nsRefPtr<MediaDecoderReader::SeekPromise>
MediaSourceReader::Seek(int64_t aTime, int64_t aIgnored /* Used only for ogg which is non-MSE */)
{
  MSE_DEBUG("Seek(aTime=%lld, aEnd=%lld, aCurrent=%lld)",
            aTime);

  MOZ_DIAGNOSTIC_ASSERT(mSeekPromise.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(mAudioPromise.IsEmpty());
  MOZ_DIAGNOSTIC_ASSERT(mVideoPromise.IsEmpty());
  MOZ_ASSERT(!mShutdown);
  nsRefPtr<SeekPromise> p = mSeekPromise.Ensure(__func__);

  // Store pending seek target in case the track buffers don't contain
  // the desired time and we delay doing the seek.
  mOriginalSeekTime = aTime;
  mPendingSeekTime = aTime;

  {
    ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
    mWaitingForSeekData = true;
    mDropAudioBeforeThreshold = false;
    mDropVideoBeforeThreshold = false;
    mTimeThreshold = 0;
  }

  AttemptSeek();
  return p;
}

nsresult
MediaSourceReader::ResetDecode()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  MSE_DEBUG("");

  // Any previous requests we've been waiting on are now unwanted.
  mAudioRequest.DisconnectIfExists();
  mVideoRequest.DisconnectIfExists();
  mAudioSeekRequest.DisconnectIfExists();
  mVideoSeekRequest.DisconnectIfExists();

  // Additionally, reject any outstanding promises _we_ made that we might have
  // been waiting on the above to fulfill.
  mAudioPromise.RejectIfExists(CANCELED, __func__);
  mVideoPromise.RejectIfExists(CANCELED, __func__);
  mSeekPromise.RejectIfExists(NS_OK, __func__);

  // Do the same for any data wait promises.
  mAudioWaitPromise.RejectIfExists(WaitForDataRejectValue(MediaData::AUDIO_DATA, WaitForDataRejectValue::CANCELED), __func__);
  mVideoWaitPromise.RejectIfExists(WaitForDataRejectValue(MediaData::VIDEO_DATA, WaitForDataRejectValue::CANCELED), __func__);

  // Reset miscellaneous seeking state.
  mWaitingForSeekData = false;
  mPendingSeekTime = -1;

  // Reset force video decode ahead.
  mForceVideoDecodeAhead = false;

  // Reset all the readers.
  if (GetAudioReader()) {
    GetAudioReader()->ResetDecode();
  }
  if (GetVideoReader()) {
    GetVideoReader()->ResetDecode();
  }

  return MediaDecoderReader::ResetDecode();
}

void
MediaSourceReader::OnVideoSeekCompleted(int64_t aTime)
{
  mVideoSeekRequest.Complete();

  // The aTime we receive is in the sub-reader's reference.
  int64_t ourTime = aTime + mVideoSourceDecoder->GetTimestampOffset();

  if (mAudioTrack) {
    mPendingSeekTime = ourTime;
    DoAudioSeek();
  } else {
    mPendingSeekTime = -1;
    mSeekPromise.Resolve(ourTime, __func__);
  }
}

void
MediaSourceReader::OnVideoSeekFailed(nsresult aResult)
{
  mVideoSeekRequest.Complete();
  mPendingSeekTime = -1;
  mSeekPromise.Reject(aResult, __func__);
}

void
MediaSourceReader::DoAudioSeek()
{
  int64_t seekTime = mPendingSeekTime;
  if (mSeekToEnd) {
    seekTime = LastSampleTime(MediaData::AUDIO_DATA);
  }
  if (SwitchAudioSource(&seekTime) == SOURCE_NONE &&
      SwitchAudioSource(&mOriginalSeekTime) == SOURCE_NONE) {
    // Data we need got evicted since the last time we checked for data
    // availability. Abort current seek attempt.
    mWaitingForSeekData = true;
    return;
  }
  GetAudioReader()->ResetDecode();
  mAudioSeekRequest.Begin(GetAudioReader()->Seek(GetReaderAudioTime(seekTime), 0)
                         ->Then(TaskQueue(), __func__, this,
                                &MediaSourceReader::OnAudioSeekCompleted,
                                &MediaSourceReader::OnAudioSeekFailed));
  MSE_DEBUG("reader=%p", GetAudioReader());
}

void
MediaSourceReader::OnAudioSeekCompleted(int64_t aTime)
{
  mAudioSeekRequest.Complete();
  mPendingSeekTime = -1;
  // The aTime we receive is in the sub-reader's reference.
  mSeekPromise.Resolve(aTime + mAudioSourceDecoder->GetTimestampOffset(),
                       __func__);
}

void
MediaSourceReader::OnAudioSeekFailed(nsresult aResult)
{
  mAudioSeekRequest.Complete();
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
    if (!mWaitingForSeekData) {
      return;
    }

    mSeekToEnd = IsEnded() && mPendingSeekTime >= mMediaSourceDuration * USECS_PER_S;
    if (!mSeekToEnd && !TrackBuffersContainTime(mPendingSeekTime)) {
      mVideoSourceDecoder = nullptr;
      mAudioSourceDecoder = nullptr;
      return;
    }
    mWaitingForSeekData = false;
  }

  // Decoding discontinuity upon seek, reset last times to seek target.
  mLastAudioTime = mPendingSeekTime;
  mLastVideoTime = mPendingSeekTime;

  if (mVideoTrack) {
    DoVideoSeek();
  } else if (mAudioTrack) {
    DoAudioSeek();
  } else {
    MOZ_CRASH();
  }
}

void
MediaSourceReader::DoVideoSeek()
{
  int64_t seekTime = mPendingSeekTime;
  if (mSeekToEnd) {
    seekTime = LastSampleTime(MediaData::VIDEO_DATA);
  }
  if (SwitchVideoSource(&seekTime) == SOURCE_NONE) {
    // Data we need got evicted since the last time we checked for data
    // availability. Abort current seek attempt.
    mWaitingForSeekData = true;
    return;
  }
  GetVideoReader()->ResetDecode();
  mVideoSeekRequest.Begin(GetVideoReader()->Seek(GetReaderVideoTime(seekTime), 0)
                          ->Then(TaskQueue(), __func__, this,
                                 &MediaSourceReader::OnVideoSeekCompleted,
                                 &MediaSourceReader::OnVideoSeekFailed));
  MSE_DEBUG("reader=%p", GetVideoReader());
}

media::TimeIntervals
MediaSourceReader::GetBuffered()
{
  MOZ_ASSERT(OnTaskQueue());
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  media::TimeIntervals buffered;

  media::TimeUnit highestEndTime;
  nsTArray<media::TimeIntervals> activeRanges;
  // Must set the capacity of the nsTArray first: bug #1164444
  activeRanges.SetCapacity(mTrackBuffers.Length());

  for (const auto& trackBuffer : mTrackBuffers) {
    activeRanges.AppendElement(trackBuffer->Buffered());
    highestEndTime = std::max(highestEndTime, activeRanges.LastElement().GetEnd());
  }

  buffered +=
    media::TimeInterval(media::TimeUnit::FromMicroseconds(0), highestEndTime);

  for (auto& range : activeRanges) {
    if (IsEnded() && range.Length()) {
      // Set the end time on the last range to highestEndTime by adding a
      // new range spanning the current end time to highestEndTime, which
      // Normalize() will then merge with the old last range.
      range +=
        media::TimeInterval(range.GetEnd(), highestEndTime);
    }
    buffered.Intersection(range);
  }

  MSE_DEBUG("ranges=%s", DumpTimeRanges(buffered).get());
  return buffered;
}

already_AddRefed<SourceBufferDecoder>
MediaSourceReader::FirstDecoder(MediaData::Type aType)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  TrackBuffer* trackBuffer =
    aType == MediaData::AUDIO_DATA ? mAudioTrack : mVideoTrack;
  MOZ_ASSERT(trackBuffer);
  const nsTArray<nsRefPtr<SourceBufferDecoder>>& decoders = trackBuffer->Decoders();
  if (decoders.IsEmpty()) {
    return nullptr;
  }

  nsRefPtr<SourceBufferDecoder> firstDecoder;
  media::TimeUnit lowestStartTime{media::TimeUnit::FromInfinity()};


  for (uint32_t i = 0; i < decoders.Length(); ++i) {
    media::TimeIntervals r = decoders[i]->GetBuffered();
    if (!r.Length()) {
      continue;
    }
    media::TimeUnit start = r.GetStart();
    if (start < lowestStartTime) {
      firstDecoder = decoders[i];
      lowestStartTime = start;
    }
  }
  return firstDecoder.forget();
}

nsRefPtr<MediaDecoderReader::WaitForDataPromise>
MediaSourceReader::WaitForData(MediaData::Type aType)
{
  MOZ_ASSERT(OnTaskQueue());
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
  bool ended = IsEnded();
  // If we are in ended mode, we will resolve any pending wait promises.
  // The next Request*Data will handle END_OF_STREAM or going back into waiting
  // mode.
  if (!IsSeeking() && mAudioTrack) {
    if (!mLastAudioTime) {
      nsRefPtr<SourceBufferDecoder> d = FirstDecoder(MediaData::AUDIO_DATA);
      haveAudio = !!d;
    } else {
      haveAudio = HaveData(mLastAudioTime, MediaData::AUDIO_DATA);
    }
    if (ended || haveAudio) {
      WaitPromise(MediaData::AUDIO_DATA).ResolveIfExists(MediaData::AUDIO_DATA, __func__);
    }
  }
  if (!IsSeeking() && mVideoTrack) {
    if (!mLastVideoTime) {
      nsRefPtr<SourceBufferDecoder> d = FirstDecoder(MediaData::VIDEO_DATA);
      haveVideo = !!d;
    } else {
      haveVideo = HaveData(mLastVideoTime, MediaData::VIDEO_DATA);
    }
    if (ended || haveVideo) {
      WaitPromise(MediaData::VIDEO_DATA).ResolveIfExists(MediaData::VIDEO_DATA, __func__);
    }
  }
  MSE_DEBUG("isSeeking=%d haveAudio=%d, haveVideo=%d ended=%d",
            IsSeeking(), haveAudio, haveVideo, ended);
}

nsresult
MediaSourceReader::ReadMetadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  MSE_DEBUG("tracks=%u/%u audio=%p video=%p",
            mEssentialTrackBuffers.Length(), mTrackBuffers.Length(),
            mAudioTrack.get(), mVideoTrack.get());

  mEssentialTrackBuffers.Clear();
  if (!mAudioTrack && !mVideoTrack) {
    MSE_DEBUG("missing track: mAudioTrack=%p mVideoTrack=%p",
              mAudioTrack.get(), mVideoTrack.get());
    return NS_ERROR_FAILURE;
  }

  if (mAudioTrack == mVideoTrack) {
    NS_WARNING("Combined audio/video sourcebuffer, this is an unsupported "
               "configuration, only using video track");
    mAudioTrack = nullptr;
  }
  if (mAudioTrack) {
    MOZ_ASSERT(mAudioTrack->IsReady());
    mAudioSourceDecoder = mAudioTrack->Decoders()[0];

    const MediaInfo& info = GetAudioReader()->GetMediaInfo();
    MOZ_ASSERT(info.HasAudio());
    mInfo.mAudio = info.mAudio;
    mInfo.mCrypto.AddInitData(info.mCrypto);
    MSE_DEBUG("audio reader=%p duration=%lld",
              mAudioSourceDecoder.get(),
              mInfo.mMetadataDuration.isSome() ? mInfo.mMetadataDuration.ref().ToMicroseconds() : -1);
  }

  if (mVideoTrack) {
    MOZ_ASSERT(mVideoTrack->IsReady());
    mVideoSourceDecoder = mVideoTrack->Decoders()[0];

    const MediaInfo& info = GetVideoReader()->GetMediaInfo();
    MOZ_ASSERT(info.HasVideo());
    mInfo.mVideo = info.mVideo;
    mInfo.mCrypto.AddInitData(info.mCrypto);
    MSE_DEBUG("video reader=%p duration=%lld",
              GetVideoReader(),
              mInfo.mMetadataDuration.isSome() ? mInfo.mMetadataDuration.ref().ToMicroseconds() : -1);
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
    mAudioSourceDecoder = mAudioTrack->Decoders()[0];

    const MediaInfo& info = GetAudioReader()->GetMediaInfo();
    MOZ_ASSERT(info.HasAudio());
    mInfo.mAudio = info.mAudio;
  }

  if (mVideoTrack) {
    MOZ_ASSERT(mVideoTrack->IsReady());
    mVideoSourceDecoder = mVideoTrack->Decoders()[0];

    const MediaInfo& info = GetVideoReader()->GetMediaInfo();
    MOZ_ASSERT(info.HasVideo());
    mInfo.mVideo = info.mVideo;
  }
  *aInfo = mInfo;
}

void
MediaSourceReader::Ended(bool aEnded)
{
  mDecoder->GetReentrantMonitor().AssertCurrentThreadIn();
  mEnded = aEnded;
  if (aEnded) {
    // post a task to the decode queue to try to complete any pending
    // seek or wait
    RefPtr<nsIRunnable> task(NS_NewRunnableMethod(
        this, &MediaSourceReader::NotifyTimeRangesChanged));
    TaskQueue()->Dispatch(task.forget());
  }
}

bool
MediaSourceReader::IsEnded()
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  return mEnded;
}

bool
MediaSourceReader::IsNearEnd(MediaData::Type aType, int64_t aTime)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  if (!mEnded) {
    return false;
  }
  TrackBuffer* trackBuffer =
    aType == MediaData::AUDIO_DATA ? mAudioTrack : mVideoTrack;
  media::TimeIntervals buffered = trackBuffer->Buffered();
  return aTime >= buffered.GetEnd().ToMicroseconds() - EOS_FUZZ_US;
}

int64_t
MediaSourceReader::LastSampleTime(MediaData::Type aType)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());

  TrackBuffer* trackBuffer =
    aType == MediaData::AUDIO_DATA ? mAudioTrack : mVideoTrack;
  media::TimeIntervals buffered = trackBuffer->Buffered();
  return buffered.GetEnd().ToMicroseconds() - 1;
}

void
MediaSourceReader::SetMediaSourceDuration(double aDuration)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  mMediaSourceDuration = aDuration;
}

void
MediaSourceReader::GetMozDebugReaderData(nsAString& aString)
{
  ReentrantMonitorAutoEnter mon(mDecoder->GetReentrantMonitor());
  nsAutoCString result;
  result += nsPrintfCString("Dumping data for reader %p:\n", this);
  if (mAudioTrack) {
    result += nsPrintfCString("\tDumping Audio Track Decoders: - mLastAudioTime: %f\n", double(mLastAudioTime) / USECS_PER_S);
    for (int32_t i = mAudioTrack->Decoders().Length() - 1; i >= 0; --i) {
      const nsRefPtr<SourceBufferDecoder>& newDecoder{mAudioTrack->Decoders()[i]};
      media::TimeIntervals ranges = mAudioTrack->GetBuffered(newDecoder);
      result += nsPrintfCString("\t\tReader %d: %p ranges=%s active=%s size=%lld\n",
                                i,
                                newDecoder->GetReader(),
                                DumpTimeRanges(ranges).get(),
                                newDecoder->GetReader() == GetAudioReader() ? "true" : "false",
                                newDecoder->GetResource()->GetSize());
    }
  }

  if (mVideoTrack) {
    result += nsPrintfCString("\tDumping Video Track Decoders - mLastVideoTime: %f\n", double(mLastVideoTime) / USECS_PER_S);
    for (int32_t i = mVideoTrack->Decoders().Length() - 1; i >= 0; --i) {
      const nsRefPtr<SourceBufferDecoder>& newDecoder{mVideoTrack->Decoders()[i]};
      media::TimeIntervals ranges = mVideoTrack->GetBuffered(newDecoder);
      result += nsPrintfCString("\t\tReader %d: %p ranges=%s active=%s size=%lld\n",
                                i,
                                newDecoder->GetReader(),
                                DumpTimeRanges(ranges).get(),
                                newDecoder->GetReader() == GetVideoReader() ? "true" : "false",
                                mVideoTrack->Decoders()[i]->GetResource()->GetSize());
    }
  }
  aString += NS_ConvertUTF8toUTF16(result);
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
  return aReader && (aReader == GetVideoReader() || aReader == GetAudioReader());
}

MediaDecoderReader*
MediaSourceReader::GetAudioReader() const
{
  return mAudioSourceDecoder ? mAudioSourceDecoder->GetReader() : nullptr;
}

MediaDecoderReader*
MediaSourceReader::GetVideoReader() const
{
  return mVideoSourceDecoder ? mVideoSourceDecoder->GetReader() : nullptr;
}

int64_t
MediaSourceReader::GetReaderAudioTime(int64_t aTime) const
{
  return aTime - mAudioSourceDecoder->GetTimestampOffset();
}

int64_t
MediaSourceReader::GetReaderVideoTime(int64_t aTime) const
{
  return aTime - mVideoSourceDecoder->GetTimestampOffset();
}

#undef MSE_DEBUG
#undef MSE_DEBUGV
} // namespace mozilla
