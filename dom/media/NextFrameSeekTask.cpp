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
  , mHasAudio(aInfo.HasAudio())
  , mHasVideo(aInfo.HasVideo())
  , mDuration(aDuration)
{
  AssertOwnerThread();
  MOZ_ASSERT(HasVideo());

  // Configure MediaDecoderReaderWrapper.
  SetMediaDecoderReaderWrapperCallback();
}

NextFrameSeekTask::~NextFrameSeekTask()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsDiscarded);
}

bool
NextFrameSeekTask::HasAudio() const
{
  AssertOwnerThread();
  return mHasAudio;
}

bool
NextFrameSeekTask::HasVideo() const
{
  AssertOwnerThread();
  return mHasVideo;
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
  CancelMediaDecoderReaderWrapperCallback();

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

  // While creating this seek task object, MDSM might had already ask the
  // wrapper to decode a media sample or the MDSM is waiting a media data.
  // If so, we cannot resolve the SeekTaskPromise immediately because there is
  // a latency of running the resolving runnable. Instead, if there is a pending
  // media request, we wait for it.
  if ((mVideoQueue.GetSize() > 0
       && !mReader->IsRequestingAudioData() && !mReader->IsWaitingAudioData()
       && !mReader->IsRequestingVideoData() && !mReader->IsWaitingVideoData())
      || mVideoQueue.AtEndOfStream()) {
    UpdateSeekTargetTime();
    SeekTaskResolveValue val = {};  // Zero-initialize data members.
    return SeekTask::SeekTaskPromise::CreateAndResolve(val, __func__);
  } else {
    // Only invoke EnsureVideoDecodeTaskQueued() if we have no video data; we
    // might be here because we are waiting audio data, and don't bother to make
    // more requests to reader in this case.
    if (mVideoQueue.GetSize() == 0) {
      EnsureVideoDecodeTaskQueued();
    }
    return mSeekTaskPromise.Ensure(__func__);
  }
}

bool
NextFrameSeekTask::IsVideoDecoding() const
{
  AssertOwnerThread();
  return HasVideo() && !mIsVideoQueueFinished;
}

nsresult
NextFrameSeekTask::EnsureVideoDecodeTaskQueued()
{
  AssertOwnerThread();
  SAMPLE_LOG("EnsureVideoDecodeTaskQueued isDecoding=%d status=%s",
             IsVideoDecoding(), VideoRequestStatus());

  if (!IsVideoDecoding() ||
      mReader->IsRequestingVideoData() ||
      mReader->IsWaitingVideoData()) {
    return NS_OK;
  }

  RequestVideoData();
  return NS_OK;
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
NextFrameSeekTask::IsAudioSeekComplete()
{
  AssertOwnerThread();
  SAMPLE_LOG("IsAudioSeekComplete() curTarVal=%d aqFin=%d aqSz=%d req=%d wait=%d",
    mSeekJob.Exists(), mIsAudioQueueFinished, !!mSeekedAudioData,
    mReader->IsRequestingAudioData(), mReader->IsWaitingAudioData());

  // Just make sure that we are not requesting or waiting for audio data. We
  // don't really need to get an decoded audio data or get EOS here.
  return
    !HasAudio() ||
    (Exists() && !mReader->IsRequestingAudioData() && !mReader->IsWaitingAudioData());
}

bool
NextFrameSeekTask::IsVideoSeekComplete()
{
  AssertOwnerThread();
  SAMPLE_LOG("IsVideoSeekComplete() curTarVal=%d vqFin=%d vqSz=%d",
      mSeekJob.Exists(), mIsVideoQueueFinished, !!mSeekedVideoData);

  return
    !HasVideo() || (Exists() && (mIsVideoQueueFinished || mSeekedVideoData));
}

void
NextFrameSeekTask::CheckIfSeekComplete()
{
  AssertOwnerThread();

  const bool audioSeekComplete = IsAudioSeekComplete();

  const bool videoSeekComplete = IsVideoSeekComplete();
  if (HasVideo() && !videoSeekComplete) {
    // We haven't reached the target. Ensure we have requested another sample.
    if (NS_FAILED(EnsureVideoDecodeTaskQueued())) {
      DECODER_WARN("Failed to request video during seek");
      RejectIfExist(__func__);
    }
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

  // The MDSM::mDecodedAudioEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnAudioDecoded [%lld,%lld] disc=%d",
             (aAudioSample ? aAudioSample->mTime : -1),
             (aAudioSample ? aAudioSample->GetEndTime() : -1),
             (aAudioSample ? aAudioSample->mDiscontinuity : 0));

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  // We accept any audio data here.
  mSeekedAudioData = aAudioSample;

  CheckIfSeekComplete();
}

void
NextFrameSeekTask::OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  SAMPLE_LOG("OnAudioNotDecoded (aReason=%u)", aReason);

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  // We don't really handle audio deocde error here. Let MDSM to trigger further
  // audio decoding tasks if it needs to play audio, and MDSM will then receive
  // the decoding state from MediaDecoderReader.

  CheckIfSeekComplete();
}

