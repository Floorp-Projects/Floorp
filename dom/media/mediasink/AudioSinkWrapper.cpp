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
    mAudioSink->SetVolume(aParams.volume);
    mAudioSink->SetPlaybackRate(aParams.playbackRate);
    mAudioSink->SetPreservesPitch(aParams.preservesPitch);
  }
  mParams = aParams;
}

nsRefPtr<GenericPromise>
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
AudioSinkWrapper::GetPosition() const
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "Must be called after playback starts.");
  return mAudioSink->GetPosition();
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
  mParams.volume = aVolume;
  if (mAudioSink) {
    mAudioSink->SetVolume(aVolume);
  }
}

void
AudioSinkWrapper::SetPlaybackRate(double aPlaybackRate)
{
  AssertOwnerThread();
  mParams.playbackRate = aPlaybackRate;
  if (mAudioSink) {
    mAudioSink->SetPlaybackRate(aPlaybackRate);
  }
}

void
AudioSinkWrapper::SetPreservesPitch(bool aPreservesPitch)
{
  AssertOwnerThread();
  mParams.preservesPitch = aPreservesPitch;
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
}

void
AudioSinkWrapper::Start(int64_t aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  MOZ_ASSERT(!mIsStarted, "playback already started.");

  mIsStarted = true;

  mAudioSink = mCreator->Create();
  mEndPromise = mAudioSink->Init();
  SetPlaybackParams(mParams);
}

void
AudioSinkWrapper::Stop()
{
  AssertOwnerThread();
  MOZ_ASSERT(mIsStarted, "playback not started.");

  mIsStarted = false;
  mAudioSink->Shutdown();
  mAudioSink = nullptr;
  mEndPromise = nullptr;
}

bool
AudioSinkWrapper::IsStarted() const
{
  AssertOwnerThread();
  return mIsStarted;
}

} // namespace media
} // namespace mozilla

