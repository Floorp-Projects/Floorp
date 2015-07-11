/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"

#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "nsHashPropertyBag.h"
#include "nsComponentManagerUtils.h"
#include "nsPIDOMWindow.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/SettingChangeNotificationBinding.h"

#ifdef MOZ_WIDGET_GONK
#include "nsJSUtils.h"
#include "nsIAudioManager.h"
#include "SpeakerManagerService.h"
#define NS_AUDIOMANAGER_CONTRACTID "@mozilla.org/telephony/audiomanager;1"
#endif

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace {

void
NotifyChannelActive(uint64_t aWindowID, AudioChannel aAudioChannel,
                    bool aActive)
{
  nsCOMPtr<nsIObserverService> observerService =
    services::GetObserverService();
  if (NS_WARN_IF(!observerService)) {
    return;
  }

  nsCOMPtr<nsISupportsPRUint64> wrapper =
    do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
  if (!wrapper) {
     return;
  }

  wrapper->SetData(aWindowID);

  nsAutoString name;
  AudioChannelService::GetAudioChannelString(aAudioChannel, name);

  nsAutoCString topic;
  topic.Assign("audiochannel-activity-");
  topic.Append(NS_ConvertUTF16toUTF8(name));

  observerService->NotifyObservers(wrapper, topic.get(),
                                   aActive
                                     ? MOZ_UTF16("active") : MOZ_UTF16("inactive"));
}

already_AddRefed<nsPIDOMWindow>
GetTopWindow(nsIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIDOMWindow> topWindow;
  aWindow->GetScriptableTop(getter_AddRefs(topWindow));
  MOZ_ASSERT(topWindow);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(topWindow);
  window = window->GetOuterWindow();

  return window.forget();
}

bool
IsParentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

} // anonymous namespace

StaticRefPtr<AudioChannelService> gAudioChannelService;

// Mappings from 'mozaudiochannel' attribute strings to an enumeration.
static const nsAttrValue::EnumTable kMozAudioChannelAttributeTable[] = {
  { "normal",             (int16_t)AudioChannel::Normal },
  { "content",            (int16_t)AudioChannel::Content },
  { "notification",       (int16_t)AudioChannel::Notification },
  { "alarm",              (int16_t)AudioChannel::Alarm },
  { "telephony",          (int16_t)AudioChannel::Telephony },
  { "ringer",             (int16_t)AudioChannel::Ringer },
  { "publicnotification", (int16_t)AudioChannel::Publicnotification },
  { nullptr }
};

/* static */ already_AddRefed<AudioChannelService>
AudioChannelService::GetOrCreate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gAudioChannelService) {
    gAudioChannelService = new AudioChannelService();
  }

  nsRefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

void
AudioChannelService::Shutdown()
{
  if (gAudioChannelService) {
    if (IsParentProcess()) {
      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      if (obs) {
        obs->RemoveObserver(gAudioChannelService, "ipc:content-shutdown");
        obs->RemoveObserver(gAudioChannelService, "xpcom-shutdown");
        obs->RemoveObserver(gAudioChannelService, "inner-window-destroyed");
#ifdef MOZ_WIDGET_GONK
        // To monitor the volume settings based on audio channel.
        obs->RemoveObserver(gAudioChannelService, "mozsettings-changed");
#endif
      }
    }

    gAudioChannelService = nullptr;
  }
}

NS_INTERFACE_MAP_BEGIN(AudioChannelService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAudioChannelService)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(AudioChannelService)
NS_IMPL_RELEASE(AudioChannelService)

AudioChannelService::AudioChannelService()
  : mDisabled(false)
  , mDefChannelChildID(CONTENT_PROCESS_ID_UNKNOWN)
  , mTelephonyChannel(false)
  , mContentOrNormalChannel(false)
  , mAnyChannel(false)
{
  if (IsParentProcess()) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "ipc:content-shutdown", false);
      obs->AddObserver(this, "xpcom-shutdown", false);
      obs->AddObserver(this, "inner-window-destroyed", false);
