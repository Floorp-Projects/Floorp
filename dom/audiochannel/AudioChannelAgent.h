/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audio_channel_agent_h__
#define mozilla_dom_audio_channel_agent_h__

#include "nsIAudioChannelAgent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsIWeakReferenceUtils.h"

class nsPIDOMWindowInner;
class nsPIDOMWindowOuter;

namespace mozilla::dom {

class AudioPlaybackConfig;

/* Header file */
class AudioChannelAgent : public nsIAudioChannelAgent {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIAUDIOCHANNELAGENT

  NS_DECL_CYCLE_COLLECTION_CLASS(AudioChannelAgent)

  AudioChannelAgent();

  // nsIAudioChannelAgentCallback MUST call this function after calling
  // NotifyStartedPlaying() to require the initial update for
  // volume/suspend/audio-capturing which might set before starting the agent.
  // Ex. starting the agent in a tab which has been muted before, so the agent
  // should apply mute state to its callback.
  void PullInitialUpdate();

  uint64_t WindowID() const;

  bool IsWindowAudioCapturingEnabled() const;
  bool IsPlayingStarted() const;

 private:
  virtual ~AudioChannelAgent();

  friend class AudioChannelService;
  void WindowVolumeChanged(float aVolume, bool aMuted);
  void WindowSuspendChanged(nsSuspendedTypes aSuspend);
  void WindowAudioCaptureChanged(uint64_t aInnerWindowID, bool aCapture);

  nsPIDOMWindowOuter* Window() const { return mWindow; }
  uint64_t InnerWindowID() const;
  AudioPlaybackConfig GetMediaConfig() const;

  // Returns mCallback if that's non-null, or otherwise tries to get an
  // nsIAudioChannelAgentCallback out of mWeakCallback.
  already_AddRefed<nsIAudioChannelAgentCallback> GetCallback();

  nsresult InitInternal(nsPIDOMWindowInner* aWindow,
                        nsIAudioChannelAgentCallback* aCallback,
                        bool aUseWeakRef);

  void Shutdown();

  nsresult FindCorrectWindow(nsPIDOMWindowInner* aWindow);

  nsCOMPtr<nsPIDOMWindowOuter> mWindow;
  nsCOMPtr<nsIAudioChannelAgentCallback> mCallback;

  nsWeakPtr mWeakCallback;

  uint64_t mInnerWindowID;
  bool mIsRegToService;
};

}  // namespace mozilla::dom

#endif
