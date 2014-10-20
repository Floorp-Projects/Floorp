/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"
#include "AudioChannelServiceChild.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"

#include "mozilla/dom/ContentParent.h"

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

// static
AudioChannelService*
AudioChannelService::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::GetAudioChannelService();
  }

  return gAudioChannelService;

}

// static
AudioChannelService*
AudioChannelService::GetOrCreateAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::GetOrCreateAudioChannelService();
  }

  // If we already exist, exit early
  if (gAudioChannelService) {
    return gAudioChannelService;
  }

  // Create new instance, register, return
  nsRefPtr<AudioChannelService> service = new AudioChannelService();
  MOZ_ASSERT(service);

  gAudioChannelService = service;
  return gAudioChannelService;
}

void
AudioChannelService::Shutdown()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::Shutdown();
  }

  if (gAudioChannelService) {
    gAudioChannelService = nullptr;
  }
}

NS_IMPL_ISUPPORTS(AudioChannelService, nsIObserver, nsITimerCallback)

AudioChannelService::AudioChannelService()
: mCurrentHigherChannel(-1)
, mCurrentVisibleHigherChannel(-1)
, mPlayableHiddenContentChildID(CONTENT_PROCESS_ID_UNKNOWN)
, mDisabled(false)
, mDefChannelChildID(CONTENT_PROCESS_ID_UNKNOWN)
{
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
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
                                               AudioChannel aChannel,
                                               bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  AudioChannelAgentData* data = new AudioChannelAgentData(aChannel,
                                true /* aElementHidden */,
                                AUDIO_CHANNEL_STATE_MUTED /* aState */,
                                aWithVideo);
  mAgents.Put(aAgent, data);
  RegisterType(aChannel, CONTENT_PROCESS_ID_MAIN, aWithVideo);

  // If this is the first agent for this window, we must notify the observers.
  uint32_t count = CountWindow(aAgent->Window());
  if (count == 1) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aAgent->Window()),
                                       "media-playback",
                                       NS_LITERAL_STRING("active").get());
    }
  }
}

void
AudioChannelService::RegisterType(AudioChannel aChannel, uint64_t aChildID,
                                  bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  AudioChannelInternalType type = GetInternalType(aChannel, true);
  mChannelCounters[type].AppendElement(aChildID);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {

    // We must keep the childIds in order to decide which app is allowed to play
    // with then telephony channel.
    if (aChannel == AudioChannel::Telephony) {
      RegisterTelephonyChild(aChildID);
    }

    // Since there is another telephony registered, we can unregister old one
    // immediately.
    if (mDeferTelChannelTimer && aChannel == AudioChannel::Telephony) {
      mDeferTelChannelTimer->Cancel();
      mDeferTelChannelTimer = nullptr;
      UnregisterTypeInternal(aChannel, mTimerElementHidden, mTimerChildID,
                             false);
    }

    if (aWithVideo) {
      mWithVideoChildIDs.AppendElement(aChildID);
    }

    // No hidden content channel can be playable if there is a content channel
    // in foreground (bug 855208), nor if there is a normal channel with video
    // in foreground (bug 894249).
    if (type == AUDIO_CHANNEL_INT_CONTENT ||
        (type == AUDIO_CHANNEL_INT_NORMAL &&
         mWithVideoChildIDs.Contains(aChildID))) {
      mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
    }
    // One hidden content channel can be playable only when there is no any
    // content channel in the foreground, and no normal channel with video in
    // foreground.
    else if (type == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
        mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
      mPlayableHiddenContentChildID = aChildID;
    }

    // In order to avoid race conditions, it's safer to notify any existing
    // agent any time a new one is registered.
    SendAudioChannelChangedNotification(aChildID);
    SendNotification();
  }
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  if (mDisabled) {
    return;
  }

  nsAutoPtr<AudioChannelAgentData> data;
  mAgents.RemoveAndForget(aAgent, data);

  if (data) {
    UnregisterType(data->mChannel, data->mElementHidden,
                   CONTENT_PROCESS_ID_MAIN, data->mWithVideo);
  }

#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive();
  for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
    mSpeakerManager[i]->SetAudioChannelActive(active);
  }
