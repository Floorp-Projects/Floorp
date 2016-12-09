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
#undef FMT
#undef DECODER_LOG
#undef SAMPLE_LOG
#undef DECODER_WARN

#define FMT(x, ...) "[AccurateSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define DECODER_LOG(...) MOZ_LOG(gMediaDecoderLog, LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define SAMPLE_LOG(...)  MOZ_LOG(gMediaSampleLog,  LogLevel::Debug,   (FMT(__VA_ARGS__)))
#define DECODER_WARN(...) NS_WARNING(nsPrintfCString(FMT(__VA_ARGS__)).get())

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
  RejectIfExist(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  mIsDiscarded = true;
}

int64_t
AccurateSeekTask::CalculateNewCurrentTime() const
{
  AssertOwnerThread();

  const int64_t seekTime = mTarget.GetTime().ToMicroseconds();

  // For the accurate seek, we always set the newCurrentTime = seekTime so that
  // the updated HTMLMediaElement.currentTime will always be the seek target;
  // we rely on the MediaSink to handles the gap between the newCurrentTime and
  // the real decoded samples' start time.
  if (mTarget.IsAccurate()) {
    return seekTime;
  }

  // For the fast seek, we update the newCurrentTime with the decoded audio and
  // video samples, set it to be the one which is closet to the seekTime.
  if (mTarget.IsFast()) {

    // A situation that both audio and video approaches the end.
    if (!mSeekedAudioData && !mSeekedVideoData) {
      return seekTime;
    }

    const int64_t audioStart = mSeekedAudioData ? mSeekedAudioData->mTime : INT64_MAX;
    const int64_t videoStart = mSeekedVideoData ? mSeekedVideoData->mTime : INT64_MAX;
    const int64_t audioGap = std::abs(audioStart - seekTime);
    const int64_t videoGap = std::abs(videoStart - seekTime);
    return audioGap <= videoGap ? audioStart : videoStart;
  }

  MOZ_ASSERT(false, "AccurateSeekTask doesn't handle other seek types.");
  return 0;
}

void
AccurateSeekTask::HandleAudioDecoded(MediaData* aAudio)
{
}

void
AccurateSeekTask::HandleVideoDecoded(MediaData* aVideo, TimeStamp aDecodeStart)
{
}

void
AccurateSeekTask::HandleNotDecoded(MediaData::Type aType, const MediaResult& aError)
{
}

void
AccurateSeekTask::HandleAudioWaited(MediaData::Type aType)
{
}

void
AccurateSeekTask::HandleVideoWaited(MediaData::Type aType)
{
}

void
AccurateSeekTask::HandleNotWaited(const WaitForDataRejectValue& aRejection)
{
}

RefPtr<AccurateSeekTask::SeekTaskPromise>
AccurateSeekTask::Seek(const media::TimeUnit& aDuration)
{
  AssertOwnerThread();

  return mSeekTaskPromise.Ensure(__func__);
}

nsresult
AccurateSeekTask::DropAudioUpToSeekTarget(MediaData* aSample)
{
  AssertOwnerThread();

  RefPtr<AudioData> audio(aSample->As<AudioData>());
  MOZ_ASSERT(audio && mTarget.IsAccurate());

  CheckedInt64 sampleDuration = FramesToUsecs(audio->mFrames, mAudioRate);
  if (!sampleDuration.isValid()) {
    return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
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
    return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
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
    return NS_ERROR_DOM_MEDIA_OVERFLOW_ERR;
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

} // namespace mozilla