#ifdef MOZ_WIDGET_GONK
      // To monitor the volume settings based on audio channel.
      obs->AddObserver(this, "mozsettings-changed", false);
#endif
    }
  }
}

AudioChannelService::~AudioChannelService()
{
}

void
AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                               AudioChannel aChannel)
{
  if (mDisabled) {
    return;
  }

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = mWindows.LookupOrAdd(windowID);

  MOZ_ASSERT(!winData->mAgents.Get(aAgent));

  AudioChannel* audioChannel = new AudioChannel(aChannel);
  winData->mAgents.Put(aAgent, audioChannel);

  ++winData->mChannels[(uint32_t)aChannel].mNumberOfAgents;

  // The first one, we must inform the BrowserElementAudioChannel.
  if (winData->mChannels[(uint32_t)aChannel].mNumberOfAgents == 1) {
    NotifyChannelActive(aAgent->WindowID(), aChannel, true);
  }

  // If this is the first agent for this window, we must notify the observers.
  if (winData->mAgents.Count() == 1) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aAgent->Window()),
                                       "media-playback",
                                       NS_LITERAL_STRING("active").get());
    }
  }

  MaybeSendStatusUpdate();
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  if (mDisabled) {
    return;
  }

  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = nullptr;
  if (!mWindows.Get(windowID, &winData)) {
    return;
  }

  nsAutoPtr<AudioChannel> audioChannel;
  winData->mAgents.RemoveAndForget(aAgent, audioChannel);
  if (audioChannel) {
    MOZ_ASSERT(winData->mChannels[(uint32_t)*audioChannel].mNumberOfAgents > 0);

    --winData->mChannels[(uint32_t)*audioChannel].mNumberOfAgents;

    // The last one, we must inform the BrowserElementAudioChannel.
    if (winData->mChannels[(uint32_t)*audioChannel].mNumberOfAgents == 0) {
      NotifyChannelActive(aAgent->WindowID(), *audioChannel, false);
    }
  }

#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive();
  for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
    mSpeakerManager[i]->SetAudioChannelActive(active);
  }
#endif

  // If this is the last agent for this window, we must notify the observers.
  if (winData->mAgents.Count() == 0) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aAgent->Window()),
                                       "media-playback",
                                       NS_LITERAL_STRING("inactive").get());
    }
  }

  MaybeSendStatusUpdate();
}

void
AudioChannelService::GetState(nsPIDOMWindow* aWindow, uint32_t aAudioChannel,
                              float* aVolume, bool* aMuted)
{
  MOZ_ASSERT(!aWindow || aWindow->IsOuterWindow());
  MOZ_ASSERT(aVolume && aMuted);
  MOZ_ASSERT(aAudioChannel < NUMBER_OF_AUDIO_CHANNELS);

  *aVolume = 1.0;
  *aMuted = false;

  if (!aWindow || !aWindow->IsOuterWindow()) {
    return;
  }

  AudioChannelWindow* winData = nullptr;
  nsCOMPtr<nsPIDOMWindow> window = aWindow;

  // The volume must be calculated based on the window hierarchy. Here we go up
  // to the top window and we calculate the volume and the muted flag.
  do {
    if (mWindows.Get(window->WindowID(), &winData)) {
      *aVolume *= winData->mChannels[aAudioChannel].mVolume;
      *aMuted = *aMuted || winData->mChannels[aAudioChannel].mMuted;
    }

    *aVolume *= window->GetAudioVolume();
    *aMuted = *aMuted || window->GetAudioMuted();

    nsCOMPtr<nsIDOMWindow> win;
    window->GetScriptableParent(getter_AddRefs(win));
    if (window == win) {
      break;
    }

    window = do_QueryInterface(win);

    // If there is no parent, or we are the toplevel we don't continue.
  } while (window && window != aWindow);
}

