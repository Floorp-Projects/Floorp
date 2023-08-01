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
  using SinkCreator = std::function<UniquePtr<AudioSink>()>;

 public:
  AudioSinkWrapper(AbstractThread* aOwnerThread,
                   MediaQueue<AudioData>& aAudioQueue, SinkCreator aFunc,
                   double aVolume, double aPlaybackRate, bool aPreservesPitch,
                   RefPtr<AudioDeviceInfo> aAudioDevice)
      : mOwnerThread(aOwnerThread),
        mAsyncInitTaskQueue(CreateAsyncInitTaskQueue()),
        mSinkCreator(std::move(aFunc)),
        mAudioDevice(std::move(aAudioDevice)),
        mParams(aVolume, aPlaybackRate, aPreservesPitch),
        mAudioQueue(aAudioQueue),
        mRetrySinkTime(TimeStamp::Now()) {
    MOZ_ASSERT(mAsyncInitTaskQueue);
  }

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
  RefPtr<GenericPromise> SetAudioDevice(
      RefPtr<AudioDeviceInfo> aDevice) override;

  double PlaybackRate() const override;

  nsresult Start(const media::TimeUnit& aStartTime,
                 const MediaInfo& aInfo) override;
  void Stop() override;
  bool IsStarted() const override;
  bool IsPlaying() const override;

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
  static already_AddRefed<TaskQueue> CreateAsyncInitTaskQueue();
  bool IsMuted() const;
  void OnMuted(bool aMuted);
  virtual ~AudioSinkWrapper();

  void AssertOwnerThread() const {
    MOZ_ASSERT(mOwnerThread->IsCurrentThreadIn());
  }

  bool NeedAudioSink();
  void StartAudioSink(UniquePtr<AudioSink> aAudioSink,
                      const media::TimeUnit& aStartTime);
  void ShutDownAudioSink();
  // Create and start mAudioSink.
  // An AudioSink can be started synchronously from the MDSM thread, or
  // asynchronously.
  // In synchronous mode, the clock doesn't advance until the sink has been
  // created, initialized and started. This is useful for the initial startup,
  // and when seeking.
  // In asynchronous mode, the clock will keep going forward (using the system
  // clock) until the AudioSink is started, at which point the clock will use
  // the AudioSink clock. This is used when unmuting a media element or
  // switching audio output devices. The promise is resolved when the
  // previous device is no longer in use and an attempt to open the new device
  // completes (successfully or not) or is deemed unnecessary because the
  // device is not required for output at this time.
  nsresult SyncCreateAudioSink(const media::TimeUnit& aStartTime);
  RefPtr<GenericPromise> MaybeAsyncCreateAudioSink(
      RefPtr<AudioDeviceInfo> aDevice);
  void ScheduleRetrySink();

  // Get the current media position using the system clock. This is used when
  // the audio is muted, or when the media has no audio track. Otherwise, the
  // media's position is based on the clock of the AudioStream.
  media::TimeUnit GetSystemClockPosition(TimeStamp aNow) const;
  bool CheckIfEnded() const;

  void OnAudioEnded(const EndedPromise::ResolveOrRejectValue& aValue);

  bool IsAudioSourceEnded(const MediaInfo& aInfo) const;

  const RefPtr<AbstractThread> mOwnerThread;
  const RefPtr<TaskQueue> mAsyncInitTaskQueue;
  SinkCreator mSinkCreator;
  UniquePtr<AudioSink> mAudioSink;
  // The output device this AudioSink is playing data to. The system's default
  // device is used if this is null.
  RefPtr<AudioDeviceInfo> mAudioDevice;
  // Will only exist when media has an audio track.
  RefPtr<EndedPromise> mEndedPromise;
  MozPromiseHolder<EndedPromise> mEndedPromiseHolder;
  // true between Start() and Stop()
  bool mIsStarted = false;
  PlaybackParams mParams;
  // mClockStartTime is null before Start(), after Stop(), and between
  // SetPlaying(false) and SetPlaying(true).  When the system time is used for
  // the clock, this is the time corresponding to mPositionAtClockStart.  When
  // an AudioStream is used for the clock, non-null values don't have specific
  // meaning beyond indicating that the clock is advancing.
  TimeStamp mClockStartTime;
  // The media position at the clock datum.  If the clock is not advancing,
  // then this is the media position from which to resume playback.  The value
  // is Invalid() before Start() to facilitate debug.
  media::TimeUnit mPositionAtClockStart = media::TimeUnit::Invalid();
  // End time of last packet played or dropped.
  // Only up-to-date when there is no AudioSink.
  media::TimeUnit mLastPacketEndTime;

  bool mAudioEnded = true;
  // mAudioSinkEndedRequest is connected when and only when mAudioSink is set
  // and not ended.
  MozPromiseRequestHolder<EndedPromise> mAudioSinkEndedRequest;
  MediaQueue<AudioData>& mAudioQueue;

  // Time when next to re-try AudioSink creation.
  // Set to a useful value only when another sink is needed.  At other times
  // it needs to be non-null for a comparison where the result will be
  // irrelevant.
  // This is checked in GetPosition() which is triggered periodically during
  // playback by MediaDecoderStateMachine::UpdatePlaybackPositionPeriodically()
  TimeStamp mRetrySinkTime;
  // Number of async AudioSink creation tasks in flight
  uint32_t mAsyncCreateCount = 0;
};

}  // namespace mozilla

#endif  // AudioSinkWrapper_h_
