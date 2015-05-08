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

  virtual void FirstFrameLoaded(nsAutoPtr<MediaInfo> aInfo,
                                MediaDecoderEventVisibility aEventVisibility) override;
  virtual void ChangeState(PlayState aState) override;
  virtual void CallSeek(const SeekTarget& aTarget) override;
  virtual void SetVolume(double aVolume) override;
  virtual void PlaybackPositionChanged(MediaDecoderEventVisibility aEventVisibility =
                                         MediaDecoderEventVisibility::Observable) override;
  virtual MediaDecoderOwner::NextFrameStatus NextFrameStatus() override;
  virtual void SetElementVisibility(bool aIsVisible) override;
  virtual void SetPlatformCanOffloadAudio(bool aCanOffloadAudio) override;
  virtual bool CheckDecoderCanOffloadAudio() override;
  virtual void AddOutputStream(ProcessedMediaStream* aStream,
                               bool aFinishWhenEnded) override;
  virtual void SetPlaybackRate(double aPlaybackRate) override;

  void AudioOffloadTearDown();

  virtual MediaDecoderStateMachine* CreateStateMachine() override;

  virtual MediaOmxCommonReader* CreateReader() = 0;
  virtual MediaDecoderStateMachine* CreateStateMachineFromReader(MediaOmxCommonReader* aReader) = 0;

protected:
  virtual ~MediaOmxCommonDecoder();
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
