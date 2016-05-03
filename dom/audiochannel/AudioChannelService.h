/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservice_h__
#define mozilla_dom_audiochannelservice_h__

#include "nsIAudioChannelService.h"
#include "nsAutoPtr.h"
#include "nsIObserver.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"

#include "AudioChannelAgent.h"
#include "nsAttrValue.h"
#include "mozilla/dom/AudioChannelBinding.h"
#include "mozilla/Function.h"

class nsIRunnable;
class nsPIDOMWindowOuter;
struct PRLogModuleInfo;

namespace mozilla {
namespace dom {

#ifdef MOZ_WIDGET_GONK
class SpeakerManagerService;
#endif

class TabParent;

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

class AudioChannelService final : public nsIAudioChannelService
                                , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIAUDIOCHANNELSERVICE

  enum AudibleState : bool {
    eAudible = true,
    eNotAudible = false
  };

  enum AudioCaptureState : bool {
    eCapturing = true,
    eNotCapturing = false
  };

  /**
   * Returns the AudioChannelServce singleton.
   * If AudioChannelServce is not exist, create and return new one.
   * Only to be called from main thread.
   */
  static already_AddRefed<AudioChannelService> GetOrCreate();

  static bool IsAudioChannelMutedByDefault();

  static PRLogModuleInfo* GetAudioChannelLog();

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
   * For nested iframes.
   */
  void RegisterTabParent(TabParent* aTabParent);
  void UnregisterTabParent(TabParent* aTabParent);

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
  void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible);

  /* Methods for the BrowserElementAudioChannel */
  float GetAudioChannelVolume(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel);

  void SetAudioChannelVolume(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
                             float aVolume);

  bool GetAudioChannelMuted(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel);

  void SetAudioChannelMuted(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel,
                            bool aMuted);

  bool IsAudioChannelActive(nsPIDOMWindowOuter* aWindow, AudioChannel aChannel);

  /**
   * Return true if there is a telephony channel active in this process
   * or one of its subprocesses.
   */
  bool TelephonyChannelIsActive();

  /**
   * Return true if a normal or content channel is active for the given
   * process ID.
   */
  bool ProcessContentOrNormalChannelIsActive(uint64_t aChildID);

  /***
   * AudioChannelManager calls this function to notify the default channel used
   * to adjust volume when there is no any active channel. if aChannel is -1,
   * the default audio channel will be used. Otherwise aChannel is casted to
   * AudioChannel enum.
   */
  virtual void SetDefaultVolumeControlChannel(int32_t aChannel,
                                              bool aVisible);

  bool AnyAudioChannelIsActive();

  void RefreshAgentsVolume(nsPIDOMWindowOuter* aWindow);
  void RefreshAgentsSuspend(nsPIDOMWindowOuter* aWindow,
                            nsSuspendedTypes aSuspend);

  void RefreshAgentsVolumeAndPropagate(AudioChannel aAudioChannel,
                                       nsPIDOMWindowOuter* aWindow);

  // This method needs to know the inner window that wants to capture audio. We
  // group agents per top outer window, but we can have multiple innerWindow per
  // top outerWindow (subiframes, etc.) and we have to identify all the agents
  // just for a particular innerWindow.
  void SetWindowAudioCaptured(nsPIDOMWindowOuter* aWindow,
                              uint64_t aInnerWindowID,
                              bool aCapture);

#ifdef MOZ_WIDGET_GONK
  void RegisterSpeakerManager(SpeakerManagerService* aSpeakerManager)
  {
    if (!mSpeakerManager.Contains(aSpeakerManager)) {
      mSpeakerManager.AppendElement(aSpeakerManager);
    }
  }

  void UnregisterSpeakerManager(SpeakerManagerService* aSpeakerManager)
  {
    mSpeakerManager.RemoveElement(aSpeakerManager);
  }
#endif

  static const nsAttrValue::EnumTable* GetAudioChannelTable();
  static AudioChannel GetAudioChannel(const nsAString& aString);
  static AudioChannel GetDefaultAudioChannel();
  static void GetAudioChannelString(AudioChannel aChannel, nsAString& aString);
  static void GetDefaultAudioChannelString(nsAString& aString);

  void Notify(uint64_t aWindowID);

  void ChildStatusReceived(uint64_t aChildID, bool aTelephonyChannel,
                           bool aContentOrNormalChannel, bool aAnyChannel);

private:
  AudioChannelService();
  ~AudioChannelService();