PLDHashOperator
AudioChannelService::TelephonyChannelIsActiveEnumerator(
                                        const uint64_t& aWindowID,
                                        nsAutoPtr<AudioChannelWindow>& aWinData,
                                        void* aPtr)
{
  bool* isActive = static_cast<bool*>(aPtr);
  *isActive =
    aWinData->mChannels[(uint32_t)AudioChannel::Telephony].mNumberOfAgents != 0 &&
    !aWinData->mChannels[(uint32_t)AudioChannel::Telephony].mMuted;
  return *isActive ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

PLDHashOperator
AudioChannelService::TelephonyChannelIsActiveInChildrenEnumerator(
                                      const uint64_t& aChildID,
                                      nsAutoPtr<AudioChannelChildStatus>& aData,
                                      void* aPtr)
{
  bool* isActive = static_cast<bool*>(aPtr);
  *isActive = aData->mActiveTelephonyChannel;
  return *isActive ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

bool
AudioChannelService::TelephonyChannelIsActive()
{
  bool active = false;
  mWindows.Enumerate(TelephonyChannelIsActiveEnumerator, &active);

  if (!active && IsParentProcess()) {
    mPlayingChildren.Enumerate(TelephonyChannelIsActiveInChildrenEnumerator,
                               &active);
  }

  return active;
}

PLDHashOperator
AudioChannelService::ContentOrNormalChannelIsActiveEnumerator(
                                        const uint64_t& aWindowID,
                                        nsAutoPtr<AudioChannelWindow>& aWinData,
                                        void* aPtr)
{
  bool* isActive = static_cast<bool*>(aPtr);
  *isActive =
    aWinData->mChannels[(uint32_t)AudioChannel::Content].mNumberOfAgents > 0 ||
    aWinData->mChannels[(uint32_t)AudioChannel::Normal].mNumberOfAgents > 0;
  return *isActive ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

bool
AudioChannelService::ContentOrNormalChannelIsActive()
{
  // This method is meant to be used just by the child to send status update.
  MOZ_ASSERT(!IsParentProcess());

  bool active = false;
  mWindows.Enumerate(ContentOrNormalChannelIsActiveEnumerator, &active);
  return active;
}

bool
AudioChannelService::ProcessContentOrNormalChannelIsActive(uint64_t aChildID)
{
  AudioChannelChildStatus* status = mPlayingChildren.Get(aChildID);
  if (!status) {
    return false;
  }

  return status->mActiveContentOrNormalChannel;
}

PLDHashOperator
AudioChannelService::AnyAudioChannelIsActiveEnumerator(
                                        const uint64_t& aWindowID,
                                        nsAutoPtr<AudioChannelWindow>& aWinData,
                                        void* aPtr)
{
  bool* isActive = static_cast<bool*>(aPtr);
  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (aWinData->mChannels[kMozAudioChannelAttributeTable[i].value].mNumberOfAgents
        != 0) {
      *isActive = true;
      break;
    }
  }

  return *isActive ? PL_DHASH_STOP : PL_DHASH_NEXT;
}

bool
AudioChannelService::AnyAudioChannelIsActive()
{
  bool active = false;
  mWindows.Enumerate(AnyAudioChannelIsActiveEnumerator, &active);

  if (!active && IsParentProcess()) {
    active = !!mPlayingChildren.Count();
  }

  return active;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    mDisabled = true;
    mWindows.Clear();
  }

#ifdef MOZ_WIDGET_GONK
  // To process the volume control on each audio channel according to
  // change of settings
  else if (!strcmp(aTopic, "mozsettings-changed")) {
    RootedDictionary<SettingChangeNotification> setting(nsContentUtils::RootingCxForThread());
    if (!WrappedJSToDictionary(aSubject, setting)) {
      return NS_OK;
    }
    if (!StringBeginsWith(setting.mKey, NS_LITERAL_STRING("audio.volume."))) {
      return NS_OK;
    }
    if (!setting.mValue.isNumber()) {
      return NS_OK;
    }

    nsCOMPtr<nsIAudioManager> audioManager = do_GetService(NS_AUDIOMANAGER_CONTRACTID);
    NS_ENSURE_TRUE(audioManager, NS_OK);

    int32_t index = setting.mValue.toNumber();
    if (setting.mKey.EqualsLiteral("audio.volume.content")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Content, index);
    } else if (setting.mKey.EqualsLiteral("audio.volume.notification")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Notification, index);
    } else if (setting.mKey.EqualsLiteral("audio.volume.alarm")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Alarm, index);
    } else if (setting.mKey.EqualsLiteral("audio.volume.telephony")) {
      audioManager->SetAudioChannelVolume((int32_t)AudioChannel::Telephony, index);
    } else if (!setting.mKey.EqualsLiteral("audio.volume.bt_sco")) {
      // bt_sco is not a valid audio channel so we manipulate it in
      // AudioManager.cpp. And the others should not be used.
      // We didn't use MOZ_CRASH or MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE here
      // because any web content who has permission of mozSettings can set any
      // names then it can be easy to crash the B2G.
      NS_WARNING("unexpected audio channel for volume control");
    }
  }