#endif

  // If this is the last agent for this window, we must notify the observers.
  uint32_t count = CountWindow(aAgent->Window());
  if (count == 0) {
    nsCOMPtr<nsIObserverService> observerService =
      services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(ToSupports(aAgent->Window()),
                                       "media-playback",
                                       NS_LITERAL_STRING("inactive").get());
    }
  }
}

void
AudioChannelService::UnregisterType(AudioChannel aChannel,
                                    bool aElementHidden,
                                    uint64_t aChildID,
                                    bool aWithVideo)
{
  if (mDisabled) {
    return;
  }

  // There are two reasons to defer the decrease of telephony channel.
  // 1. User can have time to remove device from his ear before music resuming.
  // 2. Give BT SCO to be disconnected before starting to connect A2DP.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {

    if (aChannel == AudioChannel::Telephony) {
      UnregisterTelephonyChild(aChildID);
    }

    if (aChannel == AudioChannel::Telephony &&
        (mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].Length() +
         mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].Length()) == 1) {
      mTimerElementHidden = aElementHidden;
      mTimerChildID = aChildID;
      mDeferTelChannelTimer = do_CreateInstance("@mozilla.org/timer;1");
      mDeferTelChannelTimer->InitWithCallback(this, 1500, nsITimer::TYPE_ONE_SHOT);
      return;
    }
  }

  UnregisterTypeInternal(aChannel, aElementHidden, aChildID, aWithVideo);
}

void
AudioChannelService::UnregisterTypeInternal(AudioChannel aChannel,
                                            bool aElementHidden,
                                            uint64_t aChildID,
                                            bool aWithVideo)
{
  // The array may contain multiple occurrence of this appId but
  // this should remove only the first one.
  AudioChannelInternalType type = GetInternalType(aChannel, aElementHidden);
  MOZ_ASSERT(mChannelCounters[type].Contains(aChildID));
  mChannelCounters[type].RemoveElement(aChildID);

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // No hidden content channel is playable if the original playable hidden
    // process does not need to play audio from background anymore.
    if (aChannel == AudioChannel::Content &&
        mPlayableHiddenContentChildID == aChildID &&
        !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(aChildID)) {
      mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
    }

    if (aWithVideo) {
      MOZ_ASSERT(mWithVideoChildIDs.Contains(aChildID));
      mWithVideoChildIDs.RemoveElement(aChildID);
    }

    SendAudioChannelChangedNotification(aChildID);
    SendNotification();
  }
}

void
AudioChannelService::UpdateChannelType(AudioChannel aChannel,
                                       uint64_t aChildID,
                                       bool aElementHidden,
                                       bool aElementWasHidden)
{
  // Calculate the new and old internal type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aChannel, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aChannel, aElementWasHidden);

  if (newType != oldType) {
    mChannelCounters[newType].AppendElement(aChildID);
    MOZ_ASSERT(mChannelCounters[oldType].Contains(aChildID));
    mChannelCounters[oldType].RemoveElement(aChildID);
  }

  // No hidden content channel can be playable if there is a content channel
  // in foreground (bug 855208), nor if there is a normal channel with video
  // in foreground (bug 894249).
  if (newType == AUDIO_CHANNEL_INT_CONTENT ||
      (newType == AUDIO_CHANNEL_INT_NORMAL &&
       mWithVideoChildIDs.Contains(aChildID))) {
    mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
  }
  // If there is no content channel in foreground and no normal channel with
  // video in foreground, the last content channel which goes from foreground
  // to background can be playable.
  else if (oldType == AUDIO_CHANNEL_INT_CONTENT &&
      newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
      mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    mPlayableHiddenContentChildID = aChildID;
  }
}

AudioChannelState
AudioChannelService::GetState(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  bool oldElementHidden = data->mElementHidden;
  // Update visibility.
  data->mElementHidden = aElementHidden;

  data->mState = GetStateInternal(data->mChannel, CONTENT_PROCESS_ID_MAIN,
                                aElementHidden, oldElementHidden);
  return data->mState;
}

