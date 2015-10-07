/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeakerManagerServiceChild.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/unused.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "AudioChannelService.h"
#include <cutils/properties.h>

using namespace mozilla;
using namespace mozilla::dom;

StaticRefPtr<SpeakerManagerServiceChild> gSpeakerManagerServiceChild;

// static
SpeakerManagerService*
SpeakerManagerServiceChild::GetOrCreateSpeakerManagerService()
{
  MOZ_ASSERT(NS_IsMainThread());

  // If we already exist, exit early
  if (gSpeakerManagerServiceChild) {
    return gSpeakerManagerServiceChild;
  }

  // Create new instance, register, return
  nsRefPtr<SpeakerManagerServiceChild> service = new SpeakerManagerServiceChild();

  gSpeakerManagerServiceChild = service;

  return gSpeakerManagerServiceChild;
}

// static
SpeakerManagerService*
SpeakerManagerServiceChild::GetSpeakerManagerService()
{
  MOZ_ASSERT(NS_IsMainThread());

  return gSpeakerManagerServiceChild;
}

void
SpeakerManagerServiceChild::ForceSpeaker(bool aEnable, bool aVisible)
{
  mVisible = aVisible;
  mOrgSpeakerStatus = aEnable;
  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendSpeakerManagerForceSpeaker(aEnable && aVisible);
  }
}

bool
SpeakerManagerServiceChild::GetSpeakerStatus()
{
  ContentChild *cc = ContentChild::GetSingleton();
  bool status = false;
  if (cc) {
    cc->SendSpeakerManagerGetSpeakerStatus(&status);
  }
  char propQemu[PROPERTY_VALUE_MAX];
  property_get("ro.kernel.qemu", propQemu, "");
  if (!strncmp(propQemu, "1", 1)) {
    return mOrgSpeakerStatus;
  }
  return status;
}

void
SpeakerManagerServiceChild::Shutdown()
{
  if (gSpeakerManagerServiceChild) {
    gSpeakerManagerServiceChild = nullptr;
  }
}

void
SpeakerManagerServiceChild::SetAudioChannelActive(bool aIsActive)
{
  // Content process and switch to background with no audio and speaker forced.
  // Then disable speaker
  for (uint32_t i = 0; i < mRegisteredSpeakerManagers.Length(); i++) {
    mRegisteredSpeakerManagers[i]->SetAudioChannelActive(aIsActive);
  }
}

SpeakerManagerServiceChild::SpeakerManagerServiceChild()
{
  MOZ_ASSERT(NS_IsMainThread());
  nsRefPtr<AudioChannelService> audioChannelService = AudioChannelService::GetOrCreate();
  if (audioChannelService) {
    audioChannelService->RegisterSpeakerManager(this);
  }
  MOZ_COUNT_CTOR(SpeakerManagerServiceChild);
}

SpeakerManagerServiceChild::~SpeakerManagerServiceChild()
{
  nsRefPtr<AudioChannelService> audioChannelService = AudioChannelService::GetOrCreate();
  if (audioChannelService) {
    audioChannelService->UnregisterSpeakerManager(this);
  }
  MOZ_COUNT_DTOR(SpeakerManagerServiceChild);
}

void
SpeakerManagerServiceChild::Notify()
{
  for (uint32_t i = 0; i < mRegisteredSpeakerManagers.Length(); i++) {
    mRegisteredSpeakerManagers[i]->DispatchSimpleEvent(NS_LITERAL_STRING("speakerforcedchange"));
  }
}
