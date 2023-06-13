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
#include "nsPrintfCString.h"

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
  if (aType == TrackInfo::kAudioTrack && mAudioSink &&
      mAudioSink->AudioStreamCallbackStarted()) {
    return mAudioSink->GetEndTime();
  }

  if (aType == TrackInfo::kAudioTrack && !mAudioSink && IsMuted()) {
    if (IsPlaying()) {
      return GetSystemClockPosition(TimeStamp::Now());
    }

    return mPositionAtClockStart;
  }
  return TimeUnit::Zero();
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

  if (!mAudioEnded && !IsMuted() && mAudioSink) {
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
    if (IsMuted()) {
      if (mAudioQueue.GetSize() > 0) {
        // audio track, but it's muted and won't be dequeued, discard packets
        // that are behind the current media time, to keep the queue size under
        // control.
        DropAudioPacketsIfNeeded(pos);
      }
      // If muted, it's necessary to manually check if the audio has "ended",
      // meaning that all the audio packets have been consumed, to resolve the
      // ended promise.
      if (CheckIfEnded()) {
        MOZ_ASSERT(!mAudioSink);
        mEndedPromiseHolder.ResolveIfExists(true, __func__);
      }
    }
    mLastClockSource = ClockSource::SystemClock;
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
  while (audio && audio->mTime + audio->mDuration < aMediaPosition) {
    // drop this packet, try the next one
    audio = mAudioQueue.PopFront();
    dropped++;
    if (audio) {
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
      mAudioSinkEndedPromise.DisconnectIfExists();
      if (IsPlaying()) {
        mPositionAtClockStart = mAudioSink->GetPosition();
        mClockStartTime = TimeStamp::Now();
      }
      Maybe<MozPromiseHolder<MediaSink::EndedPromise>> rv =
          mAudioSink->Shutdown(ShutdownCause::Muting);
      // There will generally be a promise here, except if the stream has
      // errored out, or if it has just finished. In both cases, the promise has
      // been handled appropriately, there is nothing to do.
      if (rv.isSome()) {
        mEndedPromiseHolder = std::move(rv.ref());
      }
      mAudioSink = nullptr;
    }
  } else {
    if (!NeedAudioSink()) {
      LOG("%p: AudioSinkWrapper::OnMuted: AudioSink not needed", this);
      return;
    }
    LOG("%p: AudioSinkWrapper unmuted, re-creating an AudioStream.", this);
    TimeUnit mediaPosition = GetSystemClockPosition(TimeStamp::Now());
    nsresult rv = CreateAudioSink(mediaPosition, AudioSinkStartPolicy::ASYNC);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "Could not start AudioSink from AudioSinkWrapper when unmuting");
    }
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
  if (!mAudioEnded && mAudioSink) {
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
      CreateAudioSink(switchTime, AudioSinkStartPolicy::SYNC);
    }
  } else {
    // Remember how long we've played.
    mPositionAtClockStart = GetPosition();
    // mClockStartTime must be updated later since GetPosition()
    // depends on the value of mClockStartTime.
    mClockStartTime = TimeStamp();
  }
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
  return CreateAudioSink(aStartTime, AudioSinkStartPolicy::SYNC);
}

bool AudioSinkWrapper::NeedAudioSink() {
  // An AudioSink is needed if unmuted, playing, and not ended.  The not-ended
  // check also avoids creating an AudioSink when there is no audio track.
  // mEndedPromiseHolder can be non-empty only if the promise is not managed
  // by an existing AudioSink.
  MOZ_ASSERT(!mAudioSink);
  return !IsMuted() && IsPlaying() && !mEndedPromiseHolder.IsEmpty();
}

void AudioSinkWrapper::StartAudioSink(const TimeUnit& aStartTime) {
  nsresult rv = mAudioSink->Start(aStartTime, mEndedPromiseHolder);
  if (NS_FAILED(rv)) {
    LOG("AudioSinkWrapper::StartAudioSink failed");
    mEndedPromiseHolder.RejectIfExists(rv, __func__);
    return;
  }
  mEndedPromise
      ->Then(mOwnerThread.get(), __func__, this,
             &AudioSinkWrapper::OnAudioEnded)
      ->Track(mAudioSinkEndedPromise);
}