AudioChannelState
AudioChannelService::GetStateInternal(AudioChannel aChannel, uint64_t aChildID,
                                      bool aElementHidden,
                                      bool aElementWasHidden)
{
  UpdateChannelType(aChannel, aChildID, aElementHidden, aElementWasHidden);

  // Calculating the new and old type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aChannel, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aChannel,
                                                     aElementWasHidden);

  if (newType != oldType &&
      (aChannel == AudioChannel::Content ||
       (aChannel == AudioChannel::Normal &&
        mWithVideoChildIDs.Contains(aChildID)))) {
    SendNotification();
  }

  SendAudioChannelChangedNotification(aChildID);

  // Let play any visible audio channel.
  if (!aElementHidden) {
    if (CheckVolumeFadedCondition(newType, aElementHidden)) {
      return AUDIO_CHANNEL_STATE_FADED;
    }
    return CheckTelephonyPolicy(aChannel, aChildID);
  }

  // We are not visible, maybe we have to mute.
  if (newType == AUDIO_CHANNEL_INT_NORMAL_HIDDEN ||
      (newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
       // One process can have multiple content channels; and during the
       // transition from foreground to background, its content channels will be
       // updated with correct visibility status one by one. All its content
       // channels should remain playable until all of their visibility statuses
       // have been updated as hidden. After all its content channels have been
       // updated properly as hidden, mPlayableHiddenContentChildID is used to
       // check whether this background process is playable or not.
       !(mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID) ||
         (mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() &&
          mPlayableHiddenContentChildID == aChildID)))) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  // After checking the condition on normal & content channel, if the state
  // is not on muted then checking other higher channels type here.
  if (ChannelsActiveWithHigherPriorityThan(newType)) {
    MOZ_ASSERT(newType != AUDIO_CHANNEL_INT_NORMAL_HIDDEN);
    if (CheckVolumeFadedCondition(newType, aElementHidden)) {
      return AUDIO_CHANNEL_STATE_FADED;
    }
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  return CheckTelephonyPolicy(aChannel, aChildID);
}

AudioChannelState
AudioChannelService::CheckTelephonyPolicy(AudioChannel aChannel,
                                          uint64_t aChildID)
{
  // Only the latest childID is allowed to play with telephony channel.
  if (aChannel != AudioChannel::Telephony) {
    return AUDIO_CHANNEL_STATE_NORMAL;
  }

  MOZ_ASSERT(!mTelephonyChildren.IsEmpty());

#if DEBUG
  bool found = false;
  for (uint32_t i = 0, len = mTelephonyChildren.Length(); i < len; ++i) {
    if (mTelephonyChildren[i].mChildID == aChildID) {
      found = true;
      break;
    }
  }

  MOZ_ASSERT(found);
#endif

  return mTelephonyChildren.LastElement().mChildID == aChildID
           ? AUDIO_CHANNEL_STATE_NORMAL : AUDIO_CHANNEL_STATE_MUTED;
}

bool
AudioChannelService::CheckVolumeFadedCondition(AudioChannelInternalType aType,
                                               bool aElementHidden)
{
  // Only normal & content channels are considered
  if (aType > AUDIO_CHANNEL_INT_CONTENT_HIDDEN) {
    return false;
  }

  // Consider that audio from notification is with short duration
  // so just fade the volume not pause it
  if (mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION].IsEmpty() &&
      mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN].IsEmpty()) {
    return false;
  }

  // Since this element is on the foreground, it can be allowed to play always.
  // So return true directly when there is any notification channel alive.
  if (aElementHidden == false) {
   return true;
  }

  // If element is on the background, it is possible paused by channels higher
  // then notification.
  for (int i = AUDIO_CHANNEL_INT_LAST - 1;
    i != AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN; --i) {
    if (!mChannelCounters[i].IsEmpty()) {
      return false;
    }
  }

  return true;
}

bool
AudioChannelService::ContentOrNormalChannelIsActive()
{
  return !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty();
}

bool
AudioChannelService::TelephonyChannelIsActive()
{
  return !mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].IsEmpty();
}

bool
AudioChannelService::ProcessContentOrNormalChannelIsActive(uint64_t aChildID)
{
  return mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].Contains(aChildID);
}

void
AudioChannelService::SetDefaultVolumeControlChannel(int32_t aChannel,
                                                    bool aHidden)
{
  SetDefaultVolumeControlChannelInternal(aChannel, aHidden,
                                         CONTENT_PROCESS_ID_MAIN);
}

