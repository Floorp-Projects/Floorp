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

#include "base/basictypes.h"

#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_GONK
#include "nsIAudioManager.h"
#endif
using namespace mozilla;
using namespace mozilla::dom;

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

NS_IMPL_ISUPPORTS0(AudioChannelService)

AudioChannelService::AudioChannelService()
: mCurrentHigherChannel(AUDIO_CHANNEL_LAST)
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
  RegisterType(aType, CONTENT_PARENT_UNKNOWN_CHILD_ID);
}

void
AudioChannelService::RegisterType(AudioChannelType aType, uint64_t aChildID)
{
  AudioChannelInternalType type = GetInternalType(aType, true);
  mChannelCounters[type].AppendElement(aChildID);

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    SendAudioChannelChangedNotification();
    Notify();
  }
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  nsAutoPtr<AudioChannelAgentData> data;
  mAgents.RemoveAndForget(aAgent, data);

  if (data) {
    UnregisterType(data->mType, data->mElementHidden, CONTENT_PARENT_UNKNOWN_CHILD_ID);
  }
}

void
AudioChannelService::UnregisterType(AudioChannelType aType,
                                    bool aElementHidden,
                                    uint64_t aChildID)
{
  // The array may contain multiple occurrence of this appId but
  // this should remove only the first one.
  AudioChannelInternalType type = GetInternalType(aType, aElementHidden);
  mChannelCounters[type].RemoveElement(aChildID);

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    SendAudioChannelChangedNotification();
    Notify();
  }
}

bool
AudioChannelService::GetMuted(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return true;
  }

  bool muted = GetMutedInternal(data->mType, CONTENT_PARENT_UNKNOWN_CHILD_ID,
                                aElementHidden, data->mElementHidden);

  // Update visibility.
  data->mElementHidden = aElementHidden;
  data->mMuted = muted;

  SendAudioChannelChangedNotification();
  return muted;
}

bool
AudioChannelService::GetMutedInternal(AudioChannelType aType, uint64_t aChildID,
                                      bool aElementHidden, bool aElementWasHidden)
{
  // Calculating the new and old type and update the hashtable if needed.
  AudioChannelInternalType newType = GetInternalType(aType, aElementHidden);
  AudioChannelInternalType oldType = GetInternalType(aType, aElementWasHidden);

  if (newType != oldType) {
    mChannelCounters[newType].AppendElement(aChildID);
    mChannelCounters[oldType].RemoveElement(aChildID);
  }

  // Let play any visible audio channel.
  if (!aElementHidden) {
    return false;
  }

  bool muted = false;

  // We are not visible, maybe we have to mute.
  if (newType == AUDIO_CHANNEL_INT_NORMAL_HIDDEN ||
      (newType == AUDIO_CHANNEL_INT_CONTENT_HIDDEN &&
       (!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() ||
        HasMoreThanOneContentChannelHidden()))) {
    muted = true;
  }

  if (!muted) {
    MOZ_ASSERT(newType != AUDIO_CHANNEL_INT_NORMAL_HIDDEN);
    muted = ChannelsActiveWithHigherPriorityThan(newType);
  }

  return muted;
}

bool
AudioChannelService::ContentChannelIsActive()
{
  return !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty() ||
         !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].IsEmpty();
}

bool
AudioChannelService::HasMoreThanOneContentChannelHidden()
{
  uint32_t childId = CONTENT_PARENT_UNKNOWN_CHILD_ID;
  bool empty = true;
  for (uint32_t i = 0;
       i < mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].Length();
       ++i) {
    if (empty) {
      childId = mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN][i];
      empty = false;
    } else if (childId != mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN][i]) {
      return true;
    }
  }

  return false;
}

void
AudioChannelService::SendAudioChannelChangedNotification()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return;
  }

  // Calculating the most important active channel.
  AudioChannelType higher = AUDIO_CHANNEL_LAST;

  // Top-Down in the hierarchy.
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

  // Only 1 content channel hidden or a visible one.
  else if ((!mChannelCounters[AUDIO_CHANNEL_INT_CONTENT_HIDDEN].IsEmpty() &&
            !HasMoreThanOneContentChannelHidden()) ||
           !mChannelCounters[AUDIO_CHANNEL_INT_CONTENT].IsEmpty()) {
    higher = AUDIO_CHANNEL_CONTENT;
  }

  // At least 1 visible normal channel.
  else if (!mChannelCounters[AUDIO_CHANNEL_INT_NORMAL].IsEmpty()) {
    higher = AUDIO_CHANNEL_NORMAL;
  }

  if (higher != mCurrentHigherChannel) {
    mCurrentHigherChannel = higher;

    nsString channelName;
    if (mCurrentHigherChannel != AUDIO_CHANNEL_LAST) {
      channelName.AssignASCII(ChannelName(mCurrentHigherChannel));
    } else {
      channelName.AssignLiteral("none");
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(nullptr, "audio-channel-changed", channelName.get());
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

bool
AudioChannelService::ChannelsActiveWithHigherPriorityThan(AudioChannelInternalType aType)
{
  for (int i = AUDIO_CHANNEL_INT_PUBLICNOTIFICATION;
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
    { AUDIO_CHANNEL_CONTENT,            "normal" },
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
    }

    // We don't have to remove the agents from the mAgents hashtable because if
    // that table contains only agents running on the same process.

    SendAudioChannelChangedNotification();
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
               ? AUDIO_CHANNEL_INT_NORMAL_HIDDEN : AUDIO_CHANNEL_INT_NORMAL;

    case AUDIO_CHANNEL_CONTENT:
      return aElementHidden
               ? AUDIO_CHANNEL_INT_CONTENT_HIDDEN : AUDIO_CHANNEL_INT_CONTENT;

    case AUDIO_CHANNEL_NOTIFICATION:
      return AUDIO_CHANNEL_INT_NOTIFICATION;

    case AUDIO_CHANNEL_ALARM:
      return AUDIO_CHANNEL_INT_ALARM;

    case AUDIO_CHANNEL_TELEPHONY:
      return AUDIO_CHANNEL_INT_TELEPHONY;

    case AUDIO_CHANNEL_RINGER:
      return AUDIO_CHANNEL_INT_RINGER;

    case AUDIO_CHANNEL_PUBLICNOTIFICATION:
      return AUDIO_CHANNEL_INT_PUBLICNOTIFICATION;

    case AUDIO_CHANNEL_LAST:
    default:
      break;
  }

  MOZ_NOT_REACHED();
  return AUDIO_CHANNEL_INT_LAST;
}
