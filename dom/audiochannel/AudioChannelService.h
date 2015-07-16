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

class nsIRunnable;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
#ifdef MOZ_WIDGET_GONK
class SpeakerManagerService;
#endif

#define NUMBER_OF_AUDIO_CHANNELS (uint32_t)AudioChannel::Publicnotification + 1

class AudioChannelService final : public nsIAudioChannelService
                                , public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIAUDIOCHANNELSERVICE

  /**
   * Returns the AudioChannelServce singleton.
   * If AudioChannelServce is not exist, create and return new one.
   * Only to be called from main thread.
   */
  static already_AddRefed<AudioChannelService> GetOrCreate();

  static bool IsAudioChannelMutedByDefault();

  /**
   * Any audio channel agent that starts playing should register itself to
   * this service, sharing the AudioChannel.
   */
  void RegisterAudioChannelAgent(AudioChannelAgent* aAgent, AudioChannel aChannel);

  /**
   * Any audio channel agent that stops playing should unregister itself to
   * this service.
   */
  void UnregisterAudioChannelAgent(AudioChannelAgent* aAgent);

  /**
   * Return the state to indicate this audioChannel for his window should keep
   * playing/muted.
   */
  void GetState(nsPIDOMWindow* aWindow, uint32_t aChannel,
                float* aVolume, bool* aMuted);

  /* Methods for the BrowserElementAudioChannel */
  float GetAudioChannelVolume(nsPIDOMWindow* aWindow, AudioChannel aChannel);

  void SetAudioChannelVolume(nsPIDOMWindow* aWindow, AudioChannel aChannel,
                             float aVolume);

  bool GetAudioChannelMuted(nsPIDOMWindow* aWindow, AudioChannel aChannel);

  void SetAudioChannelMuted(nsPIDOMWindow* aWindow, AudioChannel aChannel,
                            bool aMuted);

  bool IsAudioChannelActive(nsPIDOMWindow* aWindow, AudioChannel aChannel);

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

  void RefreshAgentsVolume(nsPIDOMWindow* aWindow);

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

  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

  void MaybeSendStatusUpdate();

  bool ContentOrNormalChannelIsActive();

  /* Send the default-volume-channel-changed notification */
  void SetDefaultVolumeControlChannelInternal(int32_t aChannel,
                                              bool aVisible, uint64_t aChildID);

  struct AudioChannelConfig final
  {
    AudioChannelConfig()
      : mVolume(1.0)
      , mMuted(IsAudioChannelMutedByDefault())
      , mNumberOfAgents(0)
    {}

    float mVolume;
    bool mMuted;

    uint32_t mNumberOfAgents;
  };

  struct AudioChannelWindow final
  {
    explicit AudioChannelWindow(uint64_t aWindowID)
      : mWindowID(aWindowID)
    {}

    uint64_t mWindowID;
    AudioChannelConfig mChannels[NUMBER_OF_AUDIO_CHANNELS];

    // Raw pointer because the AudioChannelAgent must unregister itself.
    nsTObserverArray<AudioChannelAgent*> mAgents;
  };

  AudioChannelWindow*
  GetOrCreateWindowData(nsPIDOMWindow* aWindow);

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