void
AudioChannelService::SetDefaultVolumeControlChannelInternal(int32_t aChannel,
                                                            bool aHidden,
                                                            uint64_t aChildID)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  // If this child is in the background and mDefChannelChildID is set to
  // others then it means other child in the foreground already set it's
  // own default channel already.
  if (!aHidden && mDefChannelChildID != aChildID) {
    return;
  }

  mDefChannelChildID = aChildID;
  nsString channelName;

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
AudioChannelService::SendAudioChannelChangedNotification(uint64_t aChildID)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), aChildID);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(static_cast<nsIWritablePropertyBag*>(props),
                         "audio-channel-process-changed", nullptr);
  }

  // Calculating the most important active channel.
  int32_t higher = -1;

  // Top-Down in the hierarchy for visible elements
  if (!mChannelCounters[AUDIO_CHANNEL_INT_PUBLICNOTIFICATION].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Publicnotification);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_RINGER].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Ringer);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Telephony);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_ALARM].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Alarm);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Notification);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Content);
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty()) {
    higher = static_cast<int32_t>(AudioChannel::Normal);
  }

  int32_t visibleHigher = higher;

  // Top-Down in the hierarchy for non-visible elements
  // And we can ignore normal channel because it can't play in the background.
  int32_t index;
  for (index = 0; kMozAudioChannelAttributeTable[index].tag; ++index);

  for (--index;
       kMozAudioChannelAttributeTable[index].value > higher &&
       kMozAudioChannelAttributeTable[index].value > (int16_t)AudioChannel::Normal;
       --index) {
    // Each channel type will be split to fg and bg for recording the state,
    // so here need to do a translation.
    if (mChannelCounters[index * 2 + 1].IsEmpty()) {
      continue;
    }

    if (kMozAudioChannelAttributeTable[index].value == (int16_t)AudioChannel::Content) {
      if (mPlayableHiddenContentChildID != CONTENT_PROCESS_ID_UNKNOWN) {
        higher = kMozAudioChannelAttributeTable[index].value;
        break;
      }
    } else {
      higher = kMozAudioChannelAttributeTable[index].value;
      break;
    }
  }

  if (higher != mCurrentHigherChannel) {
    mCurrentHigherChannel = higher;

    nsString channelName;
    if (mCurrentHigherChannel != -1) {
      GetAudioChannelString(static_cast<AudioChannel>(mCurrentHigherChannel),
                            channelName);
    } else {
      channelName.AssignLiteral("none");
    }

    if (obs) {
      obs->NotifyObservers(nullptr, "audio-channel-changed", channelName.get());
    }
  }

  if (visibleHigher != mCurrentVisibleHigherChannel) {
    mCurrentVisibleHigherChannel = visibleHigher;

    nsString channelName;
    if (mCurrentVisibleHigherChannel != -1) {
      GetAudioChannelString(static_cast<AudioChannel>(mCurrentVisibleHigherChannel),
                            channelName);
    } else {
      channelName.AssignLiteral("none");
    }

    if (obs) {
      obs->NotifyObservers(nullptr, "visible-audio-channel-changed", channelName.get());
    }
  }
}

PLDHashOperator
AudioChannelService::NotifyEnumerator(AudioChannelAgent* aAgent,
                                      AudioChannelAgentData* aData, void* aUnused)
{
  MOZ_ASSERT(aAgent);
  aAgent->NotifyAudioChannelStateChanged();
  return PL_DHASH_NEXT;
}

class NotifyRunnable : public nsRunnable
{
public:
  explicit NotifyRunnable(AudioChannelService* aService)
    : mService(aService)
  {}

  NS_IMETHOD Run()
  {
    mService->Notify();
    return NS_OK;
  }

private:
  nsRefPtr<AudioChannelService> mService;
};

void
AudioChannelService::SendNotification()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mRunnable) {
    return;
  }

  mRunnable = new NotifyRunnable(this);
  NS_DispatchToCurrentThread(mRunnable);
}

void
AudioChannelService::Notify()
{
  MOZ_ASSERT(NS_IsMainThread());
  mRunnable = nullptr;

  // Notify any agent for the main process.
  mAgents.EnumerateRead(NotifyEnumerator, nullptr);

  // Notify for the child processes.
  nsTArray<ContentParent*> children;
  ContentParent::GetAll(children);
  for (uint32_t i = 0; i < children.Length(); i++) {
    unused << children[i]->SendAudioChannelNotify();
  }
}

NS_IMETHODIMP
AudioChannelService::Notify(nsITimer* aTimer)
{
  UnregisterTypeInternal(AudioChannel::Telephony, mTimerElementHidden,
                         mTimerChildID, false);
  mDeferTelChannelTimer = nullptr;
  return NS_OK;
}