void
NextFrameSeekTask::OnVideoDecoded(MediaData* aVideoSample)
{
  AssertOwnerThread();
  MOZ_ASSERT(aVideoSample);

  // The MDSM::mDecodedVideoEndTime will be updated once the whole SeekTask is
  // resolved.

  SAMPLE_LOG("OnVideoDecoded [%lld,%lld] disc=%d",
             (aVideoSample ? aVideoSample->mTime : -1),
             (aVideoSample ? aVideoSample->GetEndTime() : -1),
             (aVideoSample ? aVideoSample->mDiscontinuity : 0));

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  if (aVideoSample->mTime > mCurrentTimeBeforeSeek) {
    mSeekedVideoData = aVideoSample;
  }

  CheckIfSeekComplete();
}

void
NextFrameSeekTask::OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason)
{
  AssertOwnerThread();
  SAMPLE_LOG("OnVideoNotDecoded (aReason=%u)", aReason);

  if (!Exists()) {
    // We've received a sample from a previous decode. Discard it.
    return;
  }

  if (aReason == MediaDecoderReader::DECODE_ERROR) {
    if (mVideoQueue.GetSize() > 0) {
      // The video decoding request might be filed by MDSM not the
      // NextFrameSeekTask itself. So, the NextFrameSeekTask might has already
      // found its target in the VideoQueue but still waits the video decoding
      // request (which is filed by the MDSM) to be resolved. In this case, we
      // already have the target of this seek task, try to resolve this task.
      CheckIfSeekComplete();
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
    mIsVideoQueueFinished = true;
    CheckIfSeekComplete();
  }
}

void
NextFrameSeekTask::SetMediaDecoderReaderWrapperCallback()
{
  AssertOwnerThread();

  // Register dummy callbcak for audio decoding since we don't need to handle
  // the decoded audio samples.
  mAudioCallbackID =
    mReader->SetAudioCallback(this, &NextFrameSeekTask::OnAudioDecoded,
                                    &NextFrameSeekTask::OnAudioNotDecoded);

  mVideoCallbackID =
    mReader->SetVideoCallback(this, &NextFrameSeekTask::OnVideoDecoded,
                                    &NextFrameSeekTask::OnVideoNotDecoded);

  RefPtr<NextFrameSeekTask> self = this;
  mWaitAudioCallbackID =
    mReader->SetWaitAudioCallback(
      [self] (MediaData::Type aType) -> void {
        self->AssertOwnerThread();
        // We don't make an audio decode request here, instead, let MDSM to
        // trigger further audio decode tasks if MDSM itself needs to play audio.
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

  DECODER_LOG("NextFrameSeekTask set audio callbacks: mVideoCallbackID = %d\n", (int)mAudioCallbackID);
  DECODER_LOG("NextFrameSeekTask set video callbacks: mVideoCallbackID = %d\n", (int)mVideoCallbackID);
  DECODER_LOG("NextFrameSeekTask set wait audio callbacks: mWaitAudioCallbackID = %d\n", (int)mWaitAudioCallbackID);
  DECODER_LOG("NextFrameSeekTask set wait video callbacks: mWaitVideoCallbackID = %d\n", (int)mWaitVideoCallbackID);
}

void
NextFrameSeekTask::CancelMediaDecoderReaderWrapperCallback()
{
  AssertOwnerThread();

  DECODER_LOG("NextFrameSeekTask cancel audio callbacks: mAudioCallbackID = %d\n", (int)mAudioCallbackID);
  mReader->CancelAudioCallback(mAudioCallbackID);

  DECODER_LOG("NextFrameSeekTask cancel video callbacks: mVideoCallbackID = %d\n", (int)mVideoCallbackID);
  mReader->CancelVideoCallback(mVideoCallbackID);

  DECODER_LOG("NextFrameSeekTask cancel wait audio callbacks: mWaitAudioCallbackID = %d\n", (int)mWaitAudioCallbackID);
  mReader->CancelWaitAudioCallback(mWaitAudioCallbackID);

  DECODER_LOG("NextFrameSeekTask cancel wait video callbacks: mWaitVideoCallbackID = %d\n", (int)mWaitVideoCallbackID);
  mReader->CancelWaitVideoCallback(mWaitVideoCallbackID);
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
