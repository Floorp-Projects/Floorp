/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservice_h__
#define mozilla_dom_audiochannelservice_h__

#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"

#include "AudioChannelAgent.h"
#include "nsAttrValue.h"
#include "mozilla/Logging.h"
#include "mozilla/UniquePtr.h"

#include <functional>

class nsPIDOMWindowOuter;
struct PRLogModuleInfo;

namespace mozilla {
namespace dom {

class AudioPlaybackConfig {
 public:
  AudioPlaybackConfig()
      : mVolume(1.0),
        mMuted(false),
        mSuspend(nsISuspendedTypes::NONE_SUSPENDED),
        mNumberOfAgents(0) {}

  AudioPlaybackConfig(float aVolume, bool aMuted, uint32_t aSuspended)
      : mVolume(aVolume),
        mMuted(aMuted),
        mSuspend(aSuspended),
        mNumberOfAgents(0) {}

  float mVolume;
  bool mMuted;
  uint32_t mSuspend;
  bool mCapturedAudio = false;
  uint32_t mNumberOfAgents;
};

class AudioChannelService final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * We use `AudibleState` to represent the audible state of an owner of audio
   * channel agent. Those information in AudioChannelWindow could help us to
   * determine if a tab is being audible or not, in order to tell Chrome JS to
   * show the sound indicator or delayed autoplay icon on the tab bar.
   *
   * - Sound indicator
   * When a tab is playing sound, we would show the sound indicator on tab bar
   * to tell users that this tab is producing sound now. In addition, the sound
   * indicator also give users an ablility to mute or unmute tab.
   *
   * When an AudioChannelWindow first contains an agent with state `eAudible`,
   * or an AudioChannelWindow losts its last agent with state `eAudible`, we
   * would notify Chrome JS about those changes, to tell them that a tab has
   * been being audible or not, in order to display or remove the indicator for
   * a corresponding tab.
   *
   * - Delayed autoplay icon (Play Tab icon)
   * When we enable delaying autoplay, which is to postpone the autoplay media
   * for unvisited tab until it first goes to foreground, or user click the
   * play tab icon to resume the delayed media.
   *
   * When an AudioChannelWindow first contains an agent with state `eAudible` or
   * `eMaybeAudible`, we would notify Chrome JS about this change, in order to
   * show the delayed autoplay tab icon to user, which is used to notice user
   * there is a media being delayed starting, and then user can click the play
   * tab icon to resume the start of media, or visit that tab to resume delayed
   * media automatically.
   *
   * According to our UX design, we don't show this icon for inaudible media.
   * The reason of showing the icon for a tab, where the agent starts with state
   * `eMaybeAudible`, is because some video might be silent in the beginning
   * but would soon become audible later.
   *
   * ---------------------------------------------------------------------------
   *
   * eNotAudible : agent is not audible
   * eMaybeAudible : agent is not audible now, but it might be audible later
   * eAudible : agent is audible now
   */
  enum AudibleState : uint8_t {
    eNotAudible = 0,
    eMaybeAudible = 1,
    eAudible = 2
  };

  enum AudioCaptureState : bool { eCapturing = true, eNotCapturing = false };

  enum AudibleChangedReasons : uint32_t {
    eVolumeChanged = 0,
    eDataAudibleChanged = 1,
    ePauseStateChanged = 2
  };

  /**
   * Returns the AudioChannelServce singleton.
   * If AudioChannelService doesn't exist, create and return new one.
   * Only to be called from main thread.
   */
  static already_AddRefed<AudioChannelService> GetOrCreate();

  /**
   * Returns the AudioChannelService singleton if one exists.
   * If AudioChannelService doesn't exist, returns null.
   */
  static already_AddRefed<AudioChannelService> Get();

  static LogModule* GetAudioChannelLog();

  static bool IsEnableAudioCompeting();

  /**
   * Any audio channel agent that starts playing should register itself to
   * this service, sharing the AudioChannel.
   */
  void RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                 AudibleState aAudible);

