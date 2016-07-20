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
                                   const SeekTarget& aTarget,
                                   const MediaInfo& aInfo,
                                   const media::TimeUnit& aEnd,
                                   int64_t aCurrentMediaTime)
  : SeekTask(aDecoderID, aThread, aReader, aTarget)
  , mCurrentTimeBeforeSeek(media::TimeUnit::FromMicroseconds(aCurrentMediaTime))
  , mAudioRate(aInfo.mAudio.mRate)
  , mDoneAudioSeeking(!aInfo.HasAudio() || aTarget.IsVideoOnly())
  , mDoneVideoSeeking(!aInfo.HasVideo())
{
  AssertOwnerThread();

  // Bound the seek time to be inside the media range.
  NS_ASSERTION(aEnd.ToMicroseconds() != -1, "Should know end time by now");
  mTarget.SetTime(std::max(media::TimeUnit(), std::min(mTarget.GetTime(), aEnd)));

  // Configure MediaDecoderReaderWrapper.
  SetCallbacks();
}

AccurateSeekTask::~AccurateSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

void
AccurateSeekTask::Discard()
{
  AssertOwnerThread();

  // Disconnect MDSM.
  RejectIfExist(__func__);

  // Disconnect MediaDecoderReaderWrapper.
  mSeekRequest.DisconnectIfExists();
  CancelCallbacks();

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
  mSeekRequest.Begin(mReader->Seek(mTarget, aDuration)
    ->Then(OwnerThread(), __func__, this,
           &AccurateSeekTask::OnSeekResolved, &AccurateSeekTask::OnSeekRejected));

  return mSeekTaskPromise.Ensure(__func__);
}

void
AccurateSeekTask::RequestAudioData()
{
  AssertOwnerThread();
  MOZ_ASSERT(!mDoneAudioSeeking);
  MOZ_ASSERT(!mReader->IsRequestingAudioData());
  MOZ_ASSERT(!mReader->IsWaitingAudioData());
  mReader->RequestAudioData();
}

void
AccurateSeekTask::RequestVideoData()
{
  AssertOwnerThread();
  MOZ_ASSERT(!mDoneVideoSeeking);
  MOZ_ASSERT(!mReader->IsRequestingVideoData());
  MOZ_ASSERT(!mReader->IsWaitingVideoData());
  mReader->RequestVideoData(false, media::TimeUnit());
}

nsresult
AccurateSeekTask::DropAudioUpToSeekTarget(MediaData* aSample)
{
  AssertOwnerThread();

  RefPtr<AudioData> audio(aSample->As<AudioData>());
  MOZ_ASSERT(audio && mTarget.IsAccurate());

  CheckedInt64 sampleDuration = FramesToUsecs(audio->mFrames, mAudioRate);
  if (!sampleDuration.isValid()) {
    return NS_ERROR_FAILURE;
  }

  if (audio->mTime + sampleDuration.value() <= mTarget.GetTime().ToMicroseconds()) {
    // Our seek target lies after the frames in this AudioData. Don't
    // push it onto the audio queue, and keep decoding forwards.
    return NS_OK;
  }

  if (audio->mTime > mTarget.GetTime().ToMicroseconds()) {
    // The seek target doesn't lie in the audio block just after the last
    // audio frames we've seen which were before the seek target. This
    // could have been the first audio data we've seen after seek, i.e. the
    // seek terminated after the seek target in the audio stream. Just
    // abort the audio decode-to-target, the state machine will play
    // silence to cover the gap. Typically this happens in poorly muxed
    // files.
    DECODER_WARN("Audio not synced after seek, maybe a poorly muxed file?");
    mSeekedAudioData = audio;
    mDoneAudioSeeking = true;
    return NS_OK;
  }

  // The seek target lies somewhere in this AudioData's frames, strip off
  // any frames which lie before the seek target, so we'll begin playback
  // exactly at the seek target.
  NS_ASSERTION(mTarget.GetTime().ToMicroseconds() >= audio->mTime,
               "Target must at or be after data start.");
  NS_ASSERTION(mTarget.GetTime().ToMicroseconds() < audio->mTime + sampleDuration.value(),
               "Data must end after target.");

  CheckedInt64 framesToPrune =
    UsecsToFrames(mTarget.GetTime().ToMicroseconds() - audio->mTime, mAudioRate);
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
                                       mTarget.GetTime().ToMicroseconds(),
                                       duration.value(),
                                       frames,
                                       Move(audioData),
                                       channels,
                                       audio->mRate));
  MOZ_ASSERT(!mSeekedAudioData, "Should be the 1st sample after seeking");
  mSeekedAudioData = data;
  mDoneAudioSeeking = true;

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
  const int64_t target = mTarget.GetTime().ToMicroseconds();

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
    mDoneVideoSeeking = true;
  }

  return NS_OK;
}