bool
AudioChannelService::AnyAudioChannelIsActive()
{
  for (int i = AUDIO_CHANNEL_INT_LAST - 1;
       i >= AUDIO_CHANNEL_INT_NORMAL; --i) {
    if (!mChannelCounters[i].IsEmpty()) {
      return true;
    }
  }

  return false;
}

bool
AudioChannelService::ChannelsActiveWithHigherPriorityThan(
  AudioChannelInternalType aType)
{
  for (int i = AUDIO_CHANNEL_INT_LAST - 1;
       i != AUDIO_CHANNEL_INT_CONTENT_HIDDEN; --i) {
    if (i == aType) {
      return false;
    }

    if (!mChannelCounters[i].IsEmpty()) {
      return true;
    }
  }

  return false;
}

PLDHashOperator
AudioChannelService::WindowDestroyedEnumerator(AudioChannelAgent* aAgent,
                                               nsAutoPtr<AudioChannelAgentData>& aData,
                                               void* aPtr)
{
  uint64_t* innerID = static_cast<uint64_t*>(aPtr);
  MOZ_ASSERT(innerID);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aAgent->Window());
  if (!window || window->WindowID() != *innerID) {
    return PL_DHASH_NEXT;
  }

  AudioChannelService* service = AudioChannelService::GetAudioChannelService();
  MOZ_ASSERT(service);

  service->UnregisterType(aData->mChannel, aData->mElementHidden,
                          CONTENT_PROCESS_ID_MAIN, aData->mWithVideo);

  return PL_DHASH_REMOVE;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic, const char16_t* aData)
{
  if (!strcmp(aTopic, "xpcom-shutdown")) {
    mDisabled = true;
  }

  if (!strcmp(aTopic, "ipc:content-shutdown")) {
    nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
    if (!props) {
      NS_WARNING("ipc:content-shutdown message without property bag as subject");
      return NS_OK;
    }

    int32_t index;
    uint64_t childID = 0;
    nsresult rv = props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"),
                                             &childID);
    if (NS_SUCCEEDED(rv)) {
      for (int32_t type = AUDIO_CHANNEL_INT_NORMAL;
           type < AUDIO_CHANNEL_INT_LAST;
           ++type) {

        while ((index = mChannelCounters[type].IndexOf(childID)) != -1) {
          mChannelCounters[type].RemoveElementAt(index);
        }
      }

      // No hidden content channel is playable if the original playable hidden
      // process shuts down.
      if (mPlayableHiddenContentChildID == childID) {
        mPlayableHiddenContentChildID = CONTENT_PROCESS_ID_UNKNOWN;
      }

      while ((index = mWithVideoChildIDs.IndexOf(childID)) != -1) {
        mWithVideoChildIDs.RemoveElementAt(index);
      }

      // We don't have to remove the agents from the mAgents hashtable because if
      // that table contains only agents running on the same process.

      SendAudioChannelChangedNotification(childID);
      SendNotification();

      if (mDefChannelChildID == childID) {
        SetDefaultVolumeControlChannelInternal(-1, false, childID);
        mDefChannelChildID = CONTENT_PROCESS_ID_UNKNOWN;
      }
    } else {
      NS_WARNING("ipc:content-shutdown message without childID property");
    }
  }

#ifdef MOZ_WIDGET_GONK
  // To process the volume control on each audio channel according to
  // change of settings
  else if (!strcmp(aTopic, "mozsettings-changed")) {
    AutoJSAPI jsapi;
    jsapi.Init();
    JSContext* cx = jsapi.cx();
    RootedDictionary<SettingChangeNotification> setting(cx);
    if (!WrappedJSToDictionary(cx, aSubject, setting)) {
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

    mAgents.Enumerate(WindowDestroyedEnumerator, &innerID);

#ifdef MOZ_WIDGET_GONK
    bool active = AnyAudioChannelIsActive();
    for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
      mSpeakerManager[i]->SetAudioChannelActive(active);
    }
#endif
  }

  return NS_OK;
}

