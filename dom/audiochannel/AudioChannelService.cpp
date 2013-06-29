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
#include "mozilla/Util.h"

#include "mozilla/dom/ContentParent.h"

#include "nsThreadUtils.h"
#include "nsHashPropertyBag.h"

#ifdef MOZ_WIDGET_GONK
#include "nsIAudioManager.h"
#endif
using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

StaticRefPtr<AudioChannelService> gAudioChannelService;

// static
AudioChannelService*
AudioChannelService::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return AudioChannelServiceChild::GetAudioChannelService();
  }

  // If we already exist, exit early
  if (gAudioChannelService) {
    return gAudioChannelService;
  }

  // Create new instance, register, return
  nsRefPtr<AudioChannelService> service = new AudioChannelService();
  NS_ENSURE_TRUE(service, nullptr);

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

NS_IMPL_ISUPPORTS2(AudioChannelService, nsIObserver, nsITimerCallback)

AudioChannelService::AudioChannelService()
: mCurrentHigherChannel(AUDIO_CHANNEL_LAST)
, mCurrentVisibleHigherChannel(AUDIO_CHANNEL_LAST)
, mActiveContentChildIDsFrozen(false)
{
  // Creation of the hash table.
  mAgents.Init();

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->AddObserver(this, "ipc:content-shutdown", false);
    }
  }
}

AudioChannelService::~AudioChannelService()
{
}

void
AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                               AudioChannelType aType)
{
  AudioChannelAgentData* data = new AudioChannelAgentData(aType,
                                                          true /* mElementHidden */,
                                                          true /* mMuted */);
  mAgents.Put(aAgent, data);
  RegisterType(aType, CONTENT_PROCESS_ID_MAIN);
}

void
AudioChannelService::RegisterType(AudioChannelType aType, uint64_t aChildID)
{
  AudioChannelInternalType type = GetInternalType(aType, true);
  mChannelCounters[type].AppendElement(aChildID);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // Since there is another telephony registered, we can unregister old one
    // immediately.
    if (mDeferTelChannelTimer && aType == AUDIO_CHANNEL_TELEPHONY) {
      mDeferTelChannelTimer->Cancel();
      mDeferTelChannelTimer = nullptr;
      UnregisterTypeInternal(aType, mTimerElementHidden, mTimerChildID);
    }

    // In order to avoid race conditions, it's safer to notify any existing
    // agent any time a new one is registered.
    SendAudioChannelChangedNotification(aChildID);
    Notify();
  }
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  nsAutoPtr<AudioChannelAgentData> data;
  mAgents.RemoveAndForget(aAgent, data);

  if (data) {
    UnregisterType(data->mType, data->mElementHidden, CONTENT_PROCESS_ID_MAIN);
  }
}

void
AudioChannelService::UnregisterType(AudioChannelType aType,
                                    bool aElementHidden,
                                    uint64_t aChildID)
{
  // There are two reasons to defer the decrease of telephony channel.
  // 1. User can have time to remove device from his ear before music resuming.
  // 2. Give BT SCO to be disconnected before starting to connect A2DP.
  if (XRE_GetProcessType() == GeckoProcessType_Default &&
      aType == AUDIO_CHANNEL_TELEPHONY &&
      (mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].Length() +
       mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].Length()) == 1) {
    mTimerElementHidden = aElementHidden;
    mTimerChildID = aChildID;
    mDeferTelChannelTimer = do_CreateInstance("@mozilla.org/timer;1");
    mDeferTelChannelTimer->InitWithCallback(this, 1500, nsITimer::TYPE_ONE_SHOT);
    return;
  }

  UnregisterTypeInternal(aType, aElementHidden, aChildID);
}

void
AudioChannelService::UnregisterTypeInternal(AudioChannelType aType,
                                            bool aElementHidden,
                                            uint64_t aChildID)
{
  // The array may contain multiple occurrence of this appId but
  // this should remove only the first one.
  AudioChannelInternalType type = GetInternalType(aType, aElementHidden);
  MOZ_ASSERT(mChannelCounters[type].Contains(aChildID));
  mChannelCounters[type].RemoveElement(aChildID);

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // We only remove ChildID when it is in the foreground.
    // If in the background, we kept ChildID for allowing it to play next song.
    if (aType == AUDIO_CHANNEL_CONTENT &&
        mActiveContentChildIDs.Contains(aChildID) &&
        !aElementHidden &&
        !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID)) {
      mActiveContentChildIDs.RemoveElement(aChildID);
    }
    SendAudioChannelChangedNotification(aChildID);
    Notify();
  }
}

void
AudioChannelService::UpdateChannelType(AudioChannelType aType,
                                       uint64_t aChildID,
                                       bool aElementHidden,
                                       bool aElementWasHidden)
{
  // Calculate the new and old internal type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aType, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aType, aElementWasHidden);

  if (newType != oldType) {
    mChannelCounters[newType].AppendElement(aChildID);
    MOZ_ASSERT(mChannelCounters[oldType].Contains(aChildID));
    mChannelCounters[oldType].RemoveElement(aChildID);
  }
}

