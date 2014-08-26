/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MEDIA_OMX_COMMON_DECODER_H
#define MEDIA_OMX_COMMON_DECODER_H

#include "MediaDecoder.h"

namespace android {
struct MOZ_EXPORT MediaSource;
} // namespace android

namespace mozilla {

class AudioOffloadPlayerBase;
class MediaOmxCommonReader;

class MediaOmxCommonDecoder : public MediaDecoder
{
public:
  MediaOmxCommonDecoder();

  virtual void MetadataLoaded(MediaInfo* aInfo,
                              MetadataTags* aTags);
  virtual void ChangeState(PlayState aState);
  virtual void ApplyStateToStateMachine(PlayState aState);
  virtual void SetVolume(double aVolume);
  virtual void PlaybackPositionChanged();
  virtual void UpdateReadyStateForData();
  virtual void SetElementVisibility(bool aIsVisible);
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio);
  virtual bool CheckDecoderCanOffloadAudio();
  virtual void AddOutputStream(ProcessedMediaStream* aStream,
                               bool aFinishWhenEnded);
  virtual void SetPlaybackRate(double aPlaybackRate);

  void AudioOffloadTearDown();

  virtual MediaDecoderStateMachine* CreateStateMachine();

  virtual MediaOmxCommonReader* CreateReader() = 0;
  virtual MediaDecoderStateMachine* CreateStateMachine(MediaOmxCommonReader* aReader) = 0;

protected:
  void PauseStateMachine();
  void ResumeStateMachine();

  MediaOmxCommonReader* mReader;

  // Offloaded audio track
  android::sp<android::MediaSource> mAudioTrack;

  nsAutoPtr<AudioOffloadPlayerBase> mAudioOffloadPlayer;

  // Set by Media*Reader to denote current track can be offloaded
  bool mCanOffloadAudio;

  // Set when offload playback of current track fails in the middle and need to
  // fallback to state machine
  bool mFallbackToStateMachine;
};

} // namespace mozilla

#endif // MEDIA_OMX_COMMON_DECODER_H
