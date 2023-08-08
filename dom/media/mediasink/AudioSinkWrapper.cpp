/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSinkWrapper.h"
#include "AudioDeviceInfo.h"
#include "AudioSink.h"
#include "VideoUtils.h"
#include "mozilla/Logging.h"
#include "mozilla/Result.h"
#include "mozilla/StaticPrefs_media.h"
#include "nsPrintfCString.h"
#include "nsThreadManager.h"

mozilla::LazyLogModule gAudioSinkWrapperLog("AudioSinkWrapper");
#define LOG(...) \
  MOZ_LOG(gAudioSinkWrapperLog, mozilla::LogLevel::Debug, (__VA_ARGS__));
#define LOGV(...) \
  MOZ_LOG(gAudioSinkWrapperLog, mozilla::LogLevel::Verbose, (__VA_ARGS__));

namespace mozilla {

using media::TimeUnit;

AudioSinkWrapper::~AudioSinkWrapper() = default;

void AudioSinkWrapper::Shutdown() {
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "Must be called after playback stopped.");
  mSinkCreator = nullptr;
}

/* static */
already_AddRefed<TaskQueue> AudioSinkWrapper::CreateAsyncInitTaskQueue() {
  return nsThreadManager::get().CreateBackgroundTaskQueue("AsyncAudioSinkInit");
}

RefPtr<MediaSink::EndedPromise> AudioSinkWrapper::OnEnded(TrackType aType) {
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");
  if (aType == TrackInfo::kAudioTrack) {
    return mEndedPromise;
  }
  return nullptr;
}

TimeUnit AudioSinkWrapper::GetEndTime(TrackType aType) const {
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");
  if (aType != TrackInfo::kAudioTrack) {
    return TimeUnit::Zero();
  }

  if (mAudioSink && mAudioSink->AudioStreamCallbackStarted()) {
    auto time = mAudioSink->GetEndTime();
    LOGV("%p: GetEndTime return %lf from sink", this, time.ToSeconds());
    return time;
  }

  RefPtr<const AudioData> audio = mAudioQueue.PeekBack();
  if (audio) {
    LOGV("%p: GetEndTime return %lf from queue", this,
         audio->GetEndTime().ToSeconds());
    return audio->GetEndTime();
  }

  LOGV("%p: GetEndTime return %lf from last packet", this,
       mLastPacketEndTime.ToSeconds());
  return mLastPacketEndTime;
}

TimeUnit AudioSinkWrapper::GetSystemClockPosition(TimeStamp aNow) const {
  AssertOwnerThread();
  MOZ_ASSERT(!mClockStartTime.IsNull());
  // Time elapsed since we started playing.
  double delta = (aNow - mClockStartTime).ToSeconds();
  // Take playback rate into account.
  return mPositionAtClockStart +
         TimeUnit::FromSeconds(delta * mParams.mPlaybackRate);
}

bool AudioSinkWrapper::IsMuted() const {
  AssertOwnerThread();
  return mParams.mVolume == 0.0;
}

TimeUnit AudioSinkWrapper::GetPosition(TimeStamp* aTimeStamp) {
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");

  TimeUnit pos;
  TimeStamp t = TimeStamp::Now();

  if (mAudioSink) {
    if (mLastClockSource == ClockSource::SystemClock) {
      TimeUnit switchTime = GetSystemClockPosition(t);
      // Update the _actual_ start time of the audio stream now that it has
      // started, preventing any clock discontinuity.
      mAudioSink->UpdateStartTime(switchTime);
      LOGV("%p: switching to audio clock at media time %lf", this,
           switchTime.ToSeconds());
    }
    // Rely on the audio sink to report playback position when it is not ended.
    pos = mAudioSink->GetPosition();
    LOGV("%p: Getting position from the Audio Sink %lf", this, pos.ToSeconds());
    mLastClockSource = ClockSource::AudioStream;
  } else if (!mClockStartTime.IsNull()) {
    // Calculate playback position using system clock if we are still playing,
    // but not rendering the audio, because this audio sink is muted.
    pos = GetSystemClockPosition(t);
    LOGV("%p: Getting position from the system clock %lf", this,
         pos.ToSeconds());
    if (mAudioQueue.GetSize() > 0) {
      // Audio track, but it won't be dequeued.  Discard packets
      // that are behind the current media time, to keep the queue size under
      // control.
      DropAudioPacketsIfNeeded(pos);
    }
    // Without an AudioSink, it's necessary to manually check if the audio has
    // "ended", meaning that all the audio packets have been consumed,
    // to resolve the ended promise.
    if (CheckIfEnded()) {
      MOZ_ASSERT(!mAudioSink);
      mEndedPromiseHolder.ResolveIfExists(true, __func__);
    }
    mLastClockSource = ClockSource::SystemClock;

    if (!mAudioSink && mAsyncCreateCount == 0 && NeedAudioSink() &&
        t > mRetrySinkTime) {
      MaybeAsyncCreateAudioSink(mAudioDevice);
    }
  } else {
    // Return how long we've played if we are not playing.
    pos = mPositionAtClockStart;
    LOGV("%p: Getting static position, not playing %lf", this, pos.ToSeconds());
    mLastClockSource = ClockSource::Paused;
  }

  if (aTimeStamp) {
    *aTimeStamp = t;
  }

  return pos;
}

