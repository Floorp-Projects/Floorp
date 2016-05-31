/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ACCURATE_SEEK_TASK_H
#define ACCURATE_SEEK_TASK_H

#include "SeekTask.h"
#include "MediaCallbackID.h"
#include "MediaDecoderReader.h"
#include "SeekJob.h"

namespace mozilla {

class AccurateSeekTask final : public SeekTask {
public:
  AccurateSeekTask(const void* aDecoderID,
                   AbstractThread* aThread,
                   MediaDecoderReaderWrapper* aReader,
                   SeekJob&& aSeekJob,
                   const MediaInfo& aInfo,
                   const media::TimeUnit& aDuration,
                   int64_t aCurrentMediaTime);

  void Discard() override;

  RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration) override;

  bool NeedToResetMDSM() const override;

private:
  ~AccurateSeekTask();

  bool HasAudio() const;

  bool HasVideo() const;

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

  void OnSeekResolved(media::TimeUnit);

  void OnSeekRejected(nsresult aResult);

  void OnAudioDecoded(MediaData* aAudioSample);

  void OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  void OnVideoDecoded(MediaData* aVideoSample);

  void OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  void SetMediaDecoderReaderWrapperCallback();

  void CancelMediaDecoderReaderWrapperCallback();

  /*
   * Internal state.
   */
  const int64_t mCurrentTimeBeforeSeek;
  const uint32_t mAudioRate;  // Audio sample rate.
  const bool mHasAudio;
  const bool mHasVideo;
  bool mDropAudioUntilNextDiscontinuity;
  bool mDropVideoUntilNextDiscontinuity;

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
  CallbackID mWaitAudioCallbackID;
  CallbackID mWaitVideoCallbackID;
};

} // namespace mozilla

#endif /* ACCURATE_SEEK_TASK_H */
