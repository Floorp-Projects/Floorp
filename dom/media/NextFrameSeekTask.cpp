/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NextFrameSeekTask.h"
#include "MediaDecoderReaderWrapper.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/Assertions.h"
#include "nsPrintfCString.h"

namespace mozilla {

extern LazyLogModule gMediaSampleLog;

#define SAMPLE_LOG(x, ...) MOZ_LOG(gMediaSampleLog, LogLevel::Debug, \
  ("[NextFrameSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__))

namespace media {

NextFrameSeekTask::NextFrameSeekTask(const void* aDecoderID,
                                     AbstractThread* aThread,
                                     MediaDecoderReaderWrapper* aReader,
                                     const SeekTarget& aTarget,
                                     const MediaInfo& aInfo,
                                     const media::TimeUnit& aDuration,
                                     int64_t aCurrentTime,
                                     MediaQueue<MediaData>& aAudioQueue,
                                     MediaQueue<MediaData>& aVideoQueue)
  : SeekTask(aDecoderID, aThread, aReader, aTarget)
  , mAudioQueue(aAudioQueue)
  , mVideoQueue(aVideoQueue)
  , mCurrentTime(aCurrentTime)
  , mDuration(aDuration)
{
  AssertOwnerThread();
  MOZ_ASSERT(aInfo.HasVideo());

  // Configure MediaDecoderReaderWrapper.
  SetCallbacks();
}

NextFrameSeekTask::~NextFrameSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

void
NextFrameSeekTask::Discard()
{
  AssertOwnerThread();

  // Disconnect MDSM.
  RejectIfExist(NS_ERROR_DOM_MEDIA_CANCELED, __func__);

  // Disconnect MediaDecoderReader.
  CancelCallbacks();

  mIsDiscarded = true;
}

bool
NextFrameSeekTask::NeedToResetMDSM() const
{
  AssertOwnerThread();
  return false;
}

int64_t
NextFrameSeekTask::CalculateNewCurrentTime() const
{
  AssertOwnerThread();

  // The HTMLMediaElement.currentTime should be updated to the seek target
  // which has been updated to the next frame's time.
  return mTarget.GetTime().ToMicroseconds();
}

/*
 * Remove samples from the queue until aCompare() returns false.
 * aCompare A function object with the signature bool(int64_t) which returns
 *          true for samples that should be removed.
 */
template <typename Function> static void
DiscardFrames(MediaQueue<MediaData>& aQueue, const Function& aCompare)
{
  while(aQueue.GetSize() > 0) {
    if (aCompare(aQueue.PeekFront()->mTime)) {
      RefPtr<MediaData> releaseMe = aQueue.PopFront();
      continue;
    }
    break;
  }
}

RefPtr<NextFrameSeekTask::SeekTaskPromise>
NextFrameSeekTask::Seek(const media::TimeUnit&)
{
  AssertOwnerThread();

  auto currentTime = mCurrentTime;
  DiscardFrames(mVideoQueue, [currentTime] (int64_t aSampleTime) {
    return aSampleTime <= currentTime;
  });

  RefPtr<SeekTaskPromise> promise = mSeekTaskPromise.Ensure(__func__);
  if (!IsVideoRequestPending() && NeedMoreVideo()) {
    RequestVideoData();
  }
  MaybeFinishSeek(); // Might resolve mSeekTaskPromise and modify audio queue.
  return promise;
}

void
NextFrameSeekTask::RequestVideoData()
{
  AssertOwnerThread();
  mReader->RequestVideoData(false, media::TimeUnit());
}

bool
NextFrameSeekTask::NeedMoreVideo() const
{
  AssertOwnerThread();
  // Need to request video when we have none and video queue is not finished.
  return mVideoQueue.GetSize() == 0 &&
         !mSeekedVideoData &&
         !mVideoQueue.IsFinished() &&
         !mIsVideoQueueFinished;
}

bool
NextFrameSeekTask::IsVideoRequestPending() const
{
  AssertOwnerThread();
  return mReader->IsRequestingVideoData() || mReader->IsWaitingVideoData();
}

bool
NextFrameSeekTask::IsAudioSeekComplete() const
{
  AssertOwnerThread();
  // Don't finish seek until there are no pending requests. Otherwise, we might
  // lose audio samples for the promise is resolved asynchronously.
  return !mReader->IsRequestingAudioData() && !mReader->IsWaitingAudioData();
}

bool
NextFrameSeekTask::IsVideoSeekComplete() const
{
  AssertOwnerThread();
  // Don't finish seek until there are no pending requests. Otherwise, we might
  // lose video samples for the promise is resolved asynchronously.
  return !IsVideoRequestPending() && !NeedMoreVideo();
}

void
NextFrameSeekTask::MaybeFinishSeek()
{
  AssertOwnerThread();
  if (IsAudioSeekComplete() && IsVideoSeekComplete()) {
    UpdateSeekTargetTime();

    auto time = mTarget.GetTime().ToMicroseconds();
    DiscardFrames(mAudioQueue, [time] (int64_t aSampleTime) {
      return aSampleTime < time;
    });

    Resolve(__func__); // Call to MDSM::SeekCompleted();
  }
}

void
NextFrameSeekTask::OnAudioDecoded(MediaData* aAudioSample)
{
  AssertOwnerThread();
  MOZ_ASSERT(aAudioSample);
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  // The MDSM::mDecodedAudioEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld]",
             aAudioSample->mTime,
             aAudioSample->GetEndTime());

  // We accept any audio data here.
  mSeekedAudioData = aAudioSample;

  MaybeFinishSeek();
}

void
NextFrameSeekTask::OnAudioNotDecoded(const MediaResult& aError)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  SAMPLE_LOG("OnAudioNotDecoded (aError=%u)", aError.Code());

  // We don't really handle audio deocde error here. Let MDSM to trigger further
  // audio decoding tasks if it needs to play audio, and MDSM will then receive
  // the decoding state from MediaDecoderReader.

  MaybeFinishSeek();
}

void
NextFrameSeekTask::OnVideoDecoded(MediaData* aVideoSample)
{
  AssertOwnerThread();
  MOZ_ASSERT(aVideoSample);
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  // The MDSM::mDecodedVideoEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld]",
             aVideoSample->mTime,
             aVideoSample->GetEndTime());

