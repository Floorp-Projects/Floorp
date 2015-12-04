/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef VideoSink_h_
#define VideoSink_h_

#include "FrameStatistics.h"
#include "ImageContainer.h"
#include "MediaEventSource.h"
#include "MediaSink.h"
#include "MediaTimer.h"
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
  typedef mozilla::layers::ImageContainer::ProducerID ProducerID;
public:
  VideoSink(AbstractThread* aThread,
            MediaSink* aAudioSink,
            MediaQueue<MediaData>& aVideoQueue,
            VideoFrameContainer* aContainer,
            bool aRealTime,
            FrameStatistics& aFrameStats,
            uint32_t aVQueueSentToCompositerSize);

  const PlaybackParams& GetPlaybackParams() const override;

  void SetPlaybackParams(const PlaybackParams& aParams) override;

  RefPtr<GenericPromise> OnEnded(TrackType aType) override;

  int64_t GetEndTime(TrackType aType) const override;

  int64_t GetPosition(TimeStamp* aTimeStamp = nullptr) const override;

  bool HasUnplayedFrames(TrackType aType) const override;

  void SetPlaybackRate(double aPlaybackRate) override;

  void SetVolume(double aVolume) override;

  void SetPreservesPitch(bool aPreservesPitch) override;

  void SetPlaying(bool aPlaying) override;

  void Redraw() override;

  void Start(int64_t aStartTime, const MediaInfo& aInfo) override;

  void Stop() override;

  bool IsStarted() const override;

  bool IsPlaying() const override;

  void Shutdown() override;

private:
  virtual ~VideoSink();

  // VideoQueue listener related.
  void OnVideoQueueEvent(RefPtr<MediaData>&& aSample);
  void ConnectListener();
  void DisconnectListener();

  // Sets VideoQueue images into the VideoFrameContainer. Called on the shared
  // state machine thread. The first aMaxFrames (at most) are set.
  // aClockTime and aClockTimeStamp are used as the baseline for deriving
  // timestamps for the frames; when omitted, aMaxFrames must be 1 and
  // a null timestamp is passed to the VideoFrameContainer.
  // If the VideoQueue is empty, this does nothing.
  void RenderVideoFrames(int32_t aMaxFrames, int64_t aClockTime = 0,
                         const TimeStamp& aClickTimeStamp = TimeStamp());

  // Triggered while videosink is started, videosink becomes "playing" status,
  // or VideoQueue event arrived.
  void TryUpdateRenderedVideoFrames();

  // If we have video, display a video frame if it's time for display has
  // arrived, otherwise sleep until it's time for the next frame. Update the
  // current frame time as appropriate, and trigger ready state update.
  // Called on the shared state machine thread.
  void UpdateRenderedVideoFrames();
  void UpdateRenderedVideoFramesByTimer();

  void AssertOwnerThread() const
  {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  MediaQueue<MediaData>& VideoQueue() const {
    return mVideoQueue;
  }

  const RefPtr<AbstractThread> mOwnerThread;
  RefPtr<MediaSink> mAudioSink;
  MediaQueue<MediaData>& mVideoQueue;
  VideoFrameContainer* mContainer;

  // Producer ID to help ImageContainer distinguish different streams of
  // FrameIDs. A unique and immutable value per VideoSink.
  const ProducerID mProducerID;

  // True if we are decoding a real-time stream.
  const bool mRealTime;

  // Used to notify MediaDecoder's frame statistics
  FrameStatistics& mFrameStats;

  RefPtr<GenericPromise> mEndPromise;
  MozPromiseHolder<GenericPromise> mEndPromiseHolder;
  MozPromiseRequestHolder<GenericPromise> mVideoSinkEndRequest;

  // The presentation end time of the last video frame which has been displayed
  // in microseconds.
  int64_t mVideoFrameEndTime;

  uint32_t mOldDroppedCount;

  // Event listeners for VideoQueue
  MediaEventListener mPushListener;

  // True if this sink is going to handle video track.
  bool mHasVideo;

  // Used to trigger another update of rendered frames in next round.
  DelayedScheduler mUpdateScheduler;

  // Max frame number sent to compositor at a time.
  // Based on the pref value obtained in MDSM.
  const uint32_t mVideoQueueSendToCompositorSize;
};

} // namespace media
} // namespace mozilla

#endif
