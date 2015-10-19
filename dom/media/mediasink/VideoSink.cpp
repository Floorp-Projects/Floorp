/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoSink.h"

namespace mozilla {

extern PRLogModuleInfo* gMediaDecoderLog;
#define VSINK_LOG(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Debug, \
    ("VideoSink=%p " msg, this, ##__VA_ARGS__))
#define VSINK_LOG_V(msg, ...) \
  MOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, \
  ("VideoSink=%p " msg, this, ##__VA_ARGS__))

using namespace mozilla::layers;

namespace media {

VideoSink::~VideoSink()
{
}

const MediaSink::PlaybackParams&
VideoSink::GetPlaybackParams() const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  return mAudioSink->GetPlaybackParams();
}

void
VideoSink::SetPlaybackParams(const PlaybackParams& aParams)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  mAudioSink->SetPlaybackParams(aParams);
}

RefPtr<GenericPromise>
VideoSink::OnEnded(TrackType aType)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->OnEnded(aType);
  } else if (aType == TrackInfo::kVideoTrack) {
    return mEndPromise;
  }
  return nullptr;
}

int64_t
VideoSink::GetEndTime(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  MOZ_ASSERT(mAudioSink->IsStarted(), "Must be called after playback starts.");

  if (aType == TrackInfo::kVideoTrack) {
    return mVideoFrameEndTime;
  } else if (aType == TrackInfo::kAudioTrack) {
    return mAudioSink->GetEndTime(aType);
  }
  return -1;
}

int64_t
VideoSink::GetPosition(TimeStamp* aTimeStamp) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  return mAudioSink->GetPosition(aTimeStamp);
}

bool
VideoSink::HasUnplayedFrames(TrackType aType) const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  MOZ_ASSERT(aType == TrackInfo::kAudioTrack, "Not implemented for non audio tracks.");

  return mAudioSink->HasUnplayedFrames(aType);
}

void
VideoSink::SetPlaybackRate(double aPlaybackRate)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  mAudioSink->SetPlaybackRate(aPlaybackRate);
}

void
VideoSink::SetPlaying(bool aPlaying)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  VSINK_LOG_V(" playing (%d) -> (%d)", mAudioSink->IsPlaying(), aPlaying);

  mAudioSink->SetPlaying(aPlaying);
}

void
VideoSink::Start(int64_t aStartTime, const MediaInfo& aInfo)
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  VSINK_LOG("[%s]", __func__);
  mAudioSink->Start(aStartTime, aInfo);

  if (aInfo.HasVideo()) {
    mEndPromise = mEndPromiseHolder.Ensure(__func__);
    mVideoSinkEndRequest.Begin(mEndPromise->Then(
      mOwnerThread.get(), __func__, this,
      &VideoSink::OnVideoEnded,
      &VideoSink::OnVideoEnded));
  }
}

void
VideoSink::OnVideoEnded()
{
  AssertOwnerThread();

  mVideoSinkEndRequest.Complete();
}

void
VideoSink::Stop()
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  MOZ_ASSERT(mAudioSink->IsStarted(), "playback not started.");
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Stop();

  mVideoSinkEndRequest.DisconnectIfExists();
  mEndPromiseHolder.ResolveIfExists(true, __func__);
  mEndPromise = nullptr;
}

bool
VideoSink::IsStarted() const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  return mAudioSink->IsStarted();
}

bool
VideoSink::IsPlaying() const
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");

  return mAudioSink->IsPlaying();
}

void
VideoSink::Shutdown()
{
  AssertOwnerThread();
  MOZ_ASSERT(mAudioSink, "AudioSink should exist.");
  MOZ_ASSERT(!mAudioSink->IsStarted(), "must be called after playback stops.");
  VSINK_LOG("[%s]", __func__);

  mAudioSink->Shutdown();
}

} // namespace media
} // namespace mozilla
