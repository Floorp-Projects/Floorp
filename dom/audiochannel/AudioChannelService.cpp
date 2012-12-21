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
: mCurrentHigherChannel(AUDIO_CHANNEL_NORMAL)
{
  mChannelCounters = new int32_t[AUDIO_CHANNEL_PUBLICNOTIFICATION+1];

  for (int i = AUDIO_CHANNEL_NORMAL;
       i <= AUDIO_CHANNEL_PUBLICNOTIFICATION;
       ++i) {
    mChannelCounters[i] = 0;
  }

  // Creation of the hash table.
  mAgents.Init();
}

AudioChannelService::~AudioChannelService()
{
  delete [] mChannelCounters;
}

void
AudioChannelService::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                          AudioChannelType aType)
{
  mAgents.Put(aAgent, aType);
  RegisterType(aType);
}

void
AudioChannelService::RegisterType(AudioChannelType aType)
{
  mChannelCounters[aType]++;

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  Notify();
}

void
AudioChannelService::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  AudioChannelType type;
  if (!mAgents.Get(aAgent, &type)) {
    return;
  }

  mAgents.Remove(aAgent);
  UnregisterType(type);
}

void
AudioChannelService::UnregisterType(AudioChannelType aType)
{
  mChannelCounters[aType]--;
  MOZ_ASSERT(mChannelCounters[aType] >= 0);

  bool isNoChannelUsed = true;
  for (int32_t type = AUDIO_CHANNEL_NORMAL;
         type <= AUDIO_CHANNEL_PUBLICNOTIFICATION;
         ++type) {
    if (mChannelCounters[type]) {
      isNoChannelUsed = false;
      break;
    }
  }

  if (isNoChannelUsed) {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    obs->NotifyObservers(nullptr, "audio-channel-changed", NS_LITERAL_STRING("default").get());
    return;
  }

  // In order to avoid race conditions, it's safer to notify any existing
  // agent any time a new one is registered.
  Notify();
}

bool
AudioChannelService::GetMuted(AudioChannelType aType, bool aElementHidden)
{
  // We are not visible, maybe we have to mute:
  if (aElementHidden) {
    switch (aType) {
      case AUDIO_CHANNEL_NORMAL:
        return true;

      case AUDIO_CHANNEL_CONTENT:
        // TODO: this should work per apps
        if (mChannelCounters[AUDIO_CHANNEL_CONTENT] > 1)
          return true;
        break;

      case AUDIO_CHANNEL_NOTIFICATION:
      case AUDIO_CHANNEL_ALARM:
      case AUDIO_CHANNEL_TELEPHONY:
      case AUDIO_CHANNEL_RINGER:
      case AUDIO_CHANNEL_PUBLICNOTIFICATION:
        // Nothing to do
        break;

      case AUDIO_CHANNEL_LAST:
        MOZ_NOT_REACHED();
        return false;
    }
  }

  bool muted = false;

  // Priorities:
  switch (aType) {
    case AUDIO_CHANNEL_NORMAL:
    case AUDIO_CHANNEL_CONTENT:
      muted = !!mChannelCounters[AUDIO_CHANNEL_NOTIFICATION] ||
              !!mChannelCounters[AUDIO_CHANNEL_ALARM] ||
              !!mChannelCounters[AUDIO_CHANNEL_TELEPHONY] ||
              !!mChannelCounters[AUDIO_CHANNEL_RINGER] ||
              !!mChannelCounters[AUDIO_CHANNEL_PUBLICNOTIFICATION];
      break;

    case AUDIO_CHANNEL_NOTIFICATION:
    case AUDIO_CHANNEL_ALARM:
    case AUDIO_CHANNEL_TELEPHONY:
    case AUDIO_CHANNEL_RINGER:
      muted = ChannelsActiveWithHigherPriorityThan(aType);
      break;

    case AUDIO_CHANNEL_PUBLICNOTIFICATION:
      break;

    case AUDIO_CHANNEL_LAST:
      MOZ_NOT_REACHED();
      return false;
  }

  // Notification if needed.
  if (!muted) {

    // Calculating the most important unmuted channel:
    AudioChannelType higher = AUDIO_CHANNEL_NORMAL;
    for (int32_t type = AUDIO_CHANNEL_NORMAL;
         type <= AUDIO_CHANNEL_PUBLICNOTIFICATION;
         ++type) {
      if (mChannelCounters[type]) {
        higher = (AudioChannelType)type;
      }
    }

    if (higher != mCurrentHigherChannel) {
      mCurrentHigherChannel = higher;

      nsString channelName;
      channelName.AssignASCII(ChannelName(mCurrentHigherChannel));

      nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
      obs->NotifyObservers(nullptr, "audio-channel-changed", channelName.get());
    }
  }

  return muted;
}


static PLDHashOperator
NotifyEnumerator(AudioChannelAgent* aAgent,
                 AudioChannelType aType, void* aData)
{
  if (aAgent) {
    aAgent->NotifyAudioChannelStateChanged();
  }
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
AudioChannelService::ChannelsActiveWithHigherPriorityThan(AudioChannelType aType)
{
  for (int i = AUDIO_CHANNEL_PUBLICNOTIFICATION;
       i != AUDIO_CHANNEL_CONTENT; --i) {
    if (i == aType) {
      return false;
    }

    if (mChannelCounters[i]) {
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