bool
AudioChannelService::GetMuted(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return true;
  }

  bool oldElementHidden = data->mElementHidden;
  // Update visibility.
  data->mElementHidden = aElementHidden;

  bool muted = GetMutedInternal(data->mType, CONTENT_PROCESS_ID_MAIN,
                                aElementHidden, oldElementHidden);
  data->mMuted = muted;

  return muted;
}

bool
AudioChannelService::GetMutedInternal(AudioChannelType aType, uint64_t aChildID,
                                      bool aElementHidden, bool aElementWasHidden)
{
  UpdateChannelType(aType, aChildID, aElementHidden, aElementWasHidden);

  // Calculating the new and old type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aType, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aType, aElementWasHidden);

  // If the audio content channel is visible, let's remember this ChildID.
  if (newType == AUDIO_CHANNEL_INT_CONTENT &&
      oldType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN) {

    if (mActiveContentChildIDsFrozen) {
      mActiveContentChildIDsFrozen = false;
      mActiveContentChildIDs.Clear();
    }

    if (!mActiveContentChildIDs.Contains(aChildID)) {
      mActiveContentChildIDs.AppendElement(aChildID);
    }
  }
  else if (newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
           oldType == AUDIO_CHANNEL_INT_CONTENT &&
           !mActiveContentChildIDsFrozen) {
    // If nothing is visible, the list has to been frozen.
    // Or if there is still any one with other ChildID in foreground then
    // it should be removed from list and left other ChildIDs in the foreground
    // to keep playing. Finally only last one childID which go to background
    // will be in list.
    if (mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
      mActiveContentChildIDsFrozen = true;
    } else if (!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID)) {
      MOZ_ASSERT(mActiveContentChildIDs.Contains(aChildID));
      mActiveContentChildIDs.RemoveElement(aChildID);
    }
  }

  if (newType != oldType && aType == AUDIO_CHANNEL_CONTENT) {
    Notify();
  }

  SendAudioChannelChangedNotification(aChildID);

  // Let play any visible audio channel.
  if (!aElementHidden) {
    return false;
  }

  bool muted = false;

  // We are not visible, maybe we have to mute.
  if (newType == AUDIO_CHANNEL_INT_NORMAL_HIDDEN ||
      (newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
       !mActiveContentChildIDs.Contains(aChildID))) {
    muted = true;
  }

  if (!muted) {
    MOZ_ASSERT(newType != AUDIO_CHANNEL_INT_NORMAL_HIDDEN);
    muted = ChannelsActiveWithHigherPriorityThan(newType);
  }

  return muted;
}

bool
AudioChannelService::ContentOrNormalChannelIsActive()
{
  return !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty();
}

bool
AudioChannelService::ProcessContentOrNormalChannelIsActive(uint64_t aChildID)
{
  return mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(aChildID) ||
         mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].Contains(aChildID);
}

