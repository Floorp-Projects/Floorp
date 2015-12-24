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
#include "mozilla/dom/TabParent.h"

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
#include "SpeakerManagerService.h"
#endif

#include "mozilla/Preferences.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

namespace {

// If true, any new AudioChannelAgent will be muted when created.
bool sAudioChannelMutedByDefault = false;
bool sXPCOMShuttingDown = false;

class NotifyChannelActiveRunnable final : public nsRunnable
{
public:
  NotifyChannelActiveRunnable(uint64_t aWindowID, AudioChannel aAudioChannel,
                              bool aActive)
    : mWindowID(aWindowID)
    , mAudioChannel(aAudioChannel)
    , mActive(aActive)
  {}

  NS_IMETHOD Run() override
  {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (NS_WARN_IF(!observerService)) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsISupportsPRUint64> wrapper =
      do_CreateInstance(NS_SUPPORTS_PRUINT64_CONTRACTID);
    if (NS_WARN_IF(!wrapper)) {
       return NS_ERROR_FAILURE;
    }

    wrapper->SetData(mWindowID);

    nsAutoString name;
    AudioChannelService::GetAudioChannelString(mAudioChannel, name);

    nsAutoCString topic;
    topic.Assign("audiochannel-activity-");
    topic.Append(NS_ConvertUTF16toUTF8(name));

    observerService->NotifyObservers(wrapper, topic.get(),
                                     mActive
                                       ? MOZ_UTF16("active")
                                       : MOZ_UTF16("inactive"));

    MOZ_LOG(AudioChannelService::GetAudioChannelLog(), LogLevel::Debug,
           ("NotifyChannelActiveRunnable, type = %d, active = %d\n",
            mAudioChannel, mActive));

    return NS_OK;
  }

private:
  const uint64_t mWindowID;
  const AudioChannel mAudioChannel;
  const bool mActive;
};

void
NotifyChannelActive(uint64_t aWindowID, AudioChannel aAudioChannel,
                    bool aActive)
{
  RefPtr<nsRunnable> runnable =
    new NotifyChannelActiveRunnable(aWindowID, aAudioChannel, aActive);
  NS_DispatchToCurrentThread(runnable);
}

already_AddRefed<nsPIDOMWindow>
GetTopWindow(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsPIDOMWindow> topWindow = aWindow->GetScriptableTop();
  MOZ_ASSERT(topWindow);

  topWindow = topWindow->GetOuterWindow();

  return topWindow.forget();
}

bool
IsParentProcess()
{
  return XRE_GetProcessType() == GeckoProcessType_Default;
}

class MediaPlaybackRunnable : public nsRunnable
{
public:
  MediaPlaybackRunnable(nsIDOMWindow* aWindow, bool aActive)
    : mWindow(aWindow)
    , mActive(aActive)
  {}

 NS_IMETHOD Run()
 {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(
        ToSupports(mWindow),
        "audio-playback",
        mActive ? MOZ_UTF16("active")
                : MOZ_UTF16("inactive"));
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIDOMWindow> mWindow;
  bool mActive;
};

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
  { "system",             (int16_t)AudioChannel::System },
  { nullptr }
};

/* static */ void
AudioChannelService::CreateServiceIfNeeded()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gAudioChannelService) {
    gAudioChannelService = new AudioChannelService();
  }
}

/* static */ already_AddRefed<AudioChannelService>
AudioChannelService::GetOrCreate()
{
  if (sXPCOMShuttingDown) {
    return nullptr;
  }

  CreateServiceIfNeeded();
  RefPtr<AudioChannelService> service = gAudioChannelService.get();
  return service.forget();
}

/* static */ PRLogModuleInfo*
AudioChannelService::GetAudioChannelLog()
{
  static PRLogModuleInfo *gAudioChannelLog;
  if (!gAudioChannelLog) {
    gAudioChannelLog = PR_NewLogModule("AudioChannel");
  }
  return gAudioChannelLog;
}

