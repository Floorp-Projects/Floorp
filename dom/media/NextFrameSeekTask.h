/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NEXTFRAME_SEEK_TASK_H
#define NEXTFRAME_SEEK_TASK_H

#include "SeekTask.h"
#include "MediaDecoderReader.h"

namespace mozilla {
namespace media {

/*
 * While invoking a NextFrameSeekTask, we don't know the seek target time, what
 * we know is the media's currant position. We use the media's currant position
 * to find out what the next frame is, by traversing through the video queue or
 * asking the decoder to decode more video frames. Once we confirm the next
 * frame, we then know the target time of the NextFrameSeekTask and we update it
 * so that the MDSM will be able to update the media element's position.
 */

class NextFrameSeekTask final : public SeekTask {
public:
  NextFrameSeekTask(const void* aDecoderID,
                   AbstractThread* aThread,
                   MediaDecoderReaderWrapper* aReader,
                   SeekJob&& aSeekJob,
                   const MediaInfo& aInfo,
                   const media::TimeUnit& aDuration,
                   int64_t aCurrentMediaTime,
                   MediaQueue<MediaData>& aAudioQueue,
                   MediaQueue<MediaData>& aVideoQueue);

  void Discard() override;

  RefPtr<SeekTaskPromise> Seek(const media::TimeUnit& aDuration) override;

  bool NeedToResetMDSM() const override;

private:
  ~NextFrameSeekTask();

  bool HasAudio() const;

  bool HasVideo() const;

  bool IsVideoDecoding() const;

  nsresult EnsureVideoDecodeTaskQueued();

  const char* VideoRequestStatus();

  void RequestVideoData();

  bool IsAudioSeekComplete();

  bool IsVideoSeekComplete();

  void CheckIfSeekComplete();

  void OnAudioDecoded(MediaData* aAudioSample);

  void OnAudioNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  void OnVideoDecoded(MediaData* aVideoSample);

  void OnVideoNotDecoded(MediaDecoderReader::NotDecodedReason aReason);

  void SetMediaDecoderReaderWrapperCallback();

  void CancelMediaDecoderReaderWrapperCallback();

  // Update the seek target's time before resolving this seek task, the updated
  // time will be used in the MDSM::SeekCompleted() to update the MDSM's position.
  void UpdateSeekTargetTime();

  /*
   * Data shared with MDSM.
   */
  MediaQueue<MediaData>& mAudioQueue;
  MediaQueue<MediaData>& mVideoQueue;

  /*
   * Internal state.
   */
  const int64_t mCurrentTimeBeforeSeek;
  const bool mHasAudio;
  const bool mHasVideo;
  media::TimeUnit mDuration;

  /*
   * Track the current seek promise made by the reader.
   */
  CallbackID mAudioCallbackID;
  CallbackID mVideoCallbackID;
  CallbackID mWaitAudioCallbackID;
  CallbackID mWaitVideoCallbackID;
};

} // namespace media
} // namespace mozilla

#endif /* NEXTFRAME_SEEK_TASK_H */