#endif

  else if (!strcmp(aTopic, "inner-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t innerID;
    nsresult rv = wrapper->GetData(&innerID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoPtr<AudioChannelWindow> window;
    mWindows.RemoveAndForget(innerID, window);
    if (window) {
      window->mAgents.EnumerateRead(NotifyEnumerator, nullptr);
    }

#ifdef MOZ_WIDGET_GONK
    bool active = AnyAudioChannelIsActive();
    for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
      mSpeakerManager[i]->SetAudioChannelActive(active);
    }
#endif
  }

  else if (!strcmp(aTopic, "ipc:content-shutdown")) {
    nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
    if (!props) {
      NS_WARNING("ipc:content-shutdown message without property bag as subject");
      return NS_OK;
    }

    uint64_t childID = 0;
    nsresult rv = props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"),
                                             &childID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (mDefChannelChildID == childID) {
      SetDefaultVolumeControlChannelInternal(-1, false, childID);
      mDefChannelChildID = CONTENT_PROCESS_ID_UNKNOWN;
    }

    mPlayingChildren.Remove(childID);
  }

  return NS_OK;
}

struct RefreshAgentsVolumeData
{
  explicit RefreshAgentsVolumeData(nsPIDOMWindow* aWindow)
    : mWindow(aWindow)
  {}

  nsPIDOMWindow* mWindow;
  nsTArray<nsRefPtr<AudioChannelAgent>> mAgents;
};

PLDHashOperator
AudioChannelService::RefreshAgentsVolumeEnumerator(
                                                 AudioChannelAgent* aAgent,
                                                 AudioChannel* aUnused,
                                                 void* aPtr)
{
  MOZ_ASSERT(aAgent);
  aAgent->WindowVolumeChanged();
  return PL_DHASH_NEXT;
}
void
AudioChannelService::RefreshAgentsVolume(nsPIDOMWindow* aWindow)
{
  AudioChannelWindow* winData = mWindows.Get(aWindow->WindowID());
  if (!winData) {
    return;
  }

  winData->mAgents.EnumerateRead(RefreshAgentsVolumeEnumerator, nullptr);
}

/* static */ const nsAttrValue::EnumTable*
AudioChannelService::GetAudioChannelTable()
{
  return kMozAudioChannelAttributeTable;
}

/* static */ AudioChannel
AudioChannelService::GetAudioChannel(const nsAString& aChannel)
{
  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (aChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
      return static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value);
    }
  }

  return AudioChannel::Normal;
}