AudioChannelService::AudioChannelInternalType
AudioChannelService::GetInternalType(AudioChannel aChannel,
                                     bool aElementHidden)
{
  switch (aChannel) {
    case AudioChannel::Normal:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NORMAL_HIDDEN
               : AUDIO_CHANNEL_INT_NORMAL;

    case AudioChannel::Content:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_CONTENT_HIDDEN
               : AUDIO_CHANNEL_INT_CONTENT;

    case AudioChannel::Notification:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_NOTIFICATION;

    case AudioChannel::Alarm:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_ALARM_HIDDEN
               : AUDIO_CHANNEL_INT_ALARM;

    case AudioChannel::Telephony:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN
               : AUDIO_CHANNEL_INT_TELEPHONY;

    case AudioChannel::Ringer:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_RINGER_HIDDEN
               : AUDIO_CHANNEL_INT_RINGER;

    case AudioChannel::Publicnotification:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_PUBLICNOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_PUBLICNOTIFICATION;

    default:
      break;
  }

  MOZ_CRASH("unexpected audio channel");
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
AudioChannelService::RefreshAgentsVolumeEnumerator(AudioChannelAgent* aAgent,
                                                   AudioChannelAgentData* aUnused,
                                                   void* aPtr)
{
  MOZ_ASSERT(aAgent);
  RefreshAgentsVolumeData* data = static_cast<RefreshAgentsVolumeData*>(aPtr);
  MOZ_ASSERT(data);

  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aAgent->Window());
  if (window && !window->IsInnerWindow()) {
    window = window->GetCurrentInnerWindow();
  }

  if (window == data->mWindow) {
    data->mAgents.AppendElement(aAgent);
  }

  return PL_DHASH_NEXT;
}
void
AudioChannelService::RefreshAgentsVolume(nsPIDOMWindow* aWindow)
{
  RefreshAgentsVolumeData data(aWindow);
  mAgents.EnumerateRead(RefreshAgentsVolumeEnumerator, &data);

  for (uint32_t i = 0; i < data.mAgents.Length(); ++i) {
    data.mAgents[i]->WindowVolumeChanged();
  }
}

struct CountWindowData
{
  explicit CountWindowData(nsIDOMWindow* aWindow)
    : mWindow(aWindow)
    , mCount(0)
  {}

  nsIDOMWindow* mWindow;
  uint32_t mCount;
};

PLDHashOperator
AudioChannelService::CountWindowEnumerator(AudioChannelAgent* aAgent,
                                           AudioChannelAgentData* aUnused,
                                           void* aPtr)
{
  CountWindowData* data = static_cast<CountWindowData*>(aPtr);
  MOZ_ASSERT(aAgent);

  if (aAgent->Window() == data->mWindow) {
    ++data->mCount;
  }

  return PL_DHASH_NEXT;
}

uint32_t
AudioChannelService::CountWindow(nsIDOMWindow* aWindow)
{
  CountWindowData data(aWindow);
  mAgents.EnumerateRead(CountWindowEnumerator, &data);
  return data.mCount;
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
  nsString audioChannel = Preferences::GetString("media.defaultAudioChannel");
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

  nsString audioChannel = Preferences::GetString("media.defaultAudioChannel");
  if (!audioChannel.IsEmpty()) {
    for (uint32_t i = 0; kMozAudioChannelAttributeTable[i].tag; ++i) {
      if (audioChannel.EqualsASCII(kMozAudioChannelAttributeTable[i].tag)) {
        aString = audioChannel;
        break;
      }
    }
  }
}

void
AudioChannelService::RegisterTelephonyChild(uint64_t aChildID)
{
  for (uint32_t i = 0, len = mTelephonyChildren.Length(); i < len; ++i) {
    if (mTelephonyChildren[i].mChildID == aChildID) {
      ++mTelephonyChildren[i].mInstances;

      if (i != len - 1) {
        TelephonyChild child = mTelephonyChildren[i];
        mTelephonyChildren.RemoveElementAt(i);
        mTelephonyChildren.AppendElement(child);
      }

      return;
    }
  }

  mTelephonyChildren.AppendElement(TelephonyChild(aChildID));
}

void
AudioChannelService::UnregisterTelephonyChild(uint64_t aChildID)
{
  for (uint32_t i = 0, len = mTelephonyChildren.Length(); i < len; ++i) {
    if (mTelephonyChildren[i].mChildID == aChildID) {
      if (!--mTelephonyChildren[i].mInstances) {
        mTelephonyChildren.RemoveElementAt(i);
      }

      return;
    }
  }

  MOZ_ASSERT(false, "This should not happen.");
}
