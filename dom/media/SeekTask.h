/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SEEK_TASK_H
#define SEEK_TASK_H

#include "mozilla/MozPromise.h"
#include "MediaCallbackID.h"
#include "MediaDecoderReader.h"
#include "SeekJob.h"

namespace mozilla {

class AbstractThread;
class MediaData;
class MediaDecoderReaderWrapper;

struct SeekTaskResolveValue
{
  RefPtr<MediaData> mSeekedAudioData;
  RefPtr<MediaData> mSeekedVideoData;
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
  bool mNeedToStopPrerollingAudio;
  bool mNeedToStopPrerollingVideo;
};

struct SeekTaskRejectValue
{
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
  bool mNeedToStopPrerollingAudio;
  bool mNeedToStopPrerollingVideo;
};

class SeekTask {

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SeekTask)

public:
  static const bool IsExclusive = true;

  using SeekTaskPromise =
    MozPromise<SeekTaskResolveValue, SeekTaskRejectValue, IsExclusive>;

  static already_AddRefed<SeekTask>
  CreateSeekTask(const void* aDecoderID,
                 AbstractThread* aThread,
                 MediaDecoderReaderWrapper* aReader,
                 SeekJob&& aSeekJob,
                 const MediaInfo& aInfo,
                 const media::TimeUnit& aDuration,
                 int64_t aCurrentMediaTime);

  virtual void Discard();

  virtual RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration);

  virtual bool NeedToResetMDSM() const;

  SeekJob& GetSeekJob();

  bool Exists() const;

protected:
  SeekTask(const void* aDecoderID,
           AbstractThread* aThread,
           MediaDecoderReaderWrapper* aReader,
           SeekJob&& aSeekJob,
           const MediaInfo& aInfo,
           const media::TimeUnit& aDuration,
           int64_t aCurrentMediaTime);

  virtual ~SeekTask();

  void Resolve(const char* aCallSite);

  void RejectIfExist(const char* aCallSite);

  void AssertOwnerThread() const;

  bool HasAudio() const;

  bool HasVideo() const;

  TaskQueue* DecodeTaskQueue() const;

  AbstractThread* OwnerThread() const;

  bool IsVideoDecoding() const;

  bool IsAudioDecoding() const;

  nsresult EnsureVideoDecodeTaskQueued();

  nsresult EnsureAudioDecodeTaskQueued();

  const char* AudioRequestStatus();

  const char* VideoRequestStatus();

  void RequestVideoData();

  void RequestAudioData();

  nsresult DropAudioUpToSeekTarget(MediaData* aSample);

  nsresult DropVideoUpToSeekTarget(MediaData* aSample);

  bool IsAudioSeekComplete();

  bool IsVideoSeekComplete();

  void CheckIfSeekComplete();

  virtual void OnSeekResolved(media::TimeUnit);

  virtual void OnSeekRejected(nsresult aResult);

  virtual void OnAudioDecoded(MediaData* aAudioSample);

  virtual void OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  virtual void OnVideoDecoded(MediaData* aVideoSample);

  virtual void OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  void SetMediaDecoderReaderWrapperCallback();

  void CancelMediaDecoderReaderWrapperCallback();

  /*
   * Data shared with MDSM.
   */
  const void* mDecoderID; // For logging.
  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<MediaDecoderReaderWrapper> mReader;

  /*
   * Internal state.
   */
  SeekJob mSeekJob;
  MozPromiseHolder<SeekTaskPromise> mSeekTaskPromise;
  int64_t mCurrentTimeBeforeSeek;
  const uint32_t mAudioRate;  // Audio sample rate.
  const bool mHasAudio;
  const bool mHasVideo;
  bool mDropAudioUntilNextDiscontinuity;
  bool mDropVideoUntilNextDiscontinuity;
  bool mIsDiscarded;

  // This temporarily stores the first frame we decode after we seek.
  // This is so that if we hit end of stream while we're decoding to reach
  // the seek target, we will still have a frame that we can display as the
  // last frame in the media.
  RefPtr<MediaData> mFirstVideoFrameAfterSeek;

  /*
   * Track the current seek promise made by the reader.
   */
  MozPromiseRequestHolder<MediaDecoderReader::SeekPromise> mSeekRequest;
  CallbackID mAudioCallbackID;
  CallbackID mVideoCallbackID;
  MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise> mAudioWaitRequest;
  MozPromiseRequestHolder<MediaDecoderReader::WaitForDataPromise> mVideoWaitRequest;

  /*
   * Information which are going to be returned to MDSM.
   */
  RefPtr<MediaData> mSeekedAudioData;
  RefPtr<MediaData> mSeekedVideoData;
  bool mIsAudioQueueFinished;
  bool mIsVideoQueueFinished;
  bool mNeedToStopPrerollingAudio;
  bool mNeedToStopPrerollingVideo;
};

} // namespace mozilla

#endif /* SEEK_TASK_H */
