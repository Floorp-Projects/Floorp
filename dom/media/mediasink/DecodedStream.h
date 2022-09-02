/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecodedStream_h_
#define DecodedStream_h_

#include "AudibilityMonitor.h"
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
                nsMainThreadPtrHandle<SharedDummyTrack> aDummyTrack,
                CopyableTArray<RefPtr<ProcessedMediaTrack>> aOutputTracks,
                double aVolume, double aPlaybackRate, bool aPreservesPitch,
                MediaQueue<AudioData>& aAudioQueue,
                MediaQueue<VideoData>& aVideoQueue,
                RefPtr<AudioDeviceInfo> aAudioDevice);

  RefPtr<EndedPromise> OnEnded(TrackType aType) override;
  media::TimeUnit GetEndTime(TrackType aType) const override;
  media::TimeUnit GetPosition(TimeStamp* aTimeStamp = nullptr) override;
  bool HasUnplayedFrames(TrackType aType) const override {
    // TODO: bug 1755026
    return false;
  }

  media::TimeUnit UnplayedDuration(TrackType aType) const override {
    // TODO: bug 1755026
    return media::TimeUnit::Zero();
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
  const AudioDeviceInfo* AudioDevice() const override { return mAudioDevice; }

  MediaEventSource<bool>& AudibleEvent() { return mAudibleEvent; }

 protected:
  virtual ~DecodedStream();

 private:
  void DestroyData(UniquePtr<DecodedStreamData>&& aData);
  void SendAudio(const PrincipalHandle& aPrincipalHandle);
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

  // Give the audio that is going to be appended next as an input, if there is
  // a gap between audio's time and the frames that we've written, then return
  // a silence data that has same amount of frames and can be used to fill the
  // gap. If no gap exists, return nullptr.
  already_AddRefed<AudioData> CreateSilenceDataIfGapExists(
      RefPtr<AudioData>& aNextAudio);

  const RefPtr<AbstractThread> mOwnerThread;

  // Used to access the graph.
  const nsMainThreadPtrHandle<SharedDummyTrack> mDummyTrack;

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
  Maybe<AudibilityMonitor> mAudibilityMonitor;
  MediaEventProducer<bool> mAudibleEvent;

  MediaQueue<AudioData>& mAudioQueue;
  MediaQueue<VideoData>& mVideoQueue;

  // This is the audio device we were told to play out to.
  // All audio is captured, so nothing is actually played out -- but we report
  // this upwards as it could save us from being recreated when the sink
  // changes.
  const RefPtr<AudioDeviceInfo> mAudioDevice;

  MediaEventListener mAudioPushListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoFinishListener;
  MediaEventListener mOutputListener;
};

}  // namespace mozilla

#endif  // DecodedStream_h_