/* static */ AudioChannel
AudioChannelService::GetDefaultAudioChannel()
{
  nsAutoString audioChannel(Preferences::GetString("media.defaultAudioChannel"));
  if (audioChannel.IsEmpty()) {
    return AudioChannel::Normal;
  }

  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (audioChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
      return static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value);
    }
  }

  return AudioChannel::Normal;
}

/* static */ void
AudioChannelService::GetAudioChannelString(AudioChannel aChannel,
                                           nsAString& aString)
{
  aString.AssignASCII("normal");

  for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
    if (aChannel ==
        static_cast<AudioChannel>(kMozAudioChannelAttributeTable[i].value)) {
      aString.AssignASCII(kMozAudioChannelAttributeTable[i].tag);
      break;
    }
  }
}

/* static */ void
AudioChannelService::GetDefaultAudioChannelString(nsAString& aString)
{
  aString.AssignASCII("normal");

  nsAutoString audioChannel(Preferences::GetString("media.defaultAudioChannel"));
  if (!audioChannel.IsEmpty()) {
    for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
      if (audioChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
        aString = audioChannel;
        break;
      }
    }
  }
}

AudioChannelService::AudioChannelWindow&
AudioChannelService::GetOrCreateWindowData(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow* winData = mWindows.LookupOrAdd(aWindow->WindowID());
  return *winData;
}

float
AudioChannelService::GetAudioChannelVolume(nsPIDOMWindow* aWindow,
                                           AudioChannel aAudioChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow& winData = GetOrCreateWindowData(aWindow);
  return winData.mChannels[(uint32_t)aAudioChannel].mVolume;
}

NS_IMETHODIMP
AudioChannelService::GetAudioChannelVolume(nsIDOMWindow* aWindow,
                                           unsigned short aAudioChannel,
                                           float* aVolume)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(aWindow);
  *aVolume = GetAudioChannelVolume(window, (AudioChannel)aAudioChannel);
  return NS_OK;
}

void
AudioChannelService::SetAudioChannelVolume(nsPIDOMWindow* aWindow,
                                           AudioChannel aAudioChannel,
                                           float aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow& winData = GetOrCreateWindowData(aWindow);
  winData.mChannels[(uint32_t)aAudioChannel].mVolume = aVolume;
  RefreshAgentsVolume(aWindow);
}

NS_IMETHODIMP
AudioChannelService::SetAudioChannelVolume(nsIDOMWindow* aWindow,
                                           unsigned short aAudioChannel,
                                           float aVolume)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(aWindow);
  SetAudioChannelVolume(window, (AudioChannel)aAudioChannel, aVolume);
  return NS_OK;
}

bool
AudioChannelService::GetAudioChannelMuted(nsPIDOMWindow* aWindow,
                                          AudioChannel aAudioChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow& winData = GetOrCreateWindowData(aWindow);
  return winData.mChannels[(uint32_t)aAudioChannel].mMuted;
}

NS_IMETHODIMP
AudioChannelService::GetAudioChannelMuted(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool* aMuted)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(aWindow);
  *aMuted = GetAudioChannelMuted(window, (AudioChannel)aAudioChannel);
  return NS_OK;
}

void
AudioChannelService::SetAudioChannelMuted(nsPIDOMWindow* aWindow,
                                          AudioChannel aAudioChannel,
                                          bool aMuted)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow& winData = GetOrCreateWindowData(aWindow);
  winData.mChannels[(uint32_t)aAudioChannel].mMuted = aMuted;
  RefreshAgentsVolume(aWindow);
}

NS_IMETHODIMP
AudioChannelService::SetAudioChannelMuted(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool aMuted)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(aWindow);
  SetAudioChannelMuted(window, (AudioChannel)aAudioChannel, aMuted);
  return NS_OK;
}

bool
AudioChannelService::IsAudioChannelActive(nsPIDOMWindow* aWindow,
                                          AudioChannel aAudioChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow& winData = GetOrCreateWindowData(aWindow);
  return !!winData.mChannels[(uint32_t)aAudioChannel].mNumberOfAgents;
}

