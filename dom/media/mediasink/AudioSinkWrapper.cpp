/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSinkWrapper.h"
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
  mCreator = nullptr;
  mEndedPromiseHolder.ResolveIfExists(true, __func__);
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
  if (aType == TrackInfo::kAudioTrack && mAudioSink) {
    return mAudioSink->GetEndTime();
  }

  if (aType == TrackInfo::kAudioTrack && !mAudioSink && IsMuted()) {
    if (IsPlaying()) {
      return GetSystemClockPosition(TimeStamp::Now());
    }

    return mPlayDuration;
  }
  return TimeUnit::Zero();
}

TimeUnit AudioSinkWrapper::GetSystemClockPosition(TimeStamp aNow) const {
  AssertOwnerThread();
  MOZ_ASSERT(!mPlayStartTime.IsNull());
  // Time elapsed since we started playing.
  double delta = (aNow - mPlayStartTime).ToSeconds();
  // Take playback rate into account.
  return mPlayDuration + TimeUnit::FromSeconds(delta * mParams.mPlaybackRate);
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

  if (!mAudioEnded && !IsMuted() && mAudioSink && mAudioSink->AudioStreamCallbackStarted()) {
    // Rely on the audio sink to report playback position when it is not ended.
    pos = mAudioSink->GetPosition();
    LOGV("%p: Getting position from the Audio Sink %lf", this, pos.ToSeconds());
  } else if (!mPlayStartTime.IsNull()) {
    // Calculate playback position using system clock if we are still playing,
    // but not rendering the audio, because this audio sink is muted.
    pos = GetSystemClockPosition(t);
    LOGV("%p: Getting position from the system clock %lf", this,
         pos.ToSeconds());
    if (mAudioQueue.GetSize() > 0 && IsMuted()) {
      // audio track, but it's muted and won't be dequeued, discard packets that
      // are behind the current media time, to keep the queue size under
      // control.
      DropAudioPacketsIfNeeded(pos);
      // If muted, it's necessary to manually check if the audio has "ended",
      // meaning that all the audio packets have been consumed, to resolve the
      // ended promise.
      if (CheckIfEnded()) {
        MOZ_ASSERT(!mAudioSink);
        mEndedPromiseHolder.Resolve(true, __func__);
      }
    }
  } else {
    // Return how long we've played if we are not playing.
    pos = mPlayDuration;
    LOGV("%p: Getting static position, not playing %lf", this, pos.ToSeconds());
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
      LOG("Dropping audio packets: media position: %lf, "
          "packet dropped: [%lf, %lf] (%u so far).\n",
          aMediaPosition.ToSeconds(), audio->mTime.ToSeconds(),
          (audio->mTime + audio->mDuration).ToSeconds(), dropped);
    }
    audio = mAudioQueue.PeekFront();
  }
}

void AudioSinkWrapper::OnMuted(bool aMuted) {
  AssertOwnerThread();
  if (aMuted) {
    if (mAudioSink) {
      LOG("AudioSinkWrapper muted, shutting down AudioStream.");
      mAudioSinkEndedPromise.DisconnectIfExists();
      mPlayDuration = mAudioSink->GetPosition();
      mPlayStartTime = TimeStamp::Now();
      Maybe<MozPromiseHolder<MediaSink::EndedPromise>> rv =
          mAudioSink->Shutdown(ShutdownCause::Muting);
      MOZ_ASSERT(rv.isSome());
      mEndedPromiseHolder = std::move(rv.ref());
      mAudioSink = nullptr;
    }
  } else {
    if (!IsPlaying()) {
      return;
    }
    LOG("AudioSinkWrapper unmuted, re-creating an AudioStream.");
    TimeUnit mediaPosition = GetSystemClockPosition(TimeStamp::Now());
    DropAudioPacketsIfNeeded(mediaPosition);
    nsresult rv = StartAudioSink(mediaPosition);
    if (NS_FAILED(rv)) {
      NS_WARNING(
          "Could not start AudioSink from AudioSinkWrapper when unmuting");
    }
  }
}