/* static */ void
AudioChannelService::Shutdown()
{
  if (gAudioChannelService) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(gAudioChannelService, "xpcom-shutdown");
      obs->RemoveObserver(gAudioChannelService, "outer-window-destroyed");

      if (IsParentProcess()) {
        obs->RemoveObserver(gAudioChannelService, "ipc:content-shutdown");

#ifdef MOZ_WIDGET_GONK
        // To monitor the volume settings based on audio channel.
        obs->RemoveObserver(gAudioChannelService, "mozsettings-changed");
#endif
      }
    }

    gAudioChannelService->mWindows.Clear();
    gAudioChannelService->mPlayingChildren.Clear();
    gAudioChannelService->mTabParents.Clear();
#ifdef MOZ_WIDGET_GONK
    gAudioChannelService->mSpeakerManager.Clear();
#endif

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
  : mDefChannelChildID(CONTENT_PROCESS_ID_UNKNOWN)
  , mTelephonyChannel(false)
  , mContentOrNormalChannel(false)
  , mAnyChannel(false)
{
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "xpcom-shutdown", false);
    obs->AddObserver(this, "outer-window-destroyed", false);
    if (IsParentProcess()) {
      obs->AddObserver(this, "ipc:content-shutdown", false);

#ifdef MOZ_WIDGET_GONK
      // To monitor the volume settings based on audio channel.
      obs->AddObserver(this, "mozsettings-changed", false);
#endif
    }
  }

  Preferences::AddBoolVarCache(&sAudioChannelMutedByDefault,
                               "dom.audiochannel.mutedByDefault");
}

AudioChannelService::~AudioChannelService()
{
}

void
AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                               AudioChannel aChannel)
{
  uint64_t windowID = aAgent->WindowID();
  AudioChannelWindow* winData = GetWindowData(windowID);
  if (!winData) {
    winData = new AudioChannelWindow(windowID);
    mWindows.AppendElement(winData);
  }

  MOZ_ASSERT(!winData->mAgents.Contains(aAgent));
  winData->mAgents.AppendElement(aAgent);

  ++winData->mChannels[(uint32_t)aChannel].mNumberOfAgents;

  // The first one, we must inform the BrowserElementAudioChannel.
  if (winData->mChannels[(uint32_t)aChannel].mNumberOfAgents == 1) {
    NotifyChannelActive(aAgent->WindowID(), aChannel, true);
  }

  // If this is the first agent for this window, we must notify the observers.
  if (winData->mAgents.Length() == 1) {
    RefPtr<MediaPlaybackRunnable> runnable =
      new MediaPlaybackRunnable(aAgent->Window(), true /* active */);
    NS_DispatchToCurrentThread(runnable);
  }

  // If the window has already been captured, the agent of that window should
  // also be captured.
  if (winData->mIsAudioCaptured) {
    aAgent->WindowAudioCaptureChanged(aAgent->InnerWindowID(),
                                      winData->mIsAudioCaptured);
  }

  MaybeSendStatusUpdate();
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  AudioChannelWindow* winData = GetWindowData(aAgent->WindowID());
  if (!winData) {
    return;
  }

  if (winData->mAgents.Contains(aAgent)) {
    int32_t channel = aAgent->AudioChannelType();
    uint64_t windowID = aAgent->WindowID();

    // aAgent can be freed after this call.
    winData->mAgents.RemoveElement(aAgent);

    MOZ_ASSERT(winData->mChannels[channel].mNumberOfAgents > 0);

    --winData->mChannels[channel].mNumberOfAgents;

    // The last one, we must inform the BrowserElementAudioChannel.
    if (winData->mChannels[channel].mNumberOfAgents == 0) {
      NotifyChannelActive(windowID, static_cast<AudioChannel>(channel), false);
    }
  }

#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive();
  for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
    mSpeakerManager[i]->SetAudioChannelActive(active);
  }
#endif

  // If this is the last agent for this window, we must notify the observers.
  if (winData->mAgents.IsEmpty()) {
    RefPtr<MediaPlaybackRunnable> runnable =
      new MediaPlaybackRunnable(aAgent->Window(), false /* active */);
    NS_DispatchToCurrentThread(runnable);
  }

  // No need to capture non-audible object.
  if (winData->mIsAudioCaptured) {
    aAgent->WindowAudioCaptureChanged(aAgent->InnerWindowID(), false);
  }

  MaybeSendStatusUpdate();
}

void
AudioChannelService::RegisterTabParent(TabParent* aTabParent)
{
  MOZ_ASSERT(aTabParent);
  MOZ_ASSERT(!mTabParents.Contains(aTabParent));
  mTabParents.AppendElement(aTabParent);
}

