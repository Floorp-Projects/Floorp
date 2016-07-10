/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioSink.h"
#include "AudioSinkWrapper.h"

namespace mozilla {
namespace media {

AudioSinkWrapper::~AudioSinkWrapper()
{
}

void
AudioSinkWrapper::Shutdown()
{
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "Must be called after playback stopped.");
  mCreator = nullptr;
}

const MediaSink::PlaybackParams&
AudioSinkWrapper::GetPlaybackParams() const
{
  AssertOwnerThread();
  return mParams;
}

void
AudioSinkWrapper::SetPlaybackParams(const PlaybackParams& aParams)
{
  AssertOwnerThread();
  if (mAudioSink) {
    mAudioSink->SetVolume(aParams.mVolume);
    mAudioSink->SetPlaybackRate(aParams.mPlaybackRate);
    mAudioSink->SetPreservesPitch(aParams.mPreservesPitch);
  }
  mParams = aParams;
}

RefPtr<GenericPromise>
AudioSinkWrapper::OnEnded(TrackType aType)
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");
  if (aType == TrackInfo::kAudioTrack) {
    return mEndPromise;
  }
  return nullptr;
}

int64_t
AudioSinkWrapper::GetEndTime(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");
  if (aType == TrackInfo::kAudioTrack && mAudioSink) {
    return mAudioSink->GetEndTime();
  }
  return -1;
}

int64_t
AudioSinkWrapper::GetVideoPosition(TimeStamp aNow) const
{
  AssertOwnerThread();
  MOZ_ASSERT(!mPlayStartTime.IsNull());
  // Time elapsed since we started playing.
  int64_t delta = (aNow - mPlayStartTime).ToMicroseconds();
  // Take playback rate into account.
  return mPlayDuration + delta * mParams.mPlaybackRate;
}

int64_t
AudioSinkWrapper::GetPosition(TimeStamp* aTimeStamp) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");

  int64_t pos = -1;
  TimeStamp t = TimeStamp::Now();

  if (!mAudioEnded) {
    // Rely on the audio sink to report playback position when it is not ended.
    pos = mAudioSink->GetPosition();
  } else if (!mPlayStartTime.IsNull()) {
    // Calculate playback position using system clock if we are still playing.
    pos = GetVideoPosition(t);
  } else {
    // Return how long we've played if we are not playing.
    pos = mPlayDuration;
  }

  if (aTimeStamp) {
    *aTimeStamp = t;
  }

  return pos;
}

bool
AudioSinkWrapper::HasUnplayedFrames(TrackType aType) const
{
  AssertOwnerThread();
  return mAudioSink ? mAudioSink->HasUnplayedFrames() : false;
}

void
AudioSinkWrapper::SetVolume(double aVolume)
{
  AssertOwnerThread();
  mParams.mVolume = aVolume;
  if (mAudioSink) {
    mAudioSink->SetVolume(aVolume);
  }
}

void
AudioSinkWrapper::SetPlaybackRate(double aPlaybackRate)
{
  AssertOwnerThread();
  if (!mAudioEnded) {
    // Pass the playback rate to the audio sink. The underlying AudioStream
    // will handle playback rate changes and report correct audio position.
    mAudioSink->SetPlaybackRate(aPlaybackRate);
  } else if (!mPlayStartTime.IsNull()) {
    // Adjust playback duration and start time when we are still playing.
    TimeStamp now = TimeStamp::Now();
    mPlayDuration = GetVideoPosition(now);
    mPlayStartTime = now;
  }
  // mParams.mPlaybackRate affects GetVideoPosition(). It should be updated
  // after the calls to GetVideoPosition();
  mParams.mPlaybackRate = aPlaybackRate;

  // Do nothing when not playing. Changes in playback rate will be taken into
  // account by GetVideoPosition().
}

void
AudioSinkWrapper::SetPreservesPitch(bool aPreservesPitch)
{
  AssertOwnerThread();
  mParams.mPreservesPitch = aPreservesPitch;
  if (mAudioSink) {
    mAudioSink->SetPreservesPitch(aPreservesPitch);
  }
}

void
AudioSinkWrapper::SetPlaying(bool aPlaying)
{
  AssertOwnerThread();

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

void
AudioSinkWrapper::Start(int64_t aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "playback already started.");

  mIsStarted = true;
  mPlayDuration = aStartTime;
  mPlayStartTime = TimeStamp::Now();

  // no audio is equivalent to audio ended before video starts.
  mAudioEnded = !aInfo.HasAudio();

  if (aInfo.HasAudio()) {
    mAudioSink = mCreator->Create();
    mEndPromise = mAudioSink->Init(mParams);

    mAudioSinkPromise.Begin(mEndPromise->Then(
      mOwnerThread.get(), __func__, this,
      &AudioSinkWrapper::OnAudioEnded,
      &AudioSinkWrapper::OnAudioEnded));
  }
}

void
AudioSinkWrapper::Stop()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "playback not started.");

  mIsStarted = false;
  mAudioEnded = true;

  if (mAudioSink) {
    mAudioSinkPromise.DisconnectIfExists();
    mAudioSink->Shutdown();
    mAudioSink = nullptr;
    mEndPromise = nullptr;
  }
}

bool
AudioSinkWrapper::IsStarted() const
{
  AssertOwnerThread();
  return mIsStarted;
}

bool
AudioSinkWrapper::IsPlaying() const
{
  AssertOwnerThread();
  return IsStarted() && !mPlayStartTime.IsNull();
}

void
AudioSinkWrapper::OnAudioEnded()
{
  AssertOwnerThread();
  mAudioSinkPromise.Complete();
  mPlayDuration = GetPosition();
  if (!mPlayStartTime.IsNull()) {
    mPlayStartTime = TimeStamp::Now();
  }
  mAudioEnded = true;
}

} // namespace media
} // namespace mozilla

