/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoSink_h_
#define VideoSink_h_

#include "ImageContainer.h"
#include "MediaSink.h"
#include "mozilla/AbstractThread.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "VideoFrameContainer.h"

namespace mozilla {

class VideoFrameContainer;
template <class T> class MediaQueue;

namespace media {

class VideoSink : public MediaSink
{
public:
  VideoSink(AbstractThread* aThread,
            MediaSink* aAudioSink,
            MediaQueue<MediaData>& aVideoQueue,
            VideoFrameContainer* aContainer,
            bool aRealTime)
    : mOwnerThread(aThread)
    , mAudioSink(aAudioSink)
    , mVideoQueue(aVideoQueue)
    , mContainer(aContainer)
    , mRealTime(aRealTime)
    , mVideoFrameEndTime(-1)
  {}

  const PlaybackParams& GetPlaybackParams() const override;

  void SetPlaybackParams(const PlaybackParams& aParams) override;

  RefPtr<GenericPromise> OnEnded(TrackType aType) override;

  int64_t GetEndTime(TrackType aType) const override;

  int64_t GetPosition(TimeStamp* aTimeStamp = nullptr) const override;

  bool HasUnplayedFrames(TrackType aType) const override;

  void SetPlaybackRate(double aPlaybackRate) override;

  void SetPlaying(bool aPlaying) override;

  void Start(int64_t aStartTime, const MediaInfo& aInfo) override;

  void Stop() override;

  bool IsStarted() const override;

  bool IsPlaying() const override;

  void Shutdown() override;

private:
  virtual ~VideoSink();

  void AssertOwnerThread() const
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  MediaQueue<MediaData>& VideoQueue() const {
    return mVideoQueue;
  }

  void OnVideoEnded();

  const RefPtr<AbstractThread> mOwnerThread;
  RefPtr<MediaSink> mAudioSink;
  MediaQueue<MediaData>& mVideoQueue;
  VideoFrameContainer* mContainer;

  // True if we are decoding a real-time stream.
  const bool mRealTime;

  RefPtr<GenericPromise> mEndPromise;
  MozPromiseHolder<GenericPromise> mEndPromiseHolder;
  MozPromiseRequestHolder<GenericPromise> mVideoSinkEndRequest;

  // The presentation end time of the last video frame which has been displayed
  // in microseconds.
  int64_t mVideoFrameEndTime;
};

} // namespace media
} // namespace mozilla

#endif
