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
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"

#ifdef MOZ_WIDGET_GONK
#include "SpeakerManagerService.h"
#endif

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

AudioChannelState
AudioChannelServiceChild::GetState(AudioChannelAgent* aAgent, bool aElementHidden)
{
  AudioChannelAgentData* data;
  if (!mAgents.Get(aAgent, &data)) {
    return AUDIO_CHANNEL_STATE_MUTED;
  }

  AudioChannelState state = AUDIO_CHANNEL_STATE_MUTED;
  bool oldElementHidden = data->mElementHidden;

  UpdateChannelType(data->mType, CONTENT_PROCESS_ID_MAIN, aElementHidden, oldElementHidden);

  // Update visibility.
  data->mElementHidden = aElementHidden;

  ContentChild* cc = ContentChild::GetSingleton();
  cc->SendAudioChannelGetState(data->mType, aElementHidden, oldElementHidden, &state);
  data->mState = state;
  cc->SendAudioChannelChangedNotification();

  return state;
}

void
AudioChannelServiceChild::RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                                    AudioChannelType aType,
                                                    bool aWithVideo)
{
  MOZ_ASSERT(aType != AUDIO_CHANNEL_DEFAULT);

  AudioChannelService::RegisterAudioChannelAgent(aAgent, aType, aWithVideo);

  ContentChild::GetSingleton()->SendAudioChannelRegisterType(aType, aWithVideo);

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

  ContentChild::GetSingleton()->SendAudioChannelUnregisterType(
      data.mType, data.mElementHidden, data.mWithVideo);

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(nullptr, "audio-channel-agent-changed", nullptr);
  }
#ifdef MOZ_WIDGET_GONK
  bool active = AnyAudioChannelIsActive();
  for (uint32_t i = 0; i < mSpeakerManager.Length(); i++) {
    mSpeakerManager[i]->SetAudioChannelActive(active);
  }
#endif
}

void
AudioChannelServiceChild::SetDefaultVolumeControlChannel(
  AudioChannelType aType, bool aHidden)
{
  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelChangeDefVolChannel(aType, aHidden);
  }
}