void
AudioChannelService::UnregisterTabParent(TabParent* aTabParent)
{
  MOZ_ASSERT(aTabParent);
  mTabParents.RemoveElement(aTabParent);
}

void
AudioChannelService::GetState(nsPIDOMWindow* aWindow, uint32_t aAudioChannel,
                              float* aVolume, bool* aMuted)
{
  MOZ_ASSERT(!aWindow || aWindow->IsOuterWindow());
  MOZ_ASSERT(aVolume && aMuted);
  MOZ_ASSERT(aAudioChannel < NUMBER_OF_AUDIO_CHANNELS);


  if (!aWindow || !aWindow->IsOuterWindow()) {
    *aVolume = 0.0;
    *aMuted = true;
    return;
  }

  *aVolume = 1.0;
  *aMuted = false;

  AudioChannelWindow* winData = nullptr;
  nsCOMPtr<nsPIDOMWindow> window = aWindow;

  // The volume must be calculated based on the window hierarchy. Here we go up
  // to the top window and we calculate the volume and the muted flag.
  do {
    winData = GetWindowData(window->WindowID());
    if (winData) {
      *aVolume *= winData->mChannels[aAudioChannel].mVolume;
      *aMuted = *aMuted || winData->mChannels[aAudioChannel].mMuted;
    }

    *aVolume *= window->GetAudioVolume();
    *aMuted = *aMuted || window->GetAudioMuted();

    nsCOMPtr<nsPIDOMWindow> win = window->GetScriptableParent();
    if (window == win) {
      break;
    }

    window = do_QueryInterface(win);

    // If there is no parent, or we are the toplevel we don't continue.
  } while (window && window != aWindow);
}

bool
AudioChannelService::TelephonyChannelIsActive()
{
  nsTObserverArray<nsAutoPtr<AudioChannelWindow>>::ForwardIterator windowsIter(mWindows);
  while (windowsIter.HasMore()) {
    AudioChannelWindow* next = windowsIter.GetNext();
    if (next->mChannels[(uint32_t)AudioChannel::Telephony].mNumberOfAgents != 0 &&
        !next->mChannels[(uint32_t)AudioChannel::Telephony].mMuted) {
      return true;
    }
  }

  if (IsParentProcess()) {
    nsTObserverArray<nsAutoPtr<AudioChannelChildStatus>>::ForwardIterator
      childrenIter(mPlayingChildren);
    while (childrenIter.HasMore()) {
      AudioChannelChildStatus* child = childrenIter.GetNext();
      if (child->mActiveTelephonyChannel) {
        return true;
      }
    }
  }

  return false;
}

bool
AudioChannelService::ContentOrNormalChannelIsActive()
{
  // This method is meant to be used just by the child to send status update.
  MOZ_ASSERT(!IsParentProcess());

  nsTObserverArray<nsAutoPtr<AudioChannelWindow>>::ForwardIterator iter(mWindows);
  while (iter.HasMore()) {
    AudioChannelWindow* next = iter.GetNext();
    if (next->mChannels[(uint32_t)AudioChannel::Content].mNumberOfAgents > 0 ||
        next->mChannels[(uint32_t)AudioChannel::Normal].mNumberOfAgents > 0) {
      return true;
    }
  }
  return false;
}

AudioChannelService::AudioChannelChildStatus*
AudioChannelService::GetChildStatus(uint64_t aChildID) const
{
  nsTObserverArray<nsAutoPtr<AudioChannelChildStatus>>::ForwardIterator
    iter(mPlayingChildren);
  while (iter.HasMore()) {
    AudioChannelChildStatus* child = iter.GetNext();
    if (child->mChildID == aChildID) {
      return child;
    }
  }

  return nullptr;
}

void
AudioChannelService::RemoveChildStatus(uint64_t aChildID)
{
  nsTObserverArray<nsAutoPtr<AudioChannelChildStatus>>::ForwardIterator
    iter(mPlayingChildren);
  while (iter.HasMore()) {
    nsAutoPtr<AudioChannelChildStatus>& child = iter.GetNext();
    if (child->mChildID == aChildID) {
      mPlayingChildren.RemoveElement(child);
      break;
    }
  }
}

