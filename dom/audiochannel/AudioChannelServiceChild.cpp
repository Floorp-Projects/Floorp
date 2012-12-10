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

#include "base/basictypes.h"

#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

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
    delete gAudioChannelServiceChild;
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
AudioChannelServiceChild::GetMuted(AudioChannelType aType, bool aMozHidden)
{
  ContentChild *cc = ContentChild::GetSingleton();
  bool muted = false;

  if (cc) {
    cc->SendAudioChannelGetMuted(aType, aMozHidden, &muted);
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
}

void
AudioChannelServiceChild::UnregisterAudioChannelAgent(AudioChannelAgent* aAgent)
{
  AudioChannelType type;
  if (!mAgents.Get(aAgent, &type)) {
    return;
  }

  AudioChannelService::UnregisterAudioChannelAgent(aAgent);

  ContentChild *cc = ContentChild::GetSingleton();
  if (cc) {
    cc->SendAudioChannelUnregisterType(type);
  }
}