  if (aVideoSample->mTime > mCurrentTime) {
    mSeekedVideoData = aVideoSample;
  }

  if (NeedMoreVideo()) {
    RequestVideoData();
    return;
  }

  MaybeFinishSeek();
}

void
NextFrameSeekTask::OnVideoNotDecoded(const MediaResult& aError)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  SAMPLE_LOG("OnVideoNotDecoded (aError=%u)", aError.Code());

  if (aError == NS_ERROR_DOM_MEDIA_END_OF_STREAM) {
    mIsVideoQueueFinished = true;
  }

  // Video seek not finished.
  if (NeedMoreVideo()) {
    switch (aError.Code()) {
      case NS_ERROR_DOM_MEDIA_WAITING_FOR_DATA:
        mReader->WaitForData(MediaData::VIDEO_DATA);
        break;
      case NS_ERROR_DOM_MEDIA_CANCELED:
        RequestVideoData();
        break;
      case NS_ERROR_DOM_MEDIA_END_OF_STREAM:
        MOZ_ASSERT(false, "Shouldn't want more data for ended video.");
        break;
      default:
        // We might lose the audio sample after canceling the callbacks.
        // However it doesn't really matter because MDSM is gonna shut down
        // when seek fails.
        CancelCallbacks();
        // Reject the promise since we can't finish video seek anyway.
        RejectIfExist(aError, __func__);
        break;
    }
    return;
  }

  MaybeFinishSeek();
}

void
NextFrameSeekTask::SetCallbacks()
{
  AssertOwnerThread();

  // Register dummy callbcak for audio decoding since we don't need to handle
  // the decoded audio samples.
  mAudioCallback = mReader->AudioCallback().Connect(
    OwnerThread(), [this] (AudioCallbackData aData) {
    if (aData.is<MediaData*>()) {
      OnAudioDecoded(aData.as<MediaData*>());
    } else {
      OnAudioNotDecoded(aData.as<MediaResult>());
    }
  });

  mVideoCallback = mReader->VideoCallback().Connect(
    OwnerThread(), [this] (VideoCallbackData aData) {
    typedef Tuple<MediaData*, TimeStamp> Type;
    if (aData.is<Type>()) {
      OnVideoDecoded(Get<0>(aData.as<Type>()));
    } else {
      OnVideoNotDecoded(aData.as<MediaResult>());
    }
  });

  mAudioWaitCallback = mReader->AudioWaitCallback().Connect(
    OwnerThread(), [this] (WaitCallbackData aData) {
    // We don't make an audio decode request here, instead, let MDSM to
    // trigger further audio decode tasks if MDSM itself needs to play audio.
    MaybeFinishSeek();
  });

  mVideoWaitCallback = mReader->VideoWaitCallback().Connect(
    OwnerThread(), [this] (WaitCallbackData aData) {
    if (NeedMoreVideo()) {
      if (aData.is<MediaData::Type>()) {
        RequestVideoData();
      } else {
        // Reject if we can't finish video seeking.
        CancelCallbacks();
        RejectIfExist(NS_ERROR_DOM_MEDIA_CANCELED, __func__);
      }
      return;
    }
    MaybeFinishSeek();
  });
}

void
NextFrameSeekTask::CancelCallbacks()
{
  AssertOwnerThread();
  mAudioCallback.DisconnectIfExists();
  mVideoCallback.DisconnectIfExists();
  mAudioWaitCallback.DisconnectIfExists();
  mVideoWaitCallback.DisconnectIfExists();
}

void
NextFrameSeekTask::UpdateSeekTargetTime()
{
  AssertOwnerThread();

  RefPtr<MediaData> data = mVideoQueue.PeekFront();
  if (data) {
    mTarget.SetTime(TimeUnit::FromMicroseconds(data->mTime));
  } else if (mSeekedVideoData) {
    mTarget.SetTime(TimeUnit::FromMicroseconds(mSeekedVideoData->mTime));
  } else if (mIsVideoQueueFinished || mVideoQueue.AtEndOfStream()) {
    mTarget.SetTime(mDuration);
  } else {
    MOZ_ASSERT(false, "No data!");
  }
}
} // namespace media
} // namespace mozilla