bool
AudioChannelService::ProcessContentOrNormalChannelIsActive(uint64_t aChildID)
{
  AudioChannelChildStatus* child = GetChildStatus(aChildID);
  if (!child) {
    return false;
  }

  return child->mActiveContentOrNormalChannel;
}

bool
AudioChannelService::AnyAudioChannelIsActive()
{
  nsTObserverArray<nsAutoPtr<AudioChannelWindow>>::ForwardIterator iter(mWindows);
  while (iter.HasMore()) {
    AudioChannelWindow* next = iter.GetNext();
    for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
      if (next->mChannels[kMozAudioChannelAttributeTable[i].value].mNumberOfAgents
          != 0) {
        return true;
      }
    }
  }

  if (IsParentProcess()) {
    return !mPlayingChildren.IsEmpty();
  }

  return false;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic,
                             const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    sXPCOMShuttingDown = true;
    Shutdown();
  } else if (!strcmp(aTopic, "outer-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);

    uint64_t outerID;
    nsresult rv = wrapper->GetData(&outerID);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    nsAutoPtr<AudioChannelWindow> winData;
    {
      nsTObserverArray<nsAutoPtr<AudioChannelWindow>>::ForwardIterator
        iter(mWindows);
      while (iter.HasMore()) {
        nsAutoPtr<AudioChannelWindow>& next = iter.GetNext();
        if (next->mWindowID == outerID) {
          uint32_t pos = mWindows.IndexOf(next);
          winData = next.forget();
          mWindows.RemoveElementAt(pos);
          break;
        }
      }
    }

    if (winData) {
      nsTObserverArray<AudioChannelAgent*>::ForwardIterator
        iter(winData->mAgents);
      while (iter.HasMore()) {
        iter.GetNext()->WindowVolumeChanged();
      }
    }

#ifdef MOZ_WIDGET_GONK
    bool active = AnyAudioChannelIsActive();
    for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
      mSpeakerManager[i]->SetAudioChannelActive(active);
    }
#endif
  } else if (!strcmp(aTopic, "ipc:content-shutdown")) {
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

    RemoveChildStatus(childID);
  }

  return NS_OK;
}

void
AudioChannelService::RefreshAgentsVolumeAndPropagate(AudioChannel aAudioChannel,
                                                     nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  nsCOMPtr<nsPIDOMWindow> topWindow = aWindow->GetScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  for (uint32_t i = 0; i < mTabParents.Length(); ++i) {
    mTabParents[i]->AudioChannelChangeNotification(aWindow, aAudioChannel,
                                                   winData->mChannels[(uint32_t)aAudioChannel].mVolume,
                                                   winData->mChannels[(uint32_t)aAudioChannel].mMuted);
  }

  RefreshAgentsVolume(aWindow);
}

void
AudioChannelService::RefreshAgentsVolume(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  nsCOMPtr<nsPIDOMWindow> topWindow = aWindow->GetScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());
  if (!winData) {
    return;
  }

  nsTObserverArray<AudioChannelAgent*>::ForwardIterator
    iter(winData->mAgents);
  while (iter.HasMore()) {
    iter.GetNext()->WindowVolumeChanged();
  }
}

void
AudioChannelService::SetWindowAudioCaptured(nsPIDOMWindow* aWindow,
                                            uint64_t aInnerWindowID,
                                            bool aCapture)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelService, SetWindowAudioCaptured, window = %p, "
          "aCapture = %d\n", aWindow, aCapture));

  nsCOMPtr<nsPIDOMWindow> topWindow = aWindow->GetScriptableTop();
  if (!topWindow) {
    return;
  }

  AudioChannelWindow* winData = GetWindowData(topWindow->WindowID());

  // This can happen, but only during shutdown, because the the outer window
  // changes ScriptableTop, so that its ID is different.
  // In this case either we are capturing, and it's too late because the window
  // has been closed anyways, or we are un-capturing, and everything has already
  // been cleaned up by the HTMLMediaElements or the AudioContexts.
  if (!winData) {
    return;
  }

  if (aCapture != winData->mIsAudioCaptured) {
    winData->mIsAudioCaptured = aCapture;
    nsTObserverArray<AudioChannelAgent*>::ForwardIterator
      iter(winData->mAgents);
    while (iter.HasMore()) {
      iter.GetNext()->WindowAudioCaptureChanged(aInnerWindowID, aCapture);
    }
  }
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