NS_IMETHODIMP
AudioChannelService::IsAudioChannelActive(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool* aActive)
{
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(aWindow);
  *aActive = IsAudioChannelActive(window, (AudioChannel)aAudioChannel);
  return NS_OK;
}
void
AudioChannelService::SetDefaultVolumeControlChannel(int32_t aChannel,
                                                    bool aVisible)
{
  SetDefaultVolumeControlChannelInternal(aChannel, aVisible,
                                         CONTENT_PROCESS_ID_MAIN);
}

void
AudioChannelService::SetDefaultVolumeControlChannelInternal(int32_t aChannel,
                                                            bool aVisible,
                                                            uint64_t aChildID)
{
  if (!IsParentProcess()) {
    ContentChild* cc = ContentChild::GetSingleton();
    if (cc) {
      cc->SendAudioChannelChangeDefVolChannel(aChannel, aVisible);
    }

    return;
  }

  // If this child is in the background and mDefChannelChildID is set to
  // others then it means other child in the foreground already set it's
  // own default channel.
  if (!aVisible && mDefChannelChildID != aChildID) {
    return;
  }

  // Workaround for the call screen app. The call screen app is running on the
  // main process, that will results in wrong visible state. Because we use the
  // docshell's active state as visible state, the main process is always
  // active. Therefore, we will see the strange situation that the visible
  // state of the call screen is always true. If the mDefChannelChildID is set
  // to others then it means other child in the foreground already set it's
  // own default channel already.
  // Summary :
  //   Child process : foreground app always can set type.
  //   Parent process : check the mDefChannelChildID.
  else if (aChildID == CONTENT_PROCESS_ID_MAIN &&
           mDefChannelChildID != CONTENT_PROCESS_ID_UNKNOWN) {
    return;
  }

  mDefChannelChildID = aVisible ? aChildID : CONTENT_PROCESS_ID_UNKNOWN;
  nsAutoString channelName;

  if (aChannel == -1) {
    channelName.AssignASCII("unknown");
  } else {
    GetAudioChannelString(static_cast<AudioChannel>(aChannel), channelName);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "default-volume-channel-changed",
                         channelName.get());
  }
}

void
AudioChannelService::MaybeSendStatusUpdate()
{
  if (IsParentProcess()) {
    return;
  }

  bool telephonyChannel = TelephonyChannelIsActive();
  bool contentOrNormalChannel = ContentOrNormalChannelIsActive();
  bool anyChannel = AnyAudioChannelIsActive();

  if (telephonyChannel == mTelephonyChannel &&
      contentOrNormalChannel == mContentOrNormalChannel &&
      anyChannel == mAnyChannel) {
    return;
  }

  mTelephonyChannel = telephonyChannel;
  mContentOrNormalChannel = contentOrNormalChannel;
  mAnyChannel = anyChannel;

  ContentChild* cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelServiceStatus(telephonyChannel, contentOrNormalChannel,
                                      anyChannel);
  }
}

void
AudioChannelService::ChildStatusReceived(uint64_t aChildID,
                                         bool aTelephonyChannel,
                                         bool aContentOrNormalChannel,
                                         bool aAnyChannel)
{
  if (!aAnyChannel) {
    mPlayingChildren.Remove(aChildID);
    return;
  }

  AudioChannelChildStatus* data = mPlayingChildren.LookupOrAdd(aChildID);
  data->mActiveTelephonyChannel = aTelephonyChannel;
  data->mActiveContentOrNormalChannel = aContentOrNormalChannel;
}

/* static */ PLDHashOperator
AudioChannelService::NotifyEnumerator(AudioChannelAgent* aAgent,
                                      AudioChannel* aAudioChannel,
                                      void* aUnused)
{
  aAgent->WindowVolumeChanged();
  return PL_DHASH_NEXT;
}

