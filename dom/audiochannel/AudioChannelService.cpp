/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelService.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"

#include "mozilla/dom/ContentParent.h"

#include "base/basictypes.h"

#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

StaticRefPtr<AudioChannelService> gAudioChannelService;

// static
AudioChannelService*
AudioChannelService::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

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
  if (gAudioChannelService) {
    delete gAudioChannelService;
    gAudioChannelService = nullptr;
  }
}

NS_IMPL_ISUPPORTS0(AudioChannelService)

AudioChannelService::AudioChannelService()
{
  mChannelCounters = new int32_t[AUDIO_CHANNEL_PUBLICNOTIFICATION+1];

  for (int i = AUDIO_CHANNEL_NORMAL;
       i <= AUDIO_CHANNEL_PUBLICNOTIFICATION;
       ++i) {
    mChannelCounters[i] = 0;
  }

  // Creation of the hash table.
  mMediaElements.Init();
}

AudioChannelService::~AudioChannelService()
{
  delete [] mChannelCounters;
}

void
AudioChannelService::RegisterMediaElement(nsHTMLMediaElement* aMediaElement,
                                          AudioChannelType aType)
{
  mMediaElements.Put(aMediaElement, aType);
  mChannelCounters[aType]++;

  // In order to avoid race conditions, it's safer to notify any existing
  // media element any time a new one is registered.
  Notify();
}

void
AudioChannelService::UnregisterMediaElement(nsHTMLMediaElement* aMediaElement)
{
  AudioChannelType type;
  if (!mMediaElements.Get(aMediaElement, &type)) {
    return;
  }

  mMediaElements.Remove(aMediaElement);

  mChannelCounters[type]--;
  MOZ_ASSERT(mChannelCounters[type] >= 0);

  // In order to avoid race conditions, it's safer to notify any existing
  // media element any time a new one is registered.
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
      case AUDIO_CHANNEL_PUBLICNOTIFICATION:
        // Nothing to do
        break;
    }
  }

  // Priorities:
  switch (aType) {
    case AUDIO_CHANNEL_NORMAL:
    case AUDIO_CHANNEL_CONTENT:
      return !!mChannelCounters[AUDIO_CHANNEL_NOTIFICATION] ||
             !!mChannelCounters[AUDIO_CHANNEL_ALARM] ||
             !!mChannelCounters[AUDIO_CHANNEL_TELEPHONY] ||
             !!mChannelCounters[AUDIO_CHANNEL_PUBLICNOTIFICATION];

    case AUDIO_CHANNEL_NOTIFICATION:
    case AUDIO_CHANNEL_ALARM:
    case AUDIO_CHANNEL_TELEPHONY:
      return ChannelsActiveWithHigherPriorityThan(aType);

    case AUDIO_CHANNEL_PUBLICNOTIFICATION:
      return false;
  }

  return false;
}


static PLDHashOperator
NotifyEnumerator(nsHTMLMediaElement* aElement,
                 AudioChannelType aType, void* aData)
{
  if (aElement) {
    aElement->NotifyAudioChannelStateChanged();
  }
  return PL_DHASH_NEXT;
}

void
AudioChannelService::Notify()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Notify any media element for the main process.
  mMediaElements.EnumerateRead(NotifyEnumerator, nullptr);
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