bool AudioSinkWrapper::CheckIfEnded() const {
  return mAudioQueue.IsFinished() && mAudioQueue.GetSize() == 0u;
}

bool AudioSinkWrapper::HasUnplayedFrames(TrackType aType) const {
  AssertOwnerThread();
  return mAudioSink ? mAudioSink->HasUnplayedFrames() : false;
}

media::TimeUnit AudioSinkWrapper::UnplayedDuration(TrackType aType) const {
  AssertOwnerThread();
  return mAudioSink ? mAudioSink->UnplayedDuration() : media::TimeUnit::Zero();
}

void AudioSinkWrapper::DropAudioPacketsIfNeeded(
    const TimeUnit& aMediaPosition) {
  RefPtr<AudioData> audio = mAudioQueue.PeekFront();
  uint32_t dropped = 0;
  while (audio && audio->GetEndTime() < aMediaPosition) {
    // drop this packet, try the next one
    audio = mAudioQueue.PopFront();
    dropped++;
    if (audio) {
      mLastPacketEndTime = audio->GetEndTime();
      LOGV(
          "Dropping audio packets: media position: %lf, "
          "packet dropped: [%lf, %lf] (%u so far).\n",
          aMediaPosition.ToSeconds(), audio->mTime.ToSeconds(),
          (audio->GetEndTime()).ToSeconds(), dropped);
    }
    audio = mAudioQueue.PeekFront();
  }
}

void AudioSinkWrapper::OnMuted(bool aMuted) {
  AssertOwnerThread();
  LOG("%p: AudioSinkWrapper::OnMuted(%s)", this, aMuted ? "true" : "false");
  // Nothing to do
  if (mAudioEnded) {
    LOG("%p: AudioSinkWrapper::OnMuted, but no audio track", this);
    return;
  }
  if (aMuted) {
    if (mAudioSink) {
      LOG("AudioSinkWrapper muted, shutting down AudioStream.");
      ShutDownAudioSink();
    }
  } else {
    LOG("%p: AudioSinkWrapper unmuted, maybe re-creating an AudioStream.",
        this);
    MaybeAsyncCreateAudioSink(mAudioDevice);
  }
}

void AudioSinkWrapper::SetVolume(double aVolume) {
  AssertOwnerThread();

  bool wasMuted = mParams.mVolume == 0;
  bool nowMuted = aVolume == 0.;
  mParams.mVolume = aVolume;

  if (!wasMuted && nowMuted) {
    OnMuted(true);
  } else if (wasMuted && !nowMuted) {
    OnMuted(false);
  }

  if (mAudioSink) {
    mAudioSink->SetVolume(aVolume);
  }
}

void AudioSinkWrapper::SetStreamName(const nsAString& aStreamName) {
  AssertOwnerThread();
  if (mAudioSink) {
    mAudioSink->SetStreamName(aStreamName);
  }
}

void AudioSinkWrapper::SetPlaybackRate(double aPlaybackRate) {
  AssertOwnerThread();
  if (mAudioSink) {
    // Pass the playback rate to the audio sink. The underlying AudioStream
    // will handle playback rate changes and report correct audio position.
    mAudioSink->SetPlaybackRate(aPlaybackRate);
  } else if (!mClockStartTime.IsNull()) {
    // Adjust playback duration and start time when we are still playing.
    TimeStamp now = TimeStamp::Now();
    mPositionAtClockStart = GetSystemClockPosition(now);
    mClockStartTime = now;
  }
  // mParams.mPlaybackRate affects GetSystemClockPosition(). It should be
  // updated after the calls to GetSystemClockPosition();
  mParams.mPlaybackRate = aPlaybackRate;

  // Do nothing when not playing. Changes in playback rate will be taken into
  // account by GetSystemClockPosition().
}