  /**
   * Any audio channel agent that stops playing should unregister itself to
   * this service.
   */
  void UnregisterAudioChannelAgent(AudioChannelAgent* aAgent);

  /**
   * Return the state to indicate this audioChannel for his window should keep
   * playing/muted/suspended.
   */
  AudioPlaybackConfig GetMediaConfig(nsPIDOMWindowOuter* aWindow) const;

  /**
   * Called this method when the audible state of the audio playback changed,
   * it would dispatch the playback event to observers which want to know the
   * actual audible state of the window.
   */
  void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible,
                           AudibleChangedReasons aReason);

  bool IsWindowActive(nsPIDOMWindowOuter* aWindow);

  void RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow, float aVolume,
                           bool aMuted);
  void RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                            nsSuspendedTypes aSuspend);

  // This method needs to know the inner window that wants to capture audio. We
  // group agents per top outer window, but we can have multiple innerWindow per
  // top outerWindow (subiframes, etc.) and we have to identify all the agents
  // just for a particular innerWindow.
  void SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                              uint64_t aInnerWindowID, bool aCapture);

  void NotifyMediaResumedFromBlock(nsPIDOMWindowOuter* aWindow);

 private:
  AudioChannelService();
  ~AudioChannelService();

  void RefreshAgents(nsPIDOMWindowOuter* aWindow,
                     const std::function<void(AudioChannelAgent*)>& aFunc);

  static void CreateServiceIfNeeded();

  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

  void RefreshAgentsAudioFocusChanged(AudioChannelAgent* aAgent);

  class AudioChannelWindow final {
   public:
    explicit AudioChannelWindow(uint64_t aWindowID)
        : mWindowID(aWindowID),
          mIsAudioCaptured(false),
          mShouldSendActiveMediaBlockStopEvent(false) {}

    void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible,
                             AudibleChangedReasons aReason);

    void AppendAgent(AudioChannelAgent* aAgent, AudibleState aAudible);
    void RemoveAgent(AudioChannelAgent* aAgent);

    void NotifyMediaBlockStop(nsPIDOMWindowOuter* aWindow);

    uint64_t mWindowID;
    bool mIsAudioCaptured;
    AudioPlaybackConfig mConfig;

    // Raw pointer because the AudioChannelAgent must unregister itself.
    nsTObserverArray<AudioChannelAgent*> mAgents;
    nsTObserverArray<AudioChannelAgent*> mAudibleAgents;

    // If we've dispatched "activeMediaBlockStart" event, we must dispatch
    // another event "activeMediablockStop" when the window is resumed from
    // suspend-block.
    bool mShouldSendActiveMediaBlockStopEvent;

   private:
    void AppendAudibleAgentIfNotContained(AudioChannelAgent* aAgent,
                                          AudibleChangedReasons aReason);
    void RemoveAudibleAgentIfContained(AudioChannelAgent* aAgent,
                                       AudibleChangedReasons aReason);

    void AppendAgentAndIncreaseAgentsNum(AudioChannelAgent* aAgent);
    void RemoveAgentAndReduceAgentsNum(AudioChannelAgent* aAgent);

    bool IsFirstAudibleAgent() const;
    bool IsLastAudibleAgent() const;

    void NotifyAudioAudibleChanged(nsPIDOMWindowOuter* aWindow,
                                   AudibleState aAudible,
                                   AudibleChangedReasons aReason);

    void MaybeNotifyMediaBlockStart(AudioChannelAgent* aAgent);
  };

  AudioChannelWindow* GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow);

  AudioChannelWindow* GetWindowData(uint64_t aWindowID) const;

  nsTObserverArray<UniquePtr<AudioChannelWindow>> mWindows;
};

const char* SuspendTypeToStr(const nsSuspendedTypes& aSuspend);
const char* AudibleStateToStr(
    const AudioChannelService::AudibleState& aAudible);
const char* AudibleChangedReasonToStr(
    const AudioChannelService::AudibleChangedReasons& aReason);

}  // namespace dom
}  // namespace mozilla

#endif