AudioChannelService::AudioChannelWindow*
AudioChannelService::GetOrCreateWindowData(nsPIDOMWindow* aWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow* winData = GetWindowData(aWindow->WindowID());
  if (!winData) {
    winData = new AudioChannelWindow(aWindow->WindowID());
    mWindows.AppendElement(winData);
  }

  return winData;
}

AudioChannelService::AudioChannelWindow*
AudioChannelService::GetWindowData(uint64_t aWindowID) const
{
  nsTObserverArray<nsAutoPtr<AudioChannelWindow>>::ForwardIterator
    iter(mWindows);
  while (iter.HasMore()) {
    AudioChannelWindow* next = iter.GetNext();
    if (next->mWindowID == aWindowID) {
      return next;
    }
  }

  return nullptr;
}

float
AudioChannelService::GetAudioChannelVolume(nsPIDOMWindow* aWindow,
                                           AudioChannel aAudioChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aWindow);
  MOZ_ASSERT(aWindow->IsOuterWindow());

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  return winData->mChannels[(uint32_t)aAudioChannel].mVolume;
}

NS_IMETHODIMP
AudioChannelService::GetAudioChannelVolume(nsIDOMWindow* aWindow,
                                           unsigned short aAudioChannel,
                                           float* aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(piWindow);
  MOZ_ASSERT(window->IsOuterWindow());
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

  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelService, SetAudioChannelVolume, window = %p, type = %d, "
          "volume = %f\n", aWindow, aAudioChannel, aVolume));

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  winData->mChannels[(uint32_t)aAudioChannel].mVolume = aVolume;
  RefreshAgentsVolumeAndPropagate(aAudioChannel, aWindow);
}

NS_IMETHODIMP
AudioChannelService::SetAudioChannelVolume(nsIDOMWindow* aWindow,
                                           unsigned short aAudioChannel,
                                           float aVolume)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(piWindow);
  MOZ_ASSERT(window->IsOuterWindow());
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

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  return winData->mChannels[(uint32_t)aAudioChannel].mMuted;
}

NS_IMETHODIMP
AudioChannelService::GetAudioChannelMuted(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool* aMuted)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(piWindow);
  MOZ_ASSERT(window->IsOuterWindow());
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

  MOZ_LOG(GetAudioChannelLog(), LogLevel::Debug,
         ("AudioChannelService, SetAudioChannelMuted, window = %p, type = %d, "
          "mute = %d\n", aWindow, aAudioChannel, aMuted));

  if (aAudioChannel == AudioChannel::System) {
    // Workaround for bug1183033, system channel type can always playback.
    return;
  }

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  winData->mChannels[(uint32_t)aAudioChannel].mMuted = aMuted;
  RefreshAgentsVolumeAndPropagate(aAudioChannel, aWindow);
}

NS_IMETHODIMP
AudioChannelService::SetAudioChannelMuted(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool aMuted)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(piWindow);
  MOZ_ASSERT(window->IsOuterWindow());
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

  AudioChannelWindow* winData = GetOrCreateWindowData(aWindow);
  return !!winData->mChannels[(uint32_t)aAudioChannel].mNumberOfAgents;
}

NS_IMETHODIMP
AudioChannelService::IsAudioChannelActive(nsIDOMWindow* aWindow,
                                          unsigned short aAudioChannel,
                                          bool* aActive)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsPIDOMWindow> piWindow = do_QueryInterface(aWindow);
  nsCOMPtr<nsPIDOMWindow> window = GetTopWindow(piWindow);
  MOZ_ASSERT(window->IsOuterWindow());
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
    RemoveChildStatus(aChildID);
    return;
  }

  AudioChannelChildStatus* data = GetChildStatus(aChildID);
  if (!data) {
    data = new AudioChannelChildStatus(aChildID);
    mPlayingChildren.AppendElement(data);
  }

  data->mActiveTelephonyChannel = aTelephonyChannel;
  data->mActiveContentOrNormalChannel = aContentOrNormalChannel;
}

/* static */ bool
AudioChannelService::IsAudioChannelMutedByDefault()
{
  CreateServiceIfNeeded();
  return sAudioChannelMutedByDefault;
}