void AudioSinkWrapper::SetVolume(double aVolume) {
  AssertOwnerThread();

  if (aVolume == 0. && mParams.mVolume != 0.) {
    OnMuted(true);
  } else if (aVolume != 0. && mParams.mVolume == 0.) {
    OnMuted(false);
  }

  mParams.mVolume = aVolume;
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
  } else if (!mPlayStartTime.IsNull()) {
    // Adjust playback duration and start time when we are still playing.
    TimeStamp now = TimeStamp::Now();
    mPlayDuration = GetSystemClockPosition(now);
    mPlayStartTime = now;
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
  LOG("%p: SetPlaying %s", this, aPlaying ? "true" : "false");

  // Resume/pause matters only when playback started.
  if (!mIsStarted) {
    return;
  }

  if (mAudioSink) {
    mAudioSink->SetPlaying(aPlaying);
  }

  if (aPlaying) {
    MOZ_ASSERT(mPlayStartTime.IsNull());
    mPlayStartTime = TimeStamp::Now();
  } else {
    // Remember how long we've played.
    mPlayDuration = GetPosition();
    // mPlayStartTime must be updated later since GetPosition()
    // depends on the value of mPlayStartTime.
    mPlayStartTime = TimeStamp();
  }
}

double AudioSinkWrapper::PlaybackRate() const {
  AssertOwnerThread();
  return mParams.mPlaybackRate;
}

nsresult AudioSinkWrapper::Start(const TimeUnit& aStartTime,
                                 const MediaInfo& aInfo) {
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "playback already started.");

  mIsStarted = true;
  mPlayDuration = aStartTime;
  mPlayStartTime = TimeStamp::Now();
  mAudioEnded = IsAudioSourceEnded(aInfo);

  if (mAudioEnded) {
    // Resolve promise if we start playback at the end position of the audio.
    mEndedPromise =
        aInfo.HasAudio()
            ? MediaSink::EndedPromise::CreateAndResolve(true, __func__)
            : nullptr;
    return NS_OK;
  }

  nsresult rv = StartAudioSink(aStartTime);

  return rv;
}

nsresult AudioSinkWrapper::StartAudioSink(const TimeUnit& aStartTime) {
  MOZ_RELEASE_ASSERT(!mAudioSink);

  RefPtr<MediaSink::EndedPromise> promise =
      mEndedPromiseHolder.Ensure(__func__);

  mAudioSink.reset(mCreator->Create(aStartTime));
  nsresult rv = mAudioSink->Start(mParams, mEndedPromiseHolder);
  if (NS_FAILED(rv)) {
    mEndedPromise = MediaSink::EndedPromise::CreateAndReject(rv, __func__);
  } else {
    mEndedPromise = promise;
  }

  mAudioSinkEndedPromise.DisconnectIfExists();
  mEndedPromise
      ->Then(mOwnerThread.get(), __func__, this,
             &AudioSinkWrapper::OnAudioEnded, &AudioSinkWrapper::OnAudioEnded)
      ->Track(mAudioSinkEndedPromise);

  return rv;
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

  mIsStarted = false;
  mAudioEnded = true;

  if (mAudioSink) {
    mAudioSinkEndedPromise.DisconnectIfExists();
    DebugOnly<Maybe<MozPromiseHolder<EndedPromise>>> rv =
        mAudioSink->Shutdown();
    MOZ_ASSERT(rv.inspect().isNothing());
    mAudioSink = nullptr;
    mEndedPromise = nullptr;
  }
}

bool AudioSinkWrapper::IsStarted() const {
  AssertOwnerThread();
  return mIsStarted;
}

bool AudioSinkWrapper::IsPlaying() const {
  AssertOwnerThread();
  return IsStarted() && !mPlayStartTime.IsNull();
}

void AudioSinkWrapper::OnAudioEnded() {
  AssertOwnerThread();
  mAudioSinkEndedPromise.Complete();
  mPlayDuration = GetPosition();
  if (!mPlayStartTime.IsNull()) {
    mPlayStartTime = TimeStamp::Now();
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

}  // namespace mozilla

#undef LOG
#undef LOGV
