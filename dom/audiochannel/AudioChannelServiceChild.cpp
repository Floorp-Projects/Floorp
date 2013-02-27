/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioChannelServiceChild.h"

#include "base/basictypes.h"

#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/Util.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::hal;

StaticRefPtr<AudioChannelServiceChild> gAudioChannelServiceChild;

// static
AudioChannelService*
AudioChannelServiceChild::GetAudioChannelService()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gAudioChannelServiceChild) {
    return gAudioChannelServiceChild;
  }

  // Create new instance, register, return
  nsRefPtr<AudioChannelServiceChild> service = new AudioChannelServiceChild();
  NS_ENSURE_TRUE(service, nullptr);

  gAudioChannelServiceChild = service;
  return gAudioChannelServiceChild;
}

void
AudioChannelServiceChild::Shutdown()
{
  if (gAudioChannelServiceChild) {
    gAudioChannelServiceChild = nullptr;
  }
}

AudioChannelServiceChild::AudioChannelServiceChild()
{
}

AudioChannelServiceChild::~AudioChannelServiceChild()
{
}

bool
AudioChannelServiceChild::GetMuted(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return true;
  }

  ContentChild *cc = ContentChild::GetSingleton();
  bool muted = true;
  bool oldElementHidden = data->mElementHidden;

  UpdateChannelType(data->mType, CONTENT_PROCESS_ID_MAIN, aElementHidden, oldElementHidden);

  // Update visibility.
  data->mElementHidden = aElementHidden;

  if (cc) {
    cc->SendAudioChannelGetMuted(data->mType, aElementHidden, oldElementHidden, &muted);
  }
  data->mMuted = muted;

  if (cc) {
    cc->SendAudioChannelChangedNotification();
  }

  return muted;
}

void
AudioChannelServiceChild::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                                    AudioChannelType aType)
{
  AudioChannelService::RegisterAudioChannelAgent(aAgent, aType);

  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelRegisterType(aType);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "audio-channel-agent-changed", nullptr);
  }
}

void
AudioChannelServiceChild::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  AudioChannelAgentData *pData;
  if (!mAgents.Get(aAgent, &pData)) {
    return;
  }

  // We need to keep a copy because unregister will remove the
  // AudioChannelAgentData object from the hashtable.
  AudioChannelAgentData data(*pData);

  AudioChannelService::UnregisterAudioChannelAgent(aAgent);

  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelUnregisterType(data.mType, data.mElementHidden);
  }

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "audio-channel-agent-changed", nullptr);
  }
}
