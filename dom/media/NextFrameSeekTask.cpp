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

extern LazyLogModule gMediaDecoderLog;
extern LazyLogModule gMediaSampleLog;

// avoid redefined macro in unified build
#undef LOG
#undef DECODER_LOG
#undef VERBOSE_LOG

#define LOG(m, l, x, ...) \
  MOZ_LOG(m, l, ("[NextFrameSeekTask] Decoder=%p " x, mDecoderID, ##__VA_ARGS__))
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

namespace media {

NextFrameSeekTask::NextFrameSeekTask(const void* aDecoderID,
                                   AbstractThread* aThread,
                                   MediaDecoderReaderWrapper* aReader,
                                   SeekJob&& aSeekJob,
                                   const MediaInfo& aInfo,
                                   const media::TimeUnit& aDuration,
                                   int64_t aCurrentMediaTime,
                                   MediaQueue<MediaData>& aAudioQueue,
                                   MediaQueue<MediaData>& aVideoQueue)
  : SeekTask(aDecoderID, aThread, aReader, Move(aSeekJob))
  , mAudioQueue(aAudioQueue)
  , mVideoQueue(aVideoQueue)
  , mCurrentTimeBeforeSeek(aCurrentMediaTime)
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

  // Disconnect MediaDecoder.
  mSeekJob.RejectIfExists(__func__);

  // Disconnect MDSM.
  RejectIfExist(__func__);

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

static int64_t
FindNextFrame(MediaQueue<MediaData>& aQueue, int64_t aTime)
{
  AutoTArray<RefPtr<MediaData>, 16> frames;
  aQueue.GetFirstElements(aQueue.GetSize(), &frames);
  for (auto&& frame : frames) {
    if (frame->mTime > aTime) {
      return frame->mTime;
    }
  }
  return -1;
}

static void
DropFramesUntil(MediaQueue<MediaData>& aQueue, int64_t aTime) {
  while (aQueue.GetSize() > 0) {
    if (aQueue.PeekFront()->mTime < aTime) {
      RefPtr<MediaData> releaseMe = aQueue.PopFront();
      continue;
    }
    break;
  }
}

static void
DropAllFrames(MediaQueue<MediaData>& aQueue) {
  while(aQueue.GetSize() > 0) {
    RefPtr<MediaData> releaseMe = aQueue.PopFront();
  }
}

static void
DropAllMediaDataBeforeCurrentPosition(MediaQueue<MediaData>& aAudioQueue,
                                      MediaQueue<MediaData>& aVideoQueue,
                                      int64_t const aCurrentTimeBeforeSeek)
{
  // Drop all audio/video data before GetMediaTime();
  int64_t newPos = FindNextFrame(aVideoQueue, aCurrentTimeBeforeSeek);
  if (newPos < 0) {
    // In this case, we cannot find the next frame in the video queue, so
    // the NextFrameSeekTask needs to decode video data.
    DropAllFrames(aVideoQueue);
    if (aVideoQueue.IsFinished()) {
      DropAllFrames(aAudioQueue);
    }
  } else {
    DropFramesUntil(aVideoQueue, newPos);
    DropFramesUntil(aAudioQueue, newPos);
    // So now, the 1st data in the video queue should be the target of the
    // NextFrameSeekTask.
  }
}

RefPtr<NextFrameSeekTask::SeekTaskPromise>
NextFrameSeekTask::Seek(const media::TimeUnit&)
{
  AssertOwnerThread();

  DropAllMediaDataBeforeCurrentPosition(mAudioQueue, mVideoQueue,
                                        mCurrentTimeBeforeSeek);

  if (NeedMoreVideo()) {
    EnsureVideoDecodeTaskQueued();
  }
  if (!IsAudioSeekComplete() || !IsVideoSeekComplete()) {
    return mSeekTaskPromise.Ensure(__func__);
  }

  UpdateSeekTargetTime();
  SeekTaskResolveValue val = {};  // Zero-initialize data members.
  return SeekTask::SeekTaskPromise::CreateAndResolve(val, __func__);
}

bool
NextFrameSeekTask::IsVideoDecoding() const
{
  AssertOwnerThread();
  return !mIsVideoQueueFinished;
}

void
NextFrameSeekTask::EnsureVideoDecodeTaskQueued()
{
  AssertOwnerThread();
  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d status=%s",
             IsVideoDecoding(), VideoRequestStatus());

  if (!IsVideoDecoding() || IsVideoRequestPending()) {
    return;
  }

  RequestVideoData();
}

const char*
NextFrameSeekTask::VideoRequestStatus()
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
NextFrameSeekTask::RequestVideoData()
{
  AssertOwnerThread();
  SAMPLE_LOG("Queueing video task - queued=%i, decoder-queued=%o",
             !!mSeekedVideoData, mReader->SizeOfVideoQueueInFrames());

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

  const bool audioSeekComplete = IsAudioSeekComplete();

  const bool videoSeekComplete = IsVideoSeekComplete();
  if (!videoSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    EnsureVideoDecodeTaskQueued();
  }

  SAMPLE_LOG("CheckIfSeekComplete() audioSeekComplete=%d videoSeekComplete=%d",
    audioSeekComplete, videoSeekComplete);

  if (audioSeekComplete && videoSeekComplete) {
    UpdateSeekTargetTime();
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

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
             aAudioSample->mTime,
             aAudioSample->GetEndTime(),
             aAudioSample->mDiscontinuity);

  // We accept any audio data here.
  mSeekedAudioData = aAudioSample;

  MaybeFinishSeek();
}

void
NextFrameSeekTask::OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  SAMPLE_LOG("OnAudioNotDecoded (aReason=%u)", aReason);

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

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
             aVideoSample->mTime,
             aVideoSample->GetEndTime(),
             aVideoSample->mDiscontinuity);

  if (aVideoSample->mTime > mCurrentTimeBeforeSeek) {
    mSeekedVideoData = aVideoSample;
  }

  MaybeFinishSeek();
}