nsresult AudioSinkWrapper::CreateAudioSink(const TimeUnit& aStartTime,
                                           AudioSinkStartPolicy aPolicy) {
  MOZ_RELEASE_ASSERT(!mAudioSink);
  MOZ_ASSERT(!mAudioSinkEndedPromise.Exists());

  LOG("%p: AudioSinkWrapper::CreateAudioSink (%s)", this,
      aPolicy == AudioSinkStartPolicy::ASYNC ? "Async" : "Sync");

  if (aPolicy == AudioSinkStartPolicy::ASYNC) {
    UniquePtr<AudioSink> audioSink = mSinkCreator();
    NS_DispatchBackgroundTask(NS_NewRunnableFunction(
        "CreateAudioSink (Async part: initialization)",
        [self = RefPtr<AudioSinkWrapper>(this), audioSink{std::move(audioSink)},
         this]() mutable {
          LOG("AudioSink initialization on background thread");
          // This can take about 200ms, e.g. on Windows, we don't want to do
          // it on the MDSM thread, because it would make the clock not update
          // for that amount of time, and the video would therefore not
          // update. The Start() call is very cheap on the other hand, we can
          // do it from the MDSM thread.
          nsresult rv = audioSink->InitializeAudioStream(
              mParams, mAudioDevice, AudioSink::InitializationType::UNMUTING);
          mOwnerThread->Dispatch(NS_NewRunnableFunction(
              "CreateAudioSink (Async part: start from MDSM thread)",
              [self = RefPtr<AudioSinkWrapper>(this),
               audioSink{std::move(audioSink)}, this, rv]() mutable {
                LOG("AudioSink async init done, back on MDSM thread");
                if (NS_FAILED(rv)) {
                  LOG("Async AudioSink initialization failed");
                  mEndedPromiseHolder.RejectIfExists(rv, __func__);
                  return;
                }

                // It's possible that the newly created isn't needed at this
                // point, in some cases:
                // 1. An AudioSink was created synchronously while this
                // AudioSink was initialized asynchronously, bail out here. This
                // happens when seeking (which does a synchronous
                // initialization) right after unmuting.
                // 2. The media element was muted while the async initialization
                // was happening.
                // 3. The AudioSinkWrapper was paused or stopped during
                // asynchronous initialization.
                // 4. The audio has ended during asynchronous initialization.
                if (mAudioSink || !NeedAudioSink()) {
                  LOG("AudioSink initialized async isn't needed, shutting "
                      "it down.");
                  DebugOnly<Maybe<MozPromiseHolder<EndedPromise>>> rv =
                      audioSink->Shutdown();
                  MOZ_ASSERT(rv.inspect().isNothing());
                  return;
                }

                MOZ_ASSERT(!mAudioSink);
                TimeUnit switchTime = GetPosition();
                DropAudioPacketsIfNeeded(switchTime);
                mAudioSink.swap(audioSink);
                if (mTreatUnderrunAsSilence) {
                  mAudioSink->EnableTreatAudioUnderrunAsSilence(
                      mTreatUnderrunAsSilence);
                }
                LOG("AudioSink async, start");
                StartAudioSink(switchTime);
              }));
        }));
  } else {
    mAudioSink = mSinkCreator();
    nsresult rv = mAudioSink->InitializeAudioStream(
        mParams, mAudioDevice, AudioSink::InitializationType::INITIAL);
    if (NS_FAILED(rv)) {
      mEndedPromiseHolder.RejectIfExists(rv, __func__);
      LOG("Sync AudioSinkWrapper initialization failed");
      return rv;
    }
    if (mTreatUnderrunAsSilence) {
      mAudioSink->EnableTreatAudioUnderrunAsSilence(mTreatUnderrunAsSilence);
    }
    StartAudioSink(aStartTime);
  }

  return NS_OK;
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

  mAudioSinkEndedPromise.DisconnectIfExists();

  if (mAudioSink) {
    DebugOnly<Maybe<MozPromiseHolder<EndedPromise>>> rv =
        mAudioSink->Shutdown();
    MOZ_ASSERT(rv.inspect().isNothing());
    mAudioSink = nullptr;
  } else {
    mEndedPromiseHolder.ResolveIfExists(true, __func__);
  }
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

void AudioSinkWrapper::OnAudioEnded() {
  AssertOwnerThread();
  LOG("%p: AudioSinkWrapper::OnAudioEnded", this);
  mAudioSinkEndedPromise.Complete();
  mPositionAtClockStart = GetPosition();
  if (!mClockStartTime.IsNull()) {  // playing
    // System time is now used for the clock as video may not have ended.
    mClockStartTime = TimeStamp::Now();
  }
  mAudioEnded = true;
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

void AudioSinkWrapper::EnableTreatAudioUnderrunAsSilence(bool aEnabled) {
  mTreatUnderrunAsSilence = aEnabled;
  if (mAudioSink) {
    mAudioSink->EnableTreatAudioUnderrunAsSilence(aEnabled);
  }
}

}  // namespace mozilla

#undef LOG
#undef LOGV
