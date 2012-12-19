/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audiochannelservice_h__
#define mozilla_dom_audiochannelservice_h__

#include "nsAutoPtr.h"
#include "nsISupports.h"

#include "AudioChannelCommon.h"
#include "AudioChannelAgent.h"
#include "nsDataHashtable.h"

namespace mozilla {
namespace dom {

class AudioChannelService : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  /**
   * Returns the AudioChannelServce singleton. Only to be called from main thread.
   * @return NS_OK on proper assignment, NS_ERROR_FAILURE otherwise.
   */
  static AudioChannelService*
  GetAudioChannelService();

  /**
   * Shutdown the singleton.
   */
  static void Shutdown();

  /**
   * Any audio channel agent that starts playing should register itself to
   * this service, sharing the AudioChannelType.
   */
  virtual void RegisterAudioChannelAgent(AudioChannelAgent* aAgent,
                                         AudioChannelType aType);

  /**
   * Any  audio channel agent that stops playing should unregister itself to
   * this service.
   */
  virtual void UnregisterAudioChannelAgent(AudioChannelAgent* aAgent);

  /**
   * Return true if this type should be muted.
   */
  virtual bool GetMuted(AudioChannelType aType, bool aElementHidden);

protected:
  void Notify();

  /* Register/Unregister IPC types: */
  void RegisterType(AudioChannelType aType);
  void UnregisterType(AudioChannelType aType);

  AudioChannelService();
  virtual ~AudioChannelService();

  bool ChannelsActiveWithHigherPriorityThan(AudioChannelType aType);

  const char* ChannelName(AudioChannelType aType);

  nsDataHashtable< nsPtrHashKey<AudioChannelAgent>, AudioChannelType > mAgents;

  int32_t* mChannelCounters;

  AudioChannelType mCurrentHigherChannel;

  // This is needed for IPC comunication between
  // AudioChannelServiceChild and this class.
  friend class ContentParent;
  friend class ContentChild;
};

} // namespace dom
} // namespace mozilla

#endif