void
NextFrameSeekTask::OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mSeekTaskPromise.IsEmpty(), "Seek shouldn't be finished");

  SAMPLE_LOG("OnVideoNotDecoded (aReason=%u)", aReason);

  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    if (mVideoQueue.GetSize() > 0) {
      // The video decoding request might be filed by MDSM not the
      // NextFrameSeekTask itself. So, the NextFrameSeekTask might has already
      // found its target in the VideoQueue but still waits the video decoding
      // request (which is filed by the MDSM) to be resolved. In this case, we
      // already have the target of this seek task, try to resolve this task.
      MaybeFinishSeek();
      return;
    }

    // Otherwise, we cannot get the target video frame of this seek task,
    // delegate the decode error to the generic error path.
    RejectIfExist(__func__);
    return;
  }

  // If the decoder is waiting for data, we tell it to call us back when the
  // data arrives.
  if (aReason == MediaDecoderReader::WAITING_FOR_DATA) {
    mReader->WaitForData(MediaData::VIDEO_DATA);
    return;
  }

  if (aReason == MediaDecoderReader::CANCELED) {
    EnsureVideoDecodeTaskQueued();
    return;
  }

  if (aReason == MediaDecoderReader::END_OF_STREAM) {
    mIsVideoQueueFinished = true;
    MaybeFinishSeek();
  }
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
      OnAudioNotDecoded(aData.as<MediaDecoderReader::NotDecodedReason>());
    }
  });

  mVideoCallback = mReader->VideoCallback().Connect(
    OwnerThread(), [this] (VideoCallbackData aData) {
    typedef Tuple<MediaData*, TimeStamp> Type;
    if (aData.is<Type>()) {
      OnVideoDecoded(Get<0>(aData.as<Type>()));
    } else {
      OnVideoNotDecoded(aData.as<MediaDecoderReader::NotDecodedReason>());
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
    if (aData.is<MediaData::Type>()) {
      EnsureVideoDecodeTaskQueued();
    }
  });
}

void
NextFrameSeekTask::CancelCallbacks()
{
  AssertOwnerThread();
  mAudioCallback.Disconnect();
  mVideoCallback.Disconnect();
  mAudioWaitCallback.Disconnect();
  mVideoWaitCallback.Disconnect();
}

void
NextFrameSeekTask::UpdateSeekTargetTime()
{
  AssertOwnerThread();

  RefPtr<MediaData> data = mVideoQueue.PeekFront();
  if (data) {
    mSeekJob.mTarget.SetTime(TimeUnit::FromMicroseconds(data->mTime));
  } else if (mSeekedVideoData) {
    mSeekJob.mTarget.SetTime(TimeUnit::FromMicroseconds(mSeekedVideoData->mTime));
  } else if (mIsVideoQueueFinished || mVideoQueue.AtEndOfStream()) {
    mSeekJob.mTarget.SetTime(mDuration);
  } else {
    MOZ_ASSERT(false, "No data!");
  }
}
} // namespace media
} // namespace mozilla
