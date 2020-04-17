/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "MediaEventSource.h"
#include "MediaInfo.h"
#include "MediaSegment.h"
#include "MediaSink.h"

#include "mozilla/AbstractThread.h"
#include "mozilla/Maybe.h"
#include "mozilla/MozPromise.h"
#include "mozilla/RefPtr.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class DecodedStreamData;
class MediaDecoderStateMachine;
class AudioData;
class VideoData;
struct PlaybackInfoInit;
class ProcessedMediaTrack;
class TimeStamp;

template <class T>
class MediaQueue;

class DecodedStream : public MediaSink {
 public:
  DecodedStream(MediaDecoderStateMachine* aStateMachine,
                nsTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks,
                double aVolume, double aPlaybackRate, bool aPreservesPitch,
                MediaQueue<AudioData>& aAudioQueue,
                MediaQueue<VideoData>& aVideoQueue);

  RefPtr<EndedPromise> OnEnded(TrackType aType) override;
  media::TimeUnit GetEndTime(TrackType aType) const override;
  media::TimeUnit GetPosition(TimeStamp* aTimeStamp = nullptr) const override;
  bool HasUnplayedFrames(TrackType aType) const override {
    // TODO: implement this.
    return false;
  }

  void SetVolume(double aVolume) override;
  void SetPlaybackRate(double aPlaybackRate) override;
  void SetPreservesPitch(bool aPreservesPitch) override;
  void SetPlaying(bool aPlaying) override;

  double PlaybackRate() const override;

  nsresult Start(const media::TimeUnit& aStartTime,
                 const MediaInfo& aInfo) override;
  void Stop() override;
  bool IsStarted() const override;
  bool IsPlaying() const override;
  void Shutdown() override;
  void GetDebugInfo(dom::MediaSinkDebugInfo& aInfo) override;

  MediaEventSource<bool>& AudibleEvent() { return mAudibleEvent; }

 protected:
  virtual ~DecodedStream();

 private:
  void DestroyData(UniquePtr<DecodedStreamData>&& aData);
  void SendAudio(double aVolume, const PrincipalHandle& aPrincipalHandle);
  void SendVideo(const PrincipalHandle& aPrincipalHandle);
  void ResetAudio();
  void ResetVideo(const PrincipalHandle& aPrincipalHandle);
  void SendData();
  void NotifyOutput(int64_t aTime);
  void CheckIsDataAudible(const AudioData* aData);

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  void PlayingChanged();

  void ConnectListener();
  void DisconnectListener();

  const RefPtr<AbstractThread> mOwnerThread;

  /*
   * Worker thread only members.
   */
  WatchManager<DecodedStream> mWatchManager;
  UniquePtr<DecodedStreamData> mData;
  RefPtr<EndedPromise> mAudioEndedPromise;
  RefPtr<EndedPromise> mVideoEndedPromise;

  Watchable<bool> mPlaying;
  Mirror<PrincipalHandle> mPrincipalHandle;
  AbstractCanonical<PrincipalHandle>* mCanonicalOutputPrincipal;
  const nsTArray<RefPtr<ProcessedMediaTrack>> mOutputTracks;

  double mVolume;
  double mPlaybackRate;
  bool mPreservesPitch;

  media::NullableTimeUnit mStartTime;
  media::TimeUnit mLastOutputTime;
  MediaInfo mInfo;
  // True when stream is producing audible sound, false when stream is silent.
  bool mIsAudioDataAudible = false;
  MediaEventProducer<bool> mAudibleEvent;

  MediaQueue<AudioData>& mAudioQueue;
  MediaQueue<VideoData>& mVideoQueue;

  MediaEventListener mAudioPushListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;
  MediaEventListener mOutputListener;
};

}  // namespace mozilla

#endif  // DecodedStream_h_