void AudioSinkWrapper::SetPreservesPitch(bool aPreservesPitch) {
  AssertOwnerThread();
  mParams.mPreservesPitch = aPreservesPitch;
  if (mAudioSink) {
    mAudioSink->SetPreservesPitch(aPreservesPitch);
  }
}

void AudioSinkWrapper::SetPlaying(bool aPlaying) {
  AssertOwnerThread();
  LOG("%p: AudioSinkWrapper::SetPlaying %s", this, aPlaying ? "true" : "false");

  // Resume/pause matters only when playback started.
  if (!mIsStarted) {
    return;
  }

  if (mAudioSink) {
    mAudioSink->SetPlaying(aPlaying);
  }

  if (aPlaying) {
    MOZ_ASSERT(mClockStartTime.IsNull());
    TimeUnit switchTime = GetPosition();
    mClockStartTime = TimeStamp::Now();
    if (!mAudioSink && NeedAudioSink()) {
      LOG("%p: AudioSinkWrapper::SetPlaying : starting an AudioSink", this);
      DropAudioPacketsIfNeeded(switchTime);
      SyncCreateAudioSink(switchTime);
    }
  } else {
    // Remember how long we've played.
    mPositionAtClockStart = GetPosition();
    // mClockStartTime must be updated later since GetPosition()
    // depends on the value of mClockStartTime.
    mClockStartTime = TimeStamp();
  }
}

RefPtr<GenericPromise> AudioSinkWrapper::SetAudioDevice(
    RefPtr<AudioDeviceInfo> aDevice) {
  return MaybeAsyncCreateAudioSink(std::move(aDevice));
}

double AudioSinkWrapper::PlaybackRate() const {
  AssertOwnerThread();
  return mParams.mPlaybackRate;
}

nsresult AudioSinkWrapper::Start(const TimeUnit& aStartTime,
                                 const MediaInfo& aInfo) {
  LOG("%p AudioSinkWrapper::Start", this);
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "playback already started.");

  mIsStarted = true;
  mPositionAtClockStart = aStartTime;
  mClockStartTime = TimeStamp::Now();
  mAudioEnded = IsAudioSourceEnded(aInfo);
  mLastPacketEndTime = TimeUnit::Zero();

  if (mAudioEnded) {
    // Resolve promise if we start playback at the end position of the audio.
    mEndedPromise =
        aInfo.HasAudio()
            ? MediaSink::EndedPromise::CreateAndResolve(true, __func__)
            : nullptr;
    return NS_OK;
  }

  mEndedPromise = mEndedPromiseHolder.Ensure(__func__);
  if (!NeedAudioSink()) {
    return NS_OK;
  }
  return SyncCreateAudioSink(aStartTime);
}

bool AudioSinkWrapper::NeedAudioSink() {
  // An AudioSink is needed if unmuted, playing, and not ended.  The not-ended
  // check also avoids creating an AudioSink when there is no audio track.
  return !IsMuted() && IsPlaying() && !mEndedPromiseHolder.IsEmpty();
}

void AudioSinkWrapper::StartAudioSink(UniquePtr<AudioSink> aAudioSink,
                                      const TimeUnit& aStartTime) {
  AssertOwnerThread();
  MOZ_ASSERT(!mAudioSink);
  mAudioSink = std::move(aAudioSink);
  mAudioSink->Start(aStartTime)
      ->Then(mOwnerThread.get(), __func__, this,
             &AudioSinkWrapper::OnAudioEnded)
      ->Track(mAudioSinkEndedRequest);
}

void AudioSinkWrapper::ShutDownAudioSink() {
  AssertOwnerThread();
  mAudioSinkEndedRequest.DisconnectIfExists();
  if (IsPlaying()) {
    mPositionAtClockStart = mAudioSink->GetPosition();
    mClockStartTime = TimeStamp::Now();
  }
  mAudioSink->ShutDown();
  mLastPacketEndTime = mAudioSink->GetEndTime();
  mAudioSink = nullptr;
}

