/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioSinkWrapper_h_
#define AudioSinkWrapper_h_

#include "mozilla/AbstractThread.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "AudioSink.h"
#include "MediaSink.h"

namespace mozilla {
class MediaData;
template <class T>
class MediaQueue;

/**
 * A wrapper around AudioSink to provide the interface of MediaSink.
 */
class AudioSinkWrapper : public MediaSink {
  using PlaybackParams = AudioSink::PlaybackParams;

  // An AudioSink factory.
  class Creator {
   public:
    virtual ~Creator() = default;
    virtual AudioSink* Create(const media::TimeUnit& aStartTime) = 0;
  };

  // Wrap around a function object which creates AudioSinks.
  template <typename Function>
  class CreatorImpl : public Creator {
   public:
    explicit CreatorImpl(const Function& aFunc) : mFunction(aFunc) {}
    AudioSink* Create(const media::TimeUnit& aStartTime) override {
      return mFunction(aStartTime);
    }

   private:
    Function mFunction;
  };

 public:
  template <typename Function>
  AudioSinkWrapper(AbstractThread* aOwnerThread,
                   const MediaQueue<AudioData>& aAudioQueue,
                   const Function& aFunc, double aVolume, double aPlaybackRate,
                   bool aPreservesPitch)
      : mOwnerThread(aOwnerThread),
        mCreator(new CreatorImpl<Function>(aFunc)),
        mIsStarted(false),
        mParams(aVolume, aPlaybackRate, aPreservesPitch),
        // Give an invalid value to facilitate debug if used before playback
        // starts.
        mPlayDuration(media::TimeUnit::Invalid()),
        mAudioEnded(true),
        mAudioQueue(aAudioQueue) {}

  RefPtr<EndedPromise> OnEnded(TrackType aType) override;
  media::TimeUnit GetEndTime(TrackType aType) const override;
  media::TimeUnit GetPosition(TimeStamp* aTimeStamp = nullptr) const override;
  bool HasUnplayedFrames(TrackType aType) const override;

  void SetVolume(double aVolume) override;
  void SetStreamName(const nsAString& aStreamName) override;
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

 private:
  virtual ~AudioSinkWrapper();

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  media::TimeUnit GetVideoPosition(TimeStamp aNow) const;

  void OnAudioEnded();

  bool IsAudioSourceEnded(const MediaInfo& aInfo) const;

  const RefPtr<AbstractThread> mOwnerThread;
  UniquePtr<Creator> mCreator;
  UniquePtr<AudioSink> mAudioSink;
  // Will only exist when media has an audio track.
  RefPtr<EndedPromise> mEndedPromise;

  bool mIsStarted;
  PlaybackParams mParams;

  TimeStamp mPlayStartTime;
  media::TimeUnit mPlayDuration;

  bool mAudioEnded;
  MozPromiseRequestHolder<EndedPromise> mAudioSinkEndedPromise;
  const MediaQueue<AudioData>& mAudioQueue;
};

}  // namespace mozilla

#endif  // AudioSinkWrapper_h_