void
AccurateSeekTask::MaybeFinishSeek()
{
  AssertOwnerThread();
  if (mDoneAudioSeeking && mDoneVideoSeeking) {
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
  if (!mDoneVideoSeeking) {
    RequestVideoData();
  }
  if (!mDoneAudioSeeking) {
    RequestAudioData();
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
AccurateSeekTask::AdjustFastSeekIfNeeded(MediaData* aSample)
{
  AssertOwnerThread();
  if (mTarget.IsFast() &&
      mTarget.GetTime() > mCurrentTimeBeforeSeek &&
      aSample->mTime < mCurrentTimeBeforeSeek.ToMicroseconds()) {
    // We are doing a fastSeek, but we ended up *before* the previous
    // playback position. This is surprising UX, so switch to an accurate
    // seek and decode to the seek target. This is not conformant to the
    // spec, fastSeek should always be fast, but until we get the time to
    // change all Readers to seek to the keyframe after the currentTime
    // in this case, we'll just decode forward. Bug 1026330.
    mTarget.SetType(SeekTarget::Accurate);
  }
}

void
AccurateSeekTask::OnAudioDecoded(MediaData* aAudioSample)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  RefPtr<MediaData> audio(aAudioSample);
  MOZ_ASSERT(audio);

  // The MDSM::mDecodedAudioEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
    audio->mTime, audio->GetEndTime(), audio->mDiscontinuity);

  // Video-only seek doesn't reset audio decoder. There might be pending audio
  // requests when AccurateSeekTask::Seek() begins. We will just store the data
  // without checking |mDiscontinuity| or calling DropAudioUpToSeekTarget().
  if (mTarget.IsVideoOnly()) {
    mSeekedAudioData = audio.forget();
    return;
  }

  if (mFirstAudioSample) {
    mFirstAudioSample = false;
    MOZ_ASSERT(audio->mDiscontinuity);
  }

  AdjustFastSeekIfNeeded(audio);

  if (mTarget.IsFast()) {
    // Non-precise seek; we can stop the seek at the first sample.
    mSeekedAudioData = audio;
    mDoneAudioSeeking = true;
  } else if (NS_FAILED(DropAudioUpToSeekTarget(audio))) {
    CancelCallbacks();
    RejectIfExist(__func__);
    return;
  }

  if (!mDoneAudioSeeking) {
    RequestAudioData();
    return;
  }
  MaybeFinishSeek();
}

void
AccurateSeekTask::OnNotDecoded(MediaData::Type aType,
                               MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  SAMPLE_LOG("OnNotDecoded type=%d reason=%u", aType, aReason);

  // Ignore pending requests from video-only seek.
  if (aType == MediaData::AUDIO_DATA && mTarget.IsVideoOnly()) {
    return;
  }

  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    // If this is a decode error, delegate to the generic error path.
    CancelCallbacks();
    RejectIfExist(__func__);
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aReason == MediaDecoderReader::WAITING_FOR_DATA) {
    mReader->WaitForData(aType);
    return;
  }

  if (aReason == MediaDecoderReader::CANCELED) {
    if (aType == MediaData::AUDIO_DATA) {
      RequestAudioData();
    } else {
      RequestVideoData();
    }
    return;
  }

  if (aReason == MediaDecoderReader::END_OF_STREAM) {
    if (aType == MediaData::AUDIO_DATA) {
      mIsAudioQueueFinished = true;
      mDoneAudioSeeking = true;
    } else {
      mIsVideoQueueFinished = true;
      mDoneVideoSeeking = true;
      if (mFirstVideoFrameAfterSeek) {
        // Hit the end of stream. Move mFirstVideoFrameAfterSeek into
        // mSeekedVideoData so we have something to display after seeking.
        mSeekedVideoData = mFirstVideoFrameAfterSeek.forget();
      }
    }
    MaybeFinishSeek();
  }
}

void
AccurateSeekTask::OnVideoDecoded(MediaData* aVideoSample)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  RefPtr<MediaData> video(aVideoSample);
  MOZ_ASSERT(video);

  // The MDSM::mDecodedVideoEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
    video->mTime, video->GetEndTime(), video->mDiscontinuity);

  if (mFirstVideoSample) {
    mFirstVideoSample = false;
    MOZ_ASSERT(video->mDiscontinuity);
  }

  AdjustFastSeekIfNeeded(video);

  if (mTarget.IsFast()) {
    // Non-precise seek. We can stop the seek at the first sample.
    mSeekedVideoData = video;
    mDoneVideoSeeking = true;
  } else if (NS_FAILED(DropVideoUpToSeekTarget(video.get()))) {
    CancelCallbacks();
    RejectIfExist(__func__);
    return;
  }

  if (!mDoneVideoSeeking) {
    RequestVideoData();
    return;
  }
  MaybeFinishSeek();
}

void
AccurateSeekTask::SetCallbacks()
{
  AssertOwnerThread();

  mAudioCallback = mReader->AudioCallback().Connect(
    OwnerThread(), [this] (AudioCallbackData aData) {
    if (aData.is<MediaData*>()) {
      OnAudioDecoded(aData.as<MediaData*>());
    } else {
      OnNotDecoded(MediaData::AUDIO_DATA,
        aData.as<MediaDecoderReader::NotDecodedReason>());
    }
  });

  mVideoCallback = mReader->VideoCallback().Connect(
    OwnerThread(), [this] (VideoCallbackData aData) {
    typedef Tuple<MediaData*, TimeStamp> Type;
    if (aData.is<Type>()) {
      OnVideoDecoded(Get<0>(aData.as<Type>()));
    } else {
      OnNotDecoded(MediaData::VIDEO_DATA,
        aData.as<MediaDecoderReader::NotDecodedReason>());
    }
  });

  mAudioWaitCallback = mReader->AudioWaitCallback().Connect(
    OwnerThread(), [this] (WaitCallbackData aData) {
    // Ignore pending requests from video-only seek.
    if (mTarget.IsVideoOnly()) {
      return;
    }
    if (aData.is<MediaData::Type>()) {
      RequestAudioData();
    }
  });

  mVideoWaitCallback = mReader->VideoWaitCallback().Connect(
    OwnerThread(), [this] (WaitCallbackData aData) {
    if (aData.is<MediaData::Type>()) {
      RequestVideoData();
    }
  });
}

void
AccurateSeekTask::CancelCallbacks()
{
  AssertOwnerThread();
  mAudioCallback.DisconnectIfExists();
  mVideoCallback.DisconnectIfExists();
  mAudioWaitCallback.DisconnectIfExists();
  mVideoWaitCallback.DisconnectIfExists();
}
} // namespace mozilla