RefPtr<GenericPromise> AudioSinkWrapper::MaybeAsyncCreateAudioSink(
    RefPtr<AudioDeviceInfo> aDevice) {
  AssertOwnerThread();
  UniquePtr<AudioSink> audioSink;
  if (NeedAudioSink() && (!mAudioSink || aDevice != mAudioDevice)) {
    LOG("%p: AudioSinkWrapper::MaybeAsyncCreateAudioSink: AudioSink needed",
        this);
    if (mAudioSink) {
      ShutDownAudioSink();
    }
    audioSink = mSinkCreator();
  } else {
    LOG("%p: AudioSinkWrapper::MaybeAsyncCreateAudioSink: no AudioSink change",
        this);
    // Bounce off the background thread to keep promise resolution in order.
  }
  mAudioDevice = std::move(aDevice);
  ++mAsyncCreateCount;
  using Promise =
      MozPromise<UniquePtr<AudioSink>, nsresult, /* IsExclusive = */ true>;
  return InvokeAsync(mAsyncInitTaskQueue,
                     "MaybeAsyncCreateAudioSink (Async part: initialization)",
                     [self = RefPtr<AudioSinkWrapper>(this),
                      audioSink{std::move(audioSink)},
                      audioDevice = mAudioDevice, this]() mutable {
                       if (!audioSink || !mAsyncInitTaskQueue->IsEmpty()) {
                         // Either an AudioSink is not required or there's a
                         // pending task to init an AudioSink with a possibly
                         // different device.
                         return Promise::CreateAndResolve(nullptr, __func__);
                       }

                       LOG("AudioSink initialization on background thread");
                       // This can take about 200ms, e.g. on Windows, we don't
                       // want to do it on the MDSM thread, because it would
                       // make the clock not update for that amount of time, and
                       // the video would therefore not update. The Start() call
                       // is very cheap on the other hand, we can do it from the
                       // MDSM thread.
                       nsresult rv = audioSink->InitializeAudioStream(
                           mParams, audioDevice,
                           AudioSink::InitializationType::UNMUTING);
                       if (NS_FAILED(rv)) {
                         LOG("Async AudioSink initialization failed");
                         return Promise::CreateAndReject(rv, __func__);
                       }
                       return Promise::CreateAndResolve(std::move(audioSink),
                                                        __func__);
                     })
      ->Then(
          mOwnerThread,
          "MaybeAsyncCreateAudioSink (Async part: start from MDSM thread)",
          [self = RefPtr<AudioSinkWrapper>(this), audioDevice = mAudioDevice,
           this](Promise::ResolveOrRejectValue&& aValue) mutable {
            LOG("AudioSink async init done, back on MDSM thread");
            --mAsyncCreateCount;
            UniquePtr<AudioSink> audioSink;
            if (aValue.IsResolve()) {
              audioSink = std::move(aValue.ResolveValue());
            }
            // It's possible that the newly created AudioSink isn't needed at
            // this point, in some cases:
            // 1. An AudioSink was created synchronously while this
            // AudioSink was initialized asynchronously, bail out here. This
            // happens when seeking (which does a synchronous initialization)
            // right after unmuting.  mEndedPromiseHolder is managed by the
            // other AudioSink, so don't touch it here.
            // 2. The media element was muted while the async initialization
            // was happening.
            // 3. The AudioSinkWrapper was paused or stopped during
            // asynchronous initialization.
            // 4. The audio has ended during asynchronous initialization.
            // 5. A change to a potentially different sink device is pending.
            if (mAudioSink || !NeedAudioSink() || audioDevice != mAudioDevice) {
              LOG("AudioSink async initialization isn't needed.");
              if (audioSink) {
                LOG("Shutting down unneeded AudioSink.");
                audioSink->ShutDown();
              }
              return GenericPromise::CreateAndResolve(true, __func__);
            }

            if (aValue.IsReject()) {
              if (audioDevice) {
                // Device will be started when available again.
                ScheduleRetrySink();
              } else {
                // Default device not available.  Report error.
                MOZ_ASSERT(!mAudioSink);
                mEndedPromiseHolder.RejectIfExists(aValue.RejectValue(),
                                                   __func__);
              }
              return GenericPromise::CreateAndResolve(true, __func__);
            }

            if (!audioSink) {
              // No-op because either an existing AudioSink was suitable or no
              // AudioSink was needed when MaybeAsyncCreateAudioSink() set up
              // this task.  We now need a new AudioSink, but that will be
              // handled by another task, either already pending or a delayed
              // retry task yet to be created by GetPosition().
              return GenericPromise::CreateAndResolve(true, __func__);
            }

            MOZ_ASSERT(!mAudioSink);
            // Avoiding the side effects of GetPosition() creating another
            // sink another AudioSink and resolving mEndedPromiseHolder, which
            // the new audioSink will now manage.
            TimeUnit switchTime = GetSystemClockPosition(TimeStamp::Now());
            DropAudioPacketsIfNeeded(switchTime);
            mLastClockSource = ClockSource::SystemClock;

            LOG("AudioSink async, start");
            StartAudioSink(std::move(audioSink), switchTime);
            return GenericPromise::CreateAndResolve(true, __func__);
          });
}

