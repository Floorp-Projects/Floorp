/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AccurateSeekTask.h"
#include "MediaDecoderReaderWrapper.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "nsPrintfCString.h"

namespace mozilla {

extern LazyLogModule gMediaDecoderLog;
extern LazyLogModule gMediaSampleLog;

// avoid redefined macro in unified build
#undef LOG
#undef DECODER_LOG
#undef VERBOSE_LOG

#define LOG(m, l, x, ...) \
  MOZ_LOG(m, l, ("[AccurateSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__))
#define DECODER_LOG(x, ...) \
  LOG(gMediaDecoderLog, LogLevel::Debug, x, ##__VA_ARGS__)
#define VERBOSE_LOG(x, ...) \
  LOG(gMediaDecoderLog, LogLevel::Verbose, x, ##__VA_ARGS__)
#define SAMPLE_LOG(x, ...) \
  LOG(gMediaSampleLog, LogLevel::Debug, x, ##__VA_ARGS__)

// Somehow MSVC doesn't correctly delete the comma before ##__VA_ARGS__
// when __VA_ARGS__ expands to nothing. This is a workaround for it.
#define DECODER_WARN_HELPER(a, b) NS_WARNING b
#define DECODER_WARN(x, ...) \
  DECODER_WARN_HELPER(0, (nsPrintfCString("Decoder=%p " x, mDecoderID, ##__VA_ARGS__).get()))

AccurateSeekTask::AccurateSeekTask(const void* aDecoderID,
                                   AbstractThread* aThread,
                                   MediaDecoderReaderWrapper* aReader,
                                   SeekJob&& aSeekJob,
                                   const MediaInfo& aInfo,
                                   const media::TimeUnit& aDuration,
                                   int64_t aCurrentMediaTime)
  : SeekTask(aDecoderID, aThread, aReader, Move(aSeekJob))
  , mCurrentTimeBeforeSeek(aCurrentMediaTime)
  , mAudioRate(aInfo.mAudio.mRate)
  , mHasAudio(aInfo.HasAudio())
  , mHasVideo(aInfo.HasVideo())
  , mDropAudioUntilNextDiscontinuity(false)
  , mDropVideoUntilNextDiscontinuity(false)
{
  AssertOwnerThread();

  // Bound the seek time to be inside the media range.
  int64_t end = aDuration.ToMicroseconds();
  NS_ASSERTION(end != -1, "Should know end time by now");
  int64_t seekTime = mSeekJob.mTarget.GetTime().ToMicroseconds();
  seekTime = std::min(seekTime, end);
  seekTime = std::max(int64_t(0), seekTime);
  NS_ASSERTION(seekTime >= 0 && seekTime <= end,
               "Can only seek in range [0,duration]");
  mSeekJob.mTarget.SetTime(media::TimeUnit::FromMicroseconds(seekTime));

  mDropAudioUntilNextDiscontinuity = HasAudio();
  mDropVideoUntilNextDiscontinuity = HasVideo();

  // Configure MediaDecoderReaderWrapper.
  SetMediaDecoderReaderWrapperCallback();
}

AccurateSeekTask::~AccurateSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

bool
AccurateSeekTask::HasAudio() const
{
  AssertOwnerThread();
  return mHasAudio;
}

bool
AccurateSeekTask::HasVideo() const
{
  AssertOwnerThread();
  return mHasVideo;
}

void
AccurateSeekTask::Discard()
{
  AssertOwnerThread();

  // Disconnect MediaDecoder.
  mSeekJob.RejectIfExists(__func__);

  // Disconnect MDSM.
  RejectIfExist(__func__);

  // Disconnect MediaDecoderReaderWrapper.
  mSeekRequest.DisconnectIfExists();
  CancelMediaDecoderReaderWrapperCallback();

  mIsDiscarded = true;
}

bool
AccurateSeekTask::NeedToResetMDSM() const
{
  AssertOwnerThread();
  return true;
}

RefPtr<AccurateSeekTask::SeekTaskPromise>
AccurateSeekTask::Seek(const media::TimeUnit& aDuration)
{
  AssertOwnerThread();

  // Do the seek.
  mSeekRequest.Begin(mReader->Seek(mSeekJob.mTarget, aDuration)
    ->Then(OwnerThread(), __func__, this,
           &AccurateSeekTask::OnSeekResolved, &AccurateSeekTask::OnSeekRejected));

  return mSeekTaskPromise.Ensure(__func__);
}

bool
AccurateSeekTask::IsVideoDecoding() const
{
  AssertOwnerThread();
  return HasVideo() && !mIsVideoQueueFinished;
}

bool
AccurateSeekTask::IsAudioDecoding() const
{
  AssertOwnerThread();
  return HasAudio() && !mIsAudioQueueFinished;
}

nsresult
AccurateSeekTask::EnsureAudioDecodeTaskQueued()
{
  AssertOwnerThread();
  SAMPLE_LOG("EnsureAudioDecodeTaskQueued isDecoding=%d status=%s",
              IsAudioDecoding(), AudioRequestStatus());

  if (!IsAudioDecoding() ||
      mReader->IsRequestingAudioData() ||
      mReader->IsWaitingAudioData() ||
      mSeekRequest.Exists()) {
    return NS_OK;
  }

  RequestAudioData();
  return NS_OK;
}

nsresult
AccurateSeekTask::EnsureVideoDecodeTaskQueued()
{
  AssertOwnerThread();
  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d status=%s",
             IsVideoDecoding(), VideoRequestStatus());

  if (!IsVideoDecoding() ||
      mReader->IsRequestingVideoData() ||
      mReader->IsWaitingVideoData() ||
      mSeekRequest.Exists()) {
    return NS_OK;
  }

  RequestVideoData();
  return NS_OK;
}

const char*
AccurateSeekTask::AudioRequestStatus()
{
  AssertOwnerThread();

  if (mReader->IsRequestingAudioData()) {
    MOZ_DIAGNOSTIC_ASSERT(!mReader->IsWaitingAudioData());
    return "pending";
  } else if (mReader->IsWaitingAudioData()) {
    return "waiting";
  }
  return "idle";
}

const char*
AccurateSeekTask::VideoRequestStatus()
{
  AssertOwnerThread();

  if (mReader->IsRequestingVideoData()) {
    MOZ_DIAGNOSTIC_ASSERT(!mReader->IsWaitingVideoData());
    return "pending";
  } else if (mReader->IsWaitingVideoData()) {
    return "waiting";
  }
  return "idle";
}

void
AccurateSeekTask::RequestAudioData()
{
  AssertOwnerThread();
  SAMPLE_LOG("Queueing audio task - queued=%i, decoder-queued=%o",
             !!mSeekedAudioData, mReader->SizeOfAudioQueueInFrames());

  mReader->RequestAudioData();
}

void
AccurateSeekTask::RequestVideoData()
{
  AssertOwnerThread();
  SAMPLE_LOG("Queueing video task - queued=%i, decoder-queued=%o, skip=%i, time=%lld",
               !!mSeekedVideoData, mReader->SizeOfVideoQueueInFrames(), false,
               media::TimeUnit().ToMicroseconds());

  mReader->RequestVideoData(false, media::TimeUnit());
}

nsresult
AccurateSeekTask::DropAudioUpToSeekTarget(MediaData* aSample)
{
  AssertOwnerThread();

  RefPtr<AudioData> audio(aSample->As<AudioData>());
  MOZ_ASSERT(audio && mSeekJob.Exists() && mSeekJob.mTarget.IsAccurate());

  CheckedInt64 sampleDuration = FramesToUsecs(audio->mFrames, mAudioRate);
  if (!sampleDuration.isValid()) {
    return NS_ERROR_FAILURE;
  }

  if (audio->mTime + sampleDuration.value() <= mSeekJob.mTarget.GetTime().ToMicroseconds()) {
    // Our seek target lies after the frames in this AudioData. Don't
    // push it onto the audio queue, and keep decoding forwards.
    return NS_OK;
  }

  if (audio->mTime > mSeekJob.mTarget.GetTime().ToMicroseconds()) {
    // The seek target doesn't lie in the audio block just after the last
    // audio frames we've seen which were before the seek target. This
    // could have been the first audio data we've seen after seek, i.e. the
    // seek terminated after the seek target in the audio stream. Just
    // abort the audio decode-to-target, the state machine will play
    // silence to cover the gap. Typically this happens in poorly muxed
    // files.
    DECODER_WARN("Audio not synced after seek, maybe a poorly muxed file?");
    mSeekedAudioData = audio;
    return NS_OK;
  }

  // The seek target lies somewhere in this AudioData's frames, strip off
  // any frames which lie before the seek target, so we'll begin playback
  // exactly at the seek target.
  NS_ASSERTION(mSeekJob.mTarget.GetTime().ToMicroseconds() >= audio->mTime,
               "Target must at or be after data start.");
  NS_ASSERTION(mSeekJob.mTarget.GetTime().ToMicroseconds() < audio->mTime + sampleDuration.value(),
               "Data must end after target.");

  CheckedInt64 framesToPrune =
    UsecsToFrames(mSeekJob.mTarget.GetTime().ToMicroseconds() - audio->mTime, mAudioRate);
  if (!framesToPrune.isValid()) {
    return NS_ERROR_FAILURE;
  }
  if (framesToPrune.value() > audio->mFrames) {
    // We've messed up somehow. Don't try to trim frames, the |frames|
    // variable below will overflow.
    DECODER_WARN("Can't prune more frames that we have!");
    return NS_ERROR_FAILURE;
  }
  uint32_t frames = audio->mFrames - static_cast<uint32_t>(framesToPrune.value());
  uint32_t channels = audio->mChannels;
  AlignedAudioBuffer audioData(frames * channels);
  if (!audioData) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  memcpy(audioData.get(),
         audio->mAudioData.get() + (framesToPrune.value() * channels),
         frames * channels * sizeof(AudioDataValue));
  CheckedInt64 duration = FramesToUsecs(frames, mAudioRate);
  if (!duration.isValid()) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<AudioData> data(new AudioData(audio->mOffset,
                                       mSeekJob.mTarget.GetTime().ToMicroseconds(),
                                       duration.value(),
                                       frames,
                                       Move(audioData),
                                       channels,
                                       audio->mRate));
  MOZ_ASSERT(!mSeekedAudioData, "Should be the 1st sample after seeking");
  mSeekedAudioData = data;

  return NS_OK;
}

nsresult
AccurateSeekTask::DropVideoUpToSeekTarget(MediaData* aSample)
{
  AssertOwnerThread();

  RefPtr<VideoData> video(aSample->As<VideoData>());
  MOZ_ASSERT(video);
  DECODER_LOG("DropVideoUpToSeekTarget() frame [%lld, %lld]",
              video->mTime, video->GetEndTime());
  MOZ_ASSERT(mSeekJob.Exists());
  const int64_t target = mSeekJob.mTarget.GetTime().ToMicroseconds();

  // If the frame end time is less than the seek target, we won't want
  // to display this frame after the seek, so discard it.
  if (target >= video->GetEndTime()) {
    DECODER_LOG("DropVideoUpToSeekTarget() pop video frame [%lld, %lld] target=%lld",
                video->mTime, video->GetEndTime(), target);
    mFirstVideoFrameAfterSeek = video;
  } else {
    if (target >= video->mTime && video->GetEndTime() >= target) {
      // The seek target lies inside this frame's time slice. Adjust the frame's
      // start time to match the seek target. We do this by replacing the
      // first frame with a shallow copy which has the new timestamp.
      RefPtr<VideoData> temp = VideoData::ShallowCopyUpdateTimestamp(video.get(), target);
      video = temp;
    }
    mFirstVideoFrameAfterSeek = nullptr;

    DECODER_LOG("DropVideoUpToSeekTarget() found video frame [%lld, %lld] containing target=%lld",
                video->mTime, video->GetEndTime(), target);

    MOZ_ASSERT(!mSeekedVideoData, "Should be the 1st sample after seeking");
    mSeekedVideoData = video;
  }

  return NS_OK;
}

bool
AccurateSeekTask::IsAudioSeekComplete()
{
  AssertOwnerThread();
  SAMPLE_LOG("IsAudioSeekComplete() curTarVal=%d mAudDis=%d aqFin=%d aqSz=%d",
      mSeekJob.Exists(), mDropAudioUntilNextDiscontinuity, mIsAudioQueueFinished, !!mSeekedAudioData);
  return
    !HasAudio() ||
    mSeekJob.mTarget.IsVideoOnly() ||
    (Exists() && !mDropAudioUntilNextDiscontinuity &&
     (mIsAudioQueueFinished || mSeekedAudioData));
}

bool
AccurateSeekTask::IsVideoSeekComplete()
{
  AssertOwnerThread();
  SAMPLE_LOG("IsVideoSeekComplete() curTarVal=%d mVidDis=%d vqFin=%d vqSz=%d",
      mSeekJob.Exists(), mDropVideoUntilNextDiscontinuity, mIsVideoQueueFinished, !!mSeekedVideoData);

  return
    !HasVideo() ||
    (Exists() && !mDropVideoUntilNextDiscontinuity &&
     (mIsVideoQueueFinished || mSeekedVideoData));
}

void
AccurateSeekTask::CheckIfSeekComplete()
{
  AssertOwnerThread();

  const bool videoSeekComplete = IsVideoSeekComplete();
  if (HasVideo() && !videoSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    if (NS_FAILED(EnsureVideoDecodeTaskQueued())) {
      DECODER_WARN("Failed to request video during seek");
      RejectIfExist(__func__);
    }
  }

  const bool audioSeekComplete = IsAudioSeekComplete();
  if (HasAudio() && !audioSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    if (NS_FAILED(EnsureAudioDecodeTaskQueued())) {
      DECODER_WARN("Failed to request audio during seek");
      RejectIfExist(__func__);
    }
  }

  SAMPLE_LOG("CheckIfSeekComplete() audioSeekComplete=%d videoSeekComplete=%d",
             audioSeekComplete, videoSeekComplete);

  if (audioSeekComplete && videoSeekComplete) {
    Resolve(__func__); // Call to MDSM::SeekCompleted();
  }
}

void
AccurateSeekTask::OnSeekResolved(media::TimeUnit)
{
  AssertOwnerThread();

  mSeekRequest.Complete();
  // We must decode the first samples of active streams, so we can determine
  // the new stream time. So dispatch tasks to do that.
  EnsureVideoDecodeTaskQueued();
  if (!mSeekJob.mTarget.IsVideoOnly()) {
    EnsureAudioDecodeTaskQueued();
  }
}

void
AccurateSeekTask::OnSeekRejected(nsresult aResult)
{
  AssertOwnerThread();

  mSeekRequest.Complete();
  MOZ_ASSERT(NS_FAILED(aResult), "Cancels should also disconnect mSeekRequest");
  RejectIfExist(__func__);
}

void
AccurateSeekTask::OnAudioDecoded(MediaData* aAudioSample)
{
  AssertOwnerThread();

  RefPtr<MediaData> audio(aAudioSample);
  MOZ_ASSERT(audio);

  // The MDSM::mDecodedAudioEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
             (audio ? audio->mTime : -1),
             (audio ? audio->GetEndTime() : -1),
             (audio ? audio->mDiscontinuity : 0));

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  if (audio->mDiscontinuity) {
    mDropAudioUntilNextDiscontinuity = false;
  }

  if (!mDropAudioUntilNextDiscontinuity) {
    // We must be after the discontinuity; we're receiving samples
    // at or after the seek target.
    if (mSeekJob.mTarget.IsFast() &&
        mSeekJob.mTarget.GetTime().ToMicroseconds() > mCurrentTimeBeforeSeek &&
        audio->mTime < mCurrentTimeBeforeSeek) {
      // We are doing a fastSeek, but we ended up *before* the previous
      // playback position. This is surprising UX, so switch to an accurate
      // seek and decode to the seek target. This is not conformant to the
      // spec, fastSeek should always be fast, but until we get the time to
      // change all Readers to seek to the keyframe after the currentTime
      // in this case, we'll just decode forward. Bug 1026330.
      mSeekJob.mTarget.SetType(SeekTarget::Accurate);
    }
    if (mSeekJob.mTarget.IsFast()) {
      // Non-precise seek; we can stop the seek at the first sample.
      mSeekedAudioData = audio;
    } else {
      // We're doing an accurate seek. We must discard
      // MediaData up to the one containing exact seek target.
      if (NS_FAILED(DropAudioUpToSeekTarget(audio.get()))) {
        RejectIfExist(__func__);
        return;
      }
    }
  }
  CheckIfSeekComplete();
}

void
AccurateSeekTask::OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  SAMPLE_LOG("OnAduioNotDecoded (aReason=%u)", aReason);

  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    // If this is a decode error, delegate to the generic error path.
    RejectIfExist(__func__);
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aReason == MediaDecoderReader::WAITING_FOR_DATA) {
    MOZ_ASSERT(mReader->IsWaitForDataSupported(),
               "Readers that send WAITING_FOR_DATA need to implement WaitForData");
    mReader->WaitForData(MediaData::AUDIO_DATA);

    // We are out of data to decode and will enter buffering mode soon.
    // We want to play the frames we have already decoded, so we stop pre-rolling
    // and ensure that loadeddata is fired as required.
    mNeedToStopPrerollingAudio = true;
    return;
  }

  if (aReason == MediaDecoderReader::CANCELED) {
    EnsureAudioDecodeTaskQueued();
    return;
  }

  if (aReason == MediaDecoderReader::END_OF_STREAM) {
    mIsAudioQueueFinished = true;
    mDropAudioUntilNextDiscontinuity = false; // To make IsAudioSeekComplete() return TRUE.
    CheckIfSeekComplete();
  }
}

void
AccurateSeekTask::OnVideoDecoded(MediaData* aVideoSample)
{
  AssertOwnerThread();

  RefPtr<MediaData> video(aVideoSample);
  MOZ_ASSERT(video);

  // The MDSM::mDecodedVideoEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
             (video ? video->mTime : -1),
             (video ? video->GetEndTime() : -1),
             (video ? video->mDiscontinuity : 0));

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  if (mDropVideoUntilNextDiscontinuity) {
    if (video->mDiscontinuity) {
      mDropVideoUntilNextDiscontinuity = false;
    }
  }

  if (!mDropVideoUntilNextDiscontinuity) {
    // We must be after the discontinuity; we're receiving samples
    // at or after the seek target.
    if (mSeekJob.mTarget.IsFast() &&
        mSeekJob.mTarget.GetTime().ToMicroseconds() > mCurrentTimeBeforeSeek &&
        video->mTime < mCurrentTimeBeforeSeek) {
      // We are doing a fastSeek, but we ended up *before* the previous
      // playback position. This is surprising UX, so switch to an accurate
      // seek and decode to the seek target. This is not conformant to the
      // spec, fastSeek should always be fast, but until we get the time to
      // change all Readers to seek to the keyframe after the currentTime
      // in this case, we'll just decode forward. Bug 1026330.
      mSeekJob.mTarget.SetType(SeekTarget::Accurate);
    }
    if (mSeekJob.mTarget.IsFast()) {
      // Non-precise seek. We can stop the seek at the first sample.
      mSeekedVideoData = video;
    } else {
      // We're doing an accurate seek. We still need to discard
      // MediaData up to the one containing exact seek target.
      if (NS_FAILED(DropVideoUpToSeekTarget(video.get()))) {
        RejectIfExist(__func__);
        return;
      }
    }
  }
  CheckIfSeekComplete();
}

void
AccurateSeekTask::OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  SAMPLE_LOG("OnVideoNotDecoded (aReason=%u)", aReason);

  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    // If this is a decode error, delegate to the generic error path.
    RejectIfExist(__func__);
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aReason == MediaDecoderReader::WAITING_FOR_DATA) {
    MOZ_ASSERT(mReader->IsWaitForDataSupported(),
               "Readers that send WAITING_FOR_DATA need to implement WaitForData");
    mReader->WaitForData(MediaData::VIDEO_DATA);

    // We are out of data to decode and will enter buffering mode soon.
    // We want to play the frames we have already decoded, so we stop pre-rolling
    // and ensure that loadeddata is fired as required.
    mNeedToStopPrerollingVideo = true;
    return;
  }

  if (aReason == MediaDecoderReader::CANCELED) {
    EnsureVideoDecodeTaskQueued();
    return;
  }

  if (aReason == MediaDecoderReader::END_OF_STREAM) {

    if (Exists() && mFirstVideoFrameAfterSeek) {
      // Null sample. Hit end of stream. If we have decoded a frame,
      // insert it into the queue so that we have something to display.
      // We make sure to do this before invoking VideoQueue().Finish()
      // below.
      mSeekedVideoData = mFirstVideoFrameAfterSeek;
      mFirstVideoFrameAfterSeek = nullptr;
    }

    mIsVideoQueueFinished = true;
    mDropVideoUntilNextDiscontinuity = false; // To make IsVideoSeekComplete() return TRUE.
    CheckIfSeekComplete();
  }
}

void
AccurateSeekTask::SetMediaDecoderReaderWrapperCallback()
{
  AssertOwnerThread();

  mAudioCallbackID =
    mReader->SetAudioCallback(this, &AccurateSeekTask::OnAudioDecoded,
                                    &AccurateSeekTask::OnAudioNotDecoded);

  mVideoCallbackID =
    mReader->SetVideoCallback(this, &AccurateSeekTask::OnVideoDecoded,
                                    &AccurateSeekTask::OnVideoNotDecoded);

  RefPtr<AccurateSeekTask> self = this;
  mWaitAudioCallbackID =
    mReader->SetWaitAudioCallback(
      [self] (MediaData::Type aType) -> void {
        self->AssertOwnerThread();
        self->EnsureAudioDecodeTaskQueued();
      },
      [self] (WaitForDataRejectValue aRejection) -> void {
        self->AssertOwnerThread();
      });

  mWaitVideoCallbackID =
    mReader->SetWaitVideoCallback(
      [self] (MediaData::Type aType) -> void {
        self->AssertOwnerThread();
        self->EnsureVideoDecodeTaskQueued();
      },
      [self] (WaitForDataRejectValue aRejection) -> void {
        self->AssertOwnerThread();
      });

  DECODER_LOG("SeekTask set audio callbacks: mAudioCallbackID = %d\n", (int)mAudioCallbackID);
  DECODER_LOG("SeekTask set video callbacks: mVideoCallbackID = %d\n", (int)mAudioCallbackID);
  DECODER_LOG("SeekTask set wait audio callbacks: mWaitAudioCallbackID = %d\n", (int)mWaitAudioCallbackID);
  DECODER_LOG("SeekTask set wait video callbacks: mWaitVideoCallbackID = %d\n", (int)mWaitVideoCallbackID);
}

void
AccurateSeekTask::CancelMediaDecoderReaderWrapperCallback()
{
  AssertOwnerThread();

  DECODER_LOG("SeekTask cancel audio callbacks: mVideoCallbackID = %d\n", (int)mAudioCallbackID);
  mReader->CancelAudioCallback(mAudioCallbackID);

  DECODER_LOG("SeekTask cancel video callbacks: mVideoCallbackID = %d\n", (int)mVideoCallbackID);
  mReader->CancelVideoCallback(mVideoCallbackID);

  DECODER_LOG("SeekTask cancel wait audio callbacks: mWaitAudioCallbackID = %d\n", (int)mWaitAudioCallbackID);
  mReader->CancelWaitAudioCallback(mWaitAudioCallbackID);

  DECODER_LOG("SeekTask cancel wait video callbacks: mWaitVideoCallbackID = %d\n", (int)mWaitVideoCallbackID);
  mReader->CancelWaitVideoCallback(mWaitVideoCallbackID);
}
} // namespace mozilla