void
AudioChannelService::SendAudioChannelChangedNotification(uint64_t aChildID)
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  nsRefPtr<nsHashPropertyBag> props = new nsHashPropertyBag();
  props->Init();
  props->SetPropertyAsUint64(NS_LITERAL_STRING("childID"), aChildID);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  obs->NotifyObservers(static_cast<nsIWritablePropertyBag*>(props),
                       "audio-channel-process-changed", nullptr);

  // Calculating the most important active channel.
  AudioChannelType higher = AUDIO_CHANNEL_LAST;

  // Top-Down in the hierarchy for visible elements
  if (!mChannelCounters[AUDIO_CHANNEL_INT_PUBLICNOTIFICATION].IsEmpty()) {
    higher = AUDIO_CHANNEL_PUBLICNOTIFICATION;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_RINGER].IsEmpty()) {
    higher = AUDIO_CHANNEL_RINGER;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY].IsEmpty()) {
    higher = AUDIO_CHANNEL_TELEPHONY;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_ALARM].IsEmpty()) {
    higher = AUDIO_CHANNEL_ALARM;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION].IsEmpty()) {
    higher = AUDIO_CHANNEL_NOTIFICATION;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    higher = AUDIO_CHANNEL_CONTENT;
  }

  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty()) {
    higher = AUDIO_CHANNEL_NORMAL;
  }

  AudioChannelType visibleHigher = higher;

  // Top-Down in the hierarchy for non-visible elements
  if (higher == AUDIO_CHANNEL_LAST) {
    if (!mChannelCounters[AUDIO_CHANNEL_INT_PUBLICNOTIFICATION_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_PUBLICNOTIFICATION;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_RINGER_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_RINGER;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_TELEPHONY;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_ALARM_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_ALARM;
    }

    else if (!mChannelCounters[AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN].IsEmpty()) {
      higher = AUDIO_CHANNEL_NOTIFICATION;
    }

    // There is only one Child can play content channel in the background.
    // And need to check whether there is any content channels under playing
    // now.
    else if (!mActiveContentChildIDs.IsEmpty() &&
             mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Contains(
             mActiveContentChildIDs[0])) {
      higher = AUDIO_CHANNEL_CONTENT;
    }
  }

  if (higher != mCurrentHigherChannel) {
    mCurrentHigherChannel = higher;

    nsString channelName;
    if (mCurrentHigherChannel != AUDIO_CHANNEL_LAST) {
      channelName.AssignASCII(ChannelName(mCurrentHigherChannel));
    } else {
      channelName.AssignLiteral("none");
    }

    obs->NotifyObservers(nullptr, "audio-channel-changed", channelName.get());
  }

  if (visibleHigher != mCurrentVisibleHigherChannel) {
    mCurrentVisibleHigherChannel = visibleHigher;

    nsString channelName;
    if (mCurrentVisibleHigherChannel != AUDIO_CHANNEL_LAST) {
      channelName.AssignASCII(ChannelName(mCurrentVisibleHigherChannel));
    } else {
      channelName.AssignLiteral("none");
    }

    obs->NotifyObservers(nullptr, "visible-audio-channel-changed", channelName.get());
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

void
AudioChannelService::Notify()
{
  MOZ_ASSERT(NS_IsMainThread());

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
  UnregisterTypeInternal(AUDIO_CHANNEL_TELEPHONY, mTimerElementHidden, mTimerChildID);
  mDeferTelChannelTimer = nullptr;
  return NS_OK;
}

bool
AudioChannelService::ChannelsActiveWithHigherPriorityThan(AudioChannelInternalType aType)
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

const char*
AudioChannelService::ChannelName(AudioChannelType aType)
{
  static struct {
    int32_t type;
    const char* value;
  } ChannelNameTable[] = {
    { AUDIO_CHANNEL_NORMAL,             "normal" },
    { AUDIO_CHANNEL_CONTENT,            "content" },
    { AUDIO_CHANNEL_NOTIFICATION,       "notification" },
    { AUDIO_CHANNEL_ALARM,              "alarm" },
    { AUDIO_CHANNEL_TELEPHONY,          "telephony" },
    { AUDIO_CHANNEL_RINGER,             "ringer" },
    { AUDIO_CHANNEL_PUBLICNOTIFICATION, "publicnotification" },
    { -1,                               "unknown" }
  };

  for (int i = AUDIO_CHANNEL_NORMAL; ; ++i) {
    if (ChannelNameTable[i].type == aType ||
        ChannelNameTable[i].type == -1) {
      return ChannelNameTable[i].value;
    }
  }

  NS_NOTREACHED("Execution should not reach here!");
  return nullptr;
}

NS_IMETHODIMP
AudioChannelService::Observe(nsISupports* aSubject, const char* aTopic, const PRUnichar* data)
{
  MOZ_ASSERT(!strcmp(aTopic, "ipc:content-shutdown"));

  nsCOMPtr<nsIPropertyBag2> props = do_QueryInterface(aSubject);
  if (!props) {
    NS_WARNING("ipc:content-shutdown message without property bag as subject");
    return NS_OK;
  }

  uint64_t childID = 0;
  nsresult rv = props->GetPropertyAsUint64(NS_LITERAL_STRING("childID"),
                                           &childID);
  if (NS_SUCCEEDED(rv)) {
    for (int32_t type = AUDIO_CHANNEL_INT_NORMAL;
         type < AUDIO_CHANNEL_INT_LAST;
         ++type) {
      int32_t index;
      while ((index = mChannelCounters[type].IndexOf(childID)) != -1) {
        mChannelCounters[type].RemoveElementAt(index);
      }

      if ((index = mActiveContentChildIDs.IndexOf(childID)) != -1) {
        mActiveContentChildIDs.RemoveElementAt(index);
      }
    }

    // We don't have to remove the agents from the mAgents hashtable because if
    // that table contains only agents running on the same process.

    SendAudioChannelChangedNotification(childID);
    Notify();
  } else {
    NS_WARNING("ipc:content-shutdown message without childID property");
  }

  return NS_OK;
}

AudioChannelService::AudioChannelInternalType
AudioChannelService::GetInternalType(AudioChannelType aType,
                                     bool aElementHidden)
{
  switch (aType) {
    case AUDIO_CHANNEL_NORMAL:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NORMAL_HIDDEN
               : AUDIO_CHANNEL_INT_NORMAL;

    case AUDIO_CHANNEL_CONTENT:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_CONTENT_HIDDEN
               : AUDIO_CHANNEL_INT_CONTENT;

    case AUDIO_CHANNEL_NOTIFICATION:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_NOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_NOTIFICATION;

    case AUDIO_CHANNEL_ALARM:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_ALARM_HIDDEN
               : AUDIO_CHANNEL_INT_ALARM;

    case AUDIO_CHANNEL_TELEPHONY:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_TELEPHONY_HIDDEN
               : AUDIO_CHANNEL_INT_TELEPHONY;

    case AUDIO_CHANNEL_RINGER:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_RINGER_HIDDEN
               : AUDIO_CHANNEL_INT_RINGER;

    case AUDIO_CHANNEL_PUBLICNOTIFICATION:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_PUBLICNOTIFICATION_HIDDEN
               : AUDIO_CHANNEL_INT_PUBLICNOTIFICATION;

    case AUDIO_CHANNEL_LAST:
    default:
      break;
  }

  MOZ_CRASH("unexpected audio channel type");
}