nsresult AudioSinkWrapper::SyncCreateAudioSink(const TimeUnit& aStartTime) {
  AssertOwnerThread();
  MOZ_ASSERT(!mAudioSink);
  MOZ_ASSERT(!mAudioSinkEndedRequest.Exists());

  LOG("%p: AudioSinkWrapper::SyncCreateAudioSink(%lf)", this,
      aStartTime.ToSeconds());

  UniquePtr<AudioSink> audioSink = mSinkCreator();
  nsresult rv = audioSink->InitializeAudioStream(
      mParams, mAudioDevice, AudioSink::InitializationType::INITIAL);
  if (NS_FAILED(rv)) {
    LOG("Sync AudioSinkWrapper initialization failed");
    // If a specific device has been specified through setSinkId()
    // the sink is started after the device becomes available again.
    if (mAudioDevice) {
      ScheduleRetrySink();
      return NS_OK;
    }
    // If a default output device is not available, the system may not support
    // audio output.  Report an error so that playback can be aborted if there
    // is no video.
    mEndedPromiseHolder.RejectIfExists(rv, __func__);
    return rv;
  }
  StartAudioSink(std::move(audioSink), aStartTime);

  return NS_OK;
}

void AudioSinkWrapper::ScheduleRetrySink() {
  mRetrySinkTime =
      TimeStamp::Now() + TimeDuration::FromMilliseconds(
                             StaticPrefs::media_audio_device_retry_ms());
}

bool AudioSinkWrapper::IsAudioSourceEnded(const MediaInfo& aInfo) const {
  // no audio or empty audio queue which won't get data anymore is equivalent to
  // audio ended
  return !aInfo.HasAudio() ||
         (mAudioQueue.IsFinished() && mAudioQueue.GetSize() == 0u);
}

void AudioSinkWrapper::Stop() {
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "playback not started.");

  LOG("%p: AudioSinkWrapper::Stop", this);

  mIsStarted = false;
  mClockStartTime = TimeStamp();
  mPositionAtClockStart = TimeUnit::Invalid();
  mAudioEnded = true;
  if (mAudioSink) {
    ShutDownAudioSink();
  }

  mEndedPromiseHolder.ResolveIfExists(true, __func__);
  mEndedPromise = nullptr;
}

bool AudioSinkWrapper::IsStarted() const {
  AssertOwnerThread();
  return mIsStarted;
}

bool AudioSinkWrapper::IsPlaying() const {
  AssertOwnerThread();
  MOZ_ASSERT(mClockStartTime.IsNull() || IsStarted());
  return !mClockStartTime.IsNull();
}

void AudioSinkWrapper::OnAudioEnded(
    const EndedPromise::ResolveOrRejectValue& aValue) {
  AssertOwnerThread();
  // This callback on mAudioSinkEndedRequest should have been disconnected if
  // mEndedPromiseHolder has been settled.
  MOZ_ASSERT(!mEndedPromiseHolder.IsEmpty());
  LOG("%p: AudioSinkWrapper::OnAudioEnded %i", this, aValue.IsResolve());
  mAudioSinkEndedRequest.Complete();
  ShutDownAudioSink();
  // System time is now used for the clock as video may not have ended.
  if (aValue.IsResolve()) {
    mAudioEnded = true;
    mEndedPromiseHolder.Resolve(aValue.ResolveValue(), __func__);
    return;
  }
  if (mAudioDevice) {
    ScheduleRetrySink();  // Device will be restarted when available again.
    return;
  }
  // Default device not available.  Report error.
  mEndedPromiseHolder.Reject(aValue.RejectValue(), __func__);
}

void AudioSinkWrapper::GetDebugInfo(dom::MediaSinkDebugInfo& aInfo) {
  AssertOwnerThread();
  aInfo.mAudioSinkWrapper.mIsPlaying = IsPlaying();
  aInfo.mAudioSinkWrapper.mIsStarted = IsStarted();
  aInfo.mAudioSinkWrapper.mAudioEnded = mAudioEnded;
  if (mAudioSink) {
    mAudioSink->GetDebugInfo(aInfo);
  }
}

}  // namespace mozilla

#undef LOG
#undef LOGV
