/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservicechild_h__
#define mozilla_dom_audiochannelservicechild_h__

#include "nsAutoPtr.h"
#include "nsISupports.h"

#include "AudioChannelService.h"
#include "AudioChannelCommon.h"

namespace mozilla {
namespace dom {

class AudioChannelServiceChild : public AudioChannelService
{
public:

  /**
   * Returns the AudioChannelServce singleton. Only to be called from main
   * thread.
   *
   * @return NS_OK on proper assignment, NS_ERROR_FAILURE otherwise.
   */
  static AudioChannelService* GetAudioChannelService();

  static void Shutdown();

  virtual void RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                         AudioChannelType aType);
  virtual void UnregisterAudioChannelAgent(AudioChannelAgent* aAgent);

  /**
   * Return the state to indicate this agent should keep playing/
   * fading volume/muted.
   */
  virtual AudioChannelState GetState(AudioChannelAgent* aAgent,
                                     bool aElementHidden);

  virtual void SetDefaultVolumeControlChannel(AudioChannelType aType, bool aHidden);

protected:
  AudioChannelServiceChild();
  virtual ~AudioChannelServiceChild();
};

} // namespace dom
} // namespace mozilla

#endif

