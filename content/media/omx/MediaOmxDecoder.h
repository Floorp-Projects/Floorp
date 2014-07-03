/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(MediaOmxDecoder_h_)
#define MediaOmxDecoder_h_

#include "base/basictypes.h"
#include "MediaDecoder.h"
#include "MediaOmxReader.h"
#include "AudioOffloadPlayerBase.h"

namespace mozilla {

class MediaOmxDecoder : public MediaDecoder
{
  typedef android::MediaSource MediaSource;
public:
  MediaOmxDecoder();
  virtual MediaDecoder* Clone();
  virtual MediaDecoderStateMachine* CreateStateMachine();

  virtual void MetadataLoaded(int aChannels,
                              int aRate,
                              bool aHasAudio,
                              bool aHasVideo,
                              MetadataTags* aTags);
  virtual void ChangeState(PlayState aState);
  virtual void ApplyStateToStateMachine(PlayState aState);
  virtual void SetVolume(double aVolume);
  virtual void PlaybackPositionChanged();
  virtual void UpdateReadyStateForData();
  virtual void SetElementVisibility(bool aIsVisible);
  virtual void SetCanOffloadAudio(bool aCanOffloadAudio);
  virtual void AddOutputStream(ProcessedMediaStream* aStream,
                               bool aFinishWhenEnded);
  virtual void SetPlaybackRate(double aPlaybackRate);

  void AudioOffloadTearDown();
  int64_t GetSeekTime() { return mRequestedSeekTarget.mTime; }
  void ResetSeekTime() { mRequestedSeekTarget.Reset(); }

private:
  void PauseStateMachine();
  void ResumeStateMachine();

  MediaOmxReader* mReader;

  // Offloaded audio track
  android::sp<MediaSource> mAudioTrack;

  nsAutoPtr<AudioOffloadPlayerBase> mAudioOffloadPlayer;

  // Set by MediaOmxReader to denote current track can be offloaded
  bool mCanOffloadAudio;

  // Set when offload playback of current track fails in the middle and need to
  // fallback to state machine
  bool mFallbackToStateMachine;
};

} // namespace mozilla

#endif
