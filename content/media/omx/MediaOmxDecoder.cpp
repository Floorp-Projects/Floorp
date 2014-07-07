/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaOmxDecoder.h"
#include "MediaOmxReader.h"
#include "MediaDecoderStateMachine.h"

#include "OmxDecoder.h"

#ifdef MOZ_AUDIO_OFFLOAD
#include "AudioOffloadPlayer.h"
#endif

using namespace android;

namespace mozilla {

#ifdef PR_LOGGING
extern PRLogModuleInfo* gMediaDecoderLog;
#define DECODER_LOG(type, msg) PR_LOG(gMediaDecoderLog, type, msg)
#else
#define DECODER_LOG(type, msg)
#endif

MediaOmxDecoder::MediaOmxDecoder() :
  MediaDecoder(),
  mCanOffloadAudio(false),
  mFallbackToStateMachine(false)
{
#ifdef PR_LOGGING
  if (!gMediaDecoderLog) {
    gMediaDecoderLog = PR_NewLogModule("MediaDecoder");
  }
#endif
}

MediaDecoder* MediaOmxDecoder::Clone()
{
  return new MediaOmxDecoder();
}

MediaDecoderStateMachine* MediaOmxDecoder::CreateStateMachine()
{
  mReader = new MediaOmxReader(this);
  mReader->SetAudioChannel(GetAudioChannel());
  return new MediaDecoderStateMachine(this, mReader);
}

void MediaOmxDecoder::SetCanOffloadAudio(bool aCanOffloadAudio)
{
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  mCanOffloadAudio = aCanOffloadAudio;
}

void MediaOmxDecoder::MetadataLoaded(MediaInfo* aInfo,
                                     MetadataTags* aTags)
{
  MOZ_ASSERT(NS_IsMainThread());
  MediaDecoder::MetadataLoaded(aInfo, aTags);

  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  if (!mCanOffloadAudio || mFallbackToStateMachine || mOutputStreams.Length() ||
      mInitialPlaybackRate != 1.0) {
    DECODER_LOG(PR_LOG_DEBUG, ("In %s Offload Audio check failed",
        __PRETTY_FUNCTION__));
    return;
  }

#ifdef MOZ_AUDIO_OFFLOAD
  mAudioOffloadPlayer = new AudioOffloadPlayer(this);
#endif
  mAudioOffloadPlayer->SetSource(mReader->GetAudioOffloadTrack());
  status_t err = mAudioOffloadPlayer->Start(false);
  if (err == OK) {
    PauseStateMachine();
    // Call ChangeState() to run AudioOffloadPlayer since offload state enabled
    ChangeState(mPlayState);
    return;
  }

  mAudioOffloadPlayer = nullptr;
  DECODER_LOG(PR_LOG_DEBUG, ("In %s Unable to start offload audio %d."
      "Switching to normal mode", __PRETTY_FUNCTION__, err));
}

void MediaOmxDecoder::PauseStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  GetReentrantMonitor().AssertCurrentThreadIn();
  DECODER_LOG(PR_LOG_DEBUG, ("%s", __PRETTY_FUNCTION__));
  if (!mDecoderStateMachine) {
    return;
  }
  StopProgress();
  mDecoderStateMachine->SetDormant(true);
}

void MediaOmxDecoder::ResumeStateMachine()
{
  MOZ_ASSERT(NS_IsMainThread());
  ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
  DECODER_LOG(PR_LOG_DEBUG, ("%s current time %f", __PRETTY_FUNCTION__,
      mCurrentTime));

  if (!mDecoderStateMachine) {
    return;
  }

  mFallbackToStateMachine = true;
  mAudioOffloadPlayer = nullptr;
  mRequestedSeekTarget = SeekTarget(mCurrentTime, SeekTarget::Accurate);

  mNextState = mPlayState;
  ChangeState(PLAY_STATE_LOADING);
  mDecoderStateMachine->SetDormant(false);
}

void MediaOmxDecoder::AudioOffloadTearDown()
{
  MOZ_ASSERT(NS_IsMainThread());
  DECODER_LOG(PR_LOG_DEBUG, ("%s", __PRETTY_FUNCTION__));

  // mAudioOffloadPlayer can be null here if ResumeStateMachine was called
  // just before because of some other error.
  if (mAudioOffloadPlayer) {
    // Audio offload player sent tear down event. Fallback to state machine
    PlaybackPositionChanged();
    ResumeStateMachine();
  }
}

void MediaOmxDecoder::AddOutputStream(ProcessedMediaStream* aStream,
                                      bool aFinishWhenEnded)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioOffloadPlayer) {
    // Offload player cannot handle MediaStream. Fallback
    PlaybackPositionChanged();
    ResumeStateMachine();
  }

  MediaDecoder::AddOutputStream(aStream, aFinishWhenEnded);
}

void MediaOmxDecoder::SetPlaybackRate(double aPlaybackRate)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mAudioOffloadPlayer &&
      ((aPlaybackRate != 0.0) || (aPlaybackRate != 1.0))) {
    // Offload player cannot handle playback rate other than 1/0. Fallback
    PlaybackPositionChanged();
    ResumeStateMachine();
  }

  MediaDecoder::SetPlaybackRate(aPlaybackRate);
}

void MediaOmxDecoder::ChangeState(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  // Keep MediaDecoder state in sync with MediaElement irrespective of offload
  // playback so it will continue to work in normal mode when offloading fails
  // in between
  MediaDecoder::ChangeState(aState);

  if (mAudioOffloadPlayer) {
    status_t err = mAudioOffloadPlayer->ChangeState(aState);
    if (err != OK) {
      ResumeStateMachine();
    }
  }
}

void MediaOmxDecoder::ApplyStateToStateMachine(PlayState aState)
{
  MOZ_ASSERT(NS_IsMainThread());
  // During offload playback, state machine should be in dormant state.
  // ApplyStateToStateMachine() can change state machine state to
  // something else or reset the seek time. So don't call this when audio is
  // offloaded
  if (!mAudioOffloadPlayer) {
    MediaDecoder::ApplyStateToStateMachine(aState);
  }
}

void MediaOmxDecoder::PlaybackPositionChanged()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mAudioOffloadPlayer) {
    MediaDecoder::PlaybackPositionChanged();
    return;
  }

  if (!mOwner || mShuttingDown) {
    return;
  }

  double lastTime = mCurrentTime;
  {
    ReentrantMonitorAutoEnter mon(GetReentrantMonitor());
    mCurrentTime = mAudioOffloadPlayer->GetMediaTimeSecs();
  }
  if (mOwner && lastTime != mCurrentTime) {
    FireTimeUpdate();
  }
}

void MediaOmxDecoder::SetElementVisibility(bool aIsVisible)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mAudioOffloadPlayer) {
    mAudioOffloadPlayer->SetElementVisibility(aIsVisible);
  }
}

void MediaOmxDecoder::UpdateReadyStateForData()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mAudioOffloadPlayer) {
    MediaDecoder::UpdateReadyStateForData();
    return;
  }

  if (!mOwner || mShuttingDown)
    return;
  mOwner->UpdateReadyStateForData(mAudioOffloadPlayer->GetNextFrameStatus());
}

void MediaOmxDecoder::SetVolume(double aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!mAudioOffloadPlayer) {
    MediaDecoder::SetVolume(aVolume);
    return;
  }
  mAudioOffloadPlayer->SetVolume(aVolume);
}

} // namespace mozilla
