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
    virtual AudioSink* Create() = 0;
  };

  // Wrap around a function object which creates AudioSinks.
  template <typename Function>
  class CreatorImpl : public Creator {
   public:
    explicit CreatorImpl(const Function& aFunc) : mFunction(aFunc) {}
    AudioSink* Create() override { return mFunction(); }

   private:
    Function mFunction;
  };

 public:
  template <typename Function>
  AudioSinkWrapper(AbstractThread* aOwnerThread,
                   MediaQueue<AudioData>& aAudioQueue, const Function& aFunc,
                   double aVolume, double aPlaybackRate, bool aPreservesPitch,
                   RefPtr<AudioDeviceInfo> aAudioDevice)
      : mOwnerThread(aOwnerThread),
        mCreator(new CreatorImpl<Function>(aFunc)),
        mAudioDevice(std::move(aAudioDevice)),
        mIsStarted(false),
        mParams(aVolume, aPlaybackRate, aPreservesPitch),
        // Give an invalid value to facilitate debug if used before playback
        // starts.
        mPlayDuration(media::TimeUnit::Invalid()),
        mAudioEnded(true),
        mAudioQueue(aAudioQueue) {}

  RefPtr<EndedPromise> OnEnded(TrackType aType) override;
  media::TimeUnit GetEndTime(TrackType aType) const override;
  media::TimeUnit GetPosition(TimeStamp* aTimeStamp = nullptr) override;
  bool HasUnplayedFrames(TrackType aType) const override;
  media::TimeUnit UnplayedDuration(TrackType aType) const override;
  void DropAudioPacketsIfNeeded(const media::TimeUnit& aMediaPosition);

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

  const AudioDeviceInfo* AudioDevice() const override { return mAudioDevice; }

  void Shutdown() override;

  void GetDebugInfo(dom::MediaSinkDebugInfo& aInfo) override;

 private:
  // The clock that was in use for the previous position query, allowing to
  // detect clock switches.
  enum class ClockSource {
    // The clock comes from an underlying system-level audio stream.
    AudioStream,
    // The clock comes from the system clock.
    SystemClock,
    // The stream is paused, a constant time is reported.
    Paused
  } mLastClockSource = ClockSource::Paused;
  bool IsMuted() const;
  void OnMuted(bool aMuted);
  virtual ~AudioSinkWrapper();

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  // An AudioSink can be started synchronously from the MDSM thread, or
  // asynchronously.
  // In synchronous mode, the clock doesn't advance until the sink has been
  // created, initialized and started. This is useful for the initial startup,
  // and when seeking.
  // In asynchronous mode, the clock will keep going forward (using the system
  // clock) until the AudioSink is started, at which point the clock will use
  // the AudioSink clock. This is used when unmuting a media element.
  enum class AudioSinkStartPolicy { SYNC, ASYNC };
  nsresult StartAudioSink(const media::TimeUnit& aStartTime,
                          AudioSinkStartPolicy aPolicy);

  // Get the current media position using the system clock. This is used when
  // the audio is muted, or when the media has no audio track. Otherwise, the
  // media's position is based on the clock of the AudioStream.
  media::TimeUnit GetSystemClockPosition(TimeStamp aNow) const;
  bool CheckIfEnded() const;

  void OnAudioEnded();

  bool IsAudioSourceEnded(const MediaInfo& aInfo) const;

  const RefPtr<AbstractThread> mOwnerThread;
  UniquePtr<Creator> mCreator;
  UniquePtr<AudioSink> mAudioSink;
  // The output device this AudioSink is playing data to. The system's default
  // device is used if this is null.
  const RefPtr<AudioDeviceInfo> mAudioDevice;
  // Will only exist when media has an audio track.
  RefPtr<EndedPromise> mEndedPromise;
  MozPromiseHolder<EndedPromise> mEndedPromiseHolder;

  bool mIsStarted;
  PlaybackParams mParams;

  TimeStamp mPlayStartTime;
  media::TimeUnit mPlayDuration;

  bool mAudioEnded;
  MozPromiseRequestHolder<EndedPromise> mAudioSinkEndedPromise;
  MediaQueue<AudioData>& mAudioQueue;
};

}  // namespace mozilla

#endif  // AudioSinkWrapper_h_