  void RefreshAgents(nsPIDOMWindowOuter* aWindow,
                     mozilla::function<void(AudioChannelAgent*)> aFunc);

  static void CreateServiceIfNeeded();

  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

  void MaybeSendStatusUpdate();

  bool ContentOrNormalChannelIsActive();

  /* Send the default-volume-channel-changed notification */
  void SetDefaultVolumeControlChannelInternal(int32_t aChannel,
                                              bool aVisible, uint64_t aChildID);

  class AudioChannelConfig final : public AudioPlaybackConfig
  {
  public:
    AudioChannelConfig()
      : AudioPlaybackConfig(1.0, IsAudioChannelMutedByDefault(),
                            nsISuspendedTypes::NONE_SUSPENDED)
      , mNumberOfAgents(0)
    {}

    uint32_t mNumberOfAgents;
  };

  class AudioChannelWindow final
  {
  public:
    explicit AudioChannelWindow(uint64_t aWindowID)
      : mWindowID(aWindowID),
        mIsAudioCaptured(false)
    {
      // Workaround for bug1183033, system channel type can always playback.
      mChannels[(int16_t)AudioChannel::System].mMuted = false;
    }

    void AudioAudibleChanged(AudioChannelAgent* aAgent, AudibleState aAudible);

    void AppendAgent(AudioChannelAgent* aAgent, AudibleState aAudible);
    void RemoveAgent(AudioChannelAgent* aAgent);

    uint64_t mWindowID;
    bool mIsAudioCaptured;
    AudioChannelConfig mChannels[NUMBER_OF_AUDIO_CHANNELS];

    // Raw pointer because the AudioChannelAgent must unregister itself.
    nsTObserverArray<AudioChannelAgent*> mAgents;
    nsTObserverArray<AudioChannelAgent*> mAudibleAgents;

  private:
    void AudioCapturedChanged(AudioChannelAgent* aAgent,
                              AudioCaptureState aCapture);

    void AppendAudibleAgentIfNotContained(AudioChannelAgent* aAgent);
    void RemoveAudibleAgentIfContained(AudioChannelAgent* aAgent);

    void AppendAgentAndIncreaseAgentsNum(AudioChannelAgent* aAgent);
    void RemoveAgentAndReduceAgentsNum(AudioChannelAgent* aAgent);

    bool IsFirstAudibleAgent() const;
    bool IsLastAudibleAgent() const;

    void NotifyAudioAudibleChanged(nsPIDOMWindowOuter* aWindow,
                                   AudibleState aAudible);
    void NotifyChannelActive(uint64_t aWindowID, AudioChannel aChannel,
                             bool aActive);
  };

  AudioChannelWindow*
  GetOrCreateWindowData(nsPIDOMWindowOuter* aWindow);

  AudioChannelWindow*
  GetWindowData(uint64_t aWindowID) const;

  struct AudioChannelChildStatus final
  {
    explicit AudioChannelChildStatus(uint64_t aChildID)
      : mChildID(aChildID)
      , mActiveTelephonyChannel(false)
      , mActiveContentOrNormalChannel(false)
    {}

    uint64_t mChildID;
    bool mActiveTelephonyChannel;
    bool mActiveContentOrNormalChannel;
  };

  AudioChannelChildStatus*
  GetChildStatus(uint64_t aChildID) const;

  void
  RemoveChildStatus(uint64_t aChildID);

  nsTObserverArray<nsAutoPtr<AudioChannelWindow>> mWindows;

  nsTObserverArray<nsAutoPtr<AudioChannelChildStatus>> mPlayingChildren;

#ifdef MOZ_WIDGET_GONK
  nsTArray<SpeakerManagerService*>  mSpeakerManager;
#endif

  // Raw pointers because TabParents must unregister themselves.
  nsTArray<TabParent*> mTabParents;

  nsCOMPtr<nsIRunnable> mRunnable;

  uint64_t mDefChannelChildID;

  // These boolean are used to know if we have to send an status update to the
  // service running in the main process.
  bool mTelephonyChannel;
  bool mContentOrNormalChannel;
  bool mAnyChannel;

  // This is needed for IPC comunication between
  // AudioChannelServiceChild and this class.
  friend class ContentParent;
  friend class ContentChild;
};

} // namespace dom
} // namespace mozilla

#endif
