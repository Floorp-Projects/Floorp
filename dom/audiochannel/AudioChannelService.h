/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservice_h__
#define mozilla_dom_audiochannelservice_h__

#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"

#include "AudioChannelAgent.h"
#include "nsAttrValue.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Logging.h"

#include <functional>

class nsPIDOMWindowOuter;
struct PRLogModuleInfo;

namespace mozilla {
namespace dom {

#define NUMBER_OF_AUDIO_CHANNELS (uint32_t)AudioChannel::EndGuard_

class AudioPlaybackConfig
{
public:
  AudioPlaybackConfig()
    : mVolume(1.0)
    , mMuted(false)
    , mSuspend(nsISuspendedTypes::NONE_SUSPENDED)
  {}

  AudioPlaybackConfig(float aVolume, bool aMuted, uint32_t aSuspended)
    : mVolume(aVolume)
    , mMuted(aMuted)
    , mSuspend(aSuspended)
  {}

  void SetConfig(float aVolume, bool aMuted, uint32_t aSuspended)
  {
    mVolume = aVolume;
    mMuted = aMuted;
    mSuspend = aSuspended;
  }

  float mVolume;
  bool mMuted;
  uint32_t mSuspend;
};

class AudioChannelService final : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /**
   * eNotAudible : agent is not audible
   * eMaybeAudible : agent is not audible now, but it might be audible later
   * eAudible : agent is audible now
   */
  enum AudibleState : uint8_t {
    eNotAudible = 0,
    eMaybeAudible = 1,
    eAudible = 2
  };

  enum AudioCaptureState : bool {
    eCapturing = true,
    eNotCapturing = false
  };

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
  AudioPlaybackConfig GetMediaConfig(nsPIDOMWindowOuter* aWindow,
                                     uint32_t aAudioChannel) const;

  /**
   * Called this method when the audible state of the audio playback changed,
   * it would dispatch the playback event to observers which want to know the
   * actual audible state of the window.
   */
  void AudioAudibleChanged(AudioChannelAgent* aAgent,
                           AudibleState aAudible,
                           AudibleChangedReasons aReason);

  bool IsWindowActive(nsPIDOMWindowOuter* aWindow);

  void RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow);
  void RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                            nsSuspendedTypes aSuspend);

  // This method needs to know the inner window that wants to capture audio. We
  // group agents per top outer window, but we can have multiple innerWindow per
  // top outerWindow (subiframes, etc.) and we have to identify all the agents
  // just for a particular innerWindow.
  void SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                              uint64_t aInnerWindowID,
                              bool aCapture);

  static const nsAttrValue::EnumTable* GetAudioChannelTable();
  static AudioChannel GetAudioChannel(const nsAString& aString);
  static AudioChannel GetDefaultAudioChannel();

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

  class AudioChannelConfig final : public AudioPlaybackConfig
  {
  public:
    AudioChannelConfig()
      : AudioPlaybackConfig(1.0, false, nsISuspendedTypes::NONE_SUSPENDED)
      , mNumberOfAgents(0)
    {}

    uint32_t mNumberOfAgents;
  };

  class AudioChannelWindow final
  {
  public:
    explicit AudioChannelWindow(uint64_t aWindowID)
      : mWindowID(aWindowID)
      , mIsAudioCaptured(false)
      , mOwningAudioFocus(!AudioChannelService::IsEnableAudioCompeting())
      , mShouldSendActiveMediaBlockStopEvent(false)
    {}

    void AudioFocusChanged(AudioChannelAgent* aNewPlayingAgent);
    void AudioAudibleChanged(AudioChannelAgent* aAgent,
                             AudibleState aAudible,
                             AudibleChangedReasons aReason);

    void AppendAgent(AudioChannelAgent* aAgent, AudibleState aAudible);
    void RemoveAgent(AudioChannelAgent* aAgent);

    void NotifyMediaBlockStop(nsPIDOMWindowOuter* aWindow);

    uint64_t mWindowID;
    bool mIsAudioCaptured;
    AudioChannelConfig mChannels[NUMBER_OF_AUDIO_CHANNELS];

    // Raw pointer because the AudioChannelAgent must unregister itself.
    nsTObserverArray<AudioChannelAgent*> mAgents;
    nsTObserverArray<AudioChannelAgent*> mAudibleAgents;

    // Owning audio focus when the window starts playing audible sound, and
    // lose audio focus when other windows starts playing.
    bool mOwningAudioFocus;

    // If we've dispatched "activeMediaBlockStart" event, we must dispatch
    // another event "activeMediablockStop" when the window is resumed from
    // suspend-block.
    bool mShouldSendActiveMediaBlockStopEvent;
  private:
    void AudioCapturedChanged(AudioChannelAgent* aAgent,
                              AudioCaptureState aCapture);

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

    void NotifyChannelActive(uint64_t aWindowID, bool aActive);
    void MaybeNotifyMediaBlockStart(AudioChannelAgent* aAgent);

    void RequestAudioFocus(AudioChannelAgent* aAgent);

    // We need to do audio competing only when the new incoming agent started.
    void NotifyAudioCompetingChanged(AudioChannelAgent* aAgent);

    uint32_t GetCompetingBehavior(AudioChannelAgent* aAgent,
                                  int32_t aIncomingChannelType) const;
    bool IsAgentInvolvingInAudioCompeting(AudioChannelAgent* aAgent) const;
    bool IsAudioCompetingInSameTab() const;
    bool IsContainingPlayingAgent(AudioChannelAgent* aAgent) const;

    bool IsInactiveWindow() const;
  };

  AudioChannelWindow*
  GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow);

  AudioChannelWindow*
  GetWindowData(uint64_t aWindowID) const;

  nsTObserverArray<nsAutoPtr<AudioChannelWindow>> mWindows;
};

const char* SuspendTypeToStr(const nsSuspendedTypes& aSuspend);
const char* AudibleStateToStr(const AudioChannelService::AudibleState& aAudible);
const char* AudibleChangedReasonToStr(const AudioChannelService::AudibleChangedReasons& aReason);

} // namespace dom
} // namespace mozilla

#endif
