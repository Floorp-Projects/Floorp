/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_audio_channel_agent_h__
#define mozilla_dom_audio_channel_agent_h__

#include "nsIAudioChannelAgent.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsWeakPtr.h"

#define NS_AUDIOCHANNELAGENT_CONTRACTID "@mozilla.org/audiochannelagent;1"
// f27688e2-3dd7-11e2-904e-10bf48d64bd4
#define NS_AUDIOCHANNELAGENT_CID {0xf27688e2, 0x3dd7, 0x11e2, \
      {0x90, 0x4e, 0x10, 0xbf, 0x48, 0xd6, 0x4b, 0xd4}}

class nsIDOMWindow;

namespace mozilla {
namespace dom {

/* Header file */
class AudioChannelAgent : public nsIAudioChannelAgent
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIAUDIOCHANNELAGENT

  NS_DECL_CYCLE_COLLECTION_CLASS(AudioChannelAgent)

  AudioChannelAgent();
  virtual void NotifyAudioChannelStateChanged();

  void WindowVolumeChanged();

  nsIDOMWindow* Window() const
  {
    return mWindow;
  }

private:
  virtual ~AudioChannelAgent();

  // Returns mCallback if that's non-null, or otherwise tries to get an
  // nsIAudioChannelAgentCallback out of mWeakCallback.
  already_AddRefed<nsIAudioChannelAgentCallback> GetCallback();

  nsresult InitInternal(nsIDOMWindow* aWindow, int32_t aAudioAgentType,
                        nsIAudioChannelAgentCallback* aCallback,
                        bool aUseWeakRef, bool aWithVideo=false);

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsCOMPtr<nsIAudioChannelAgentCallback> mCallback;
  nsWeakPtr mWeakCallback;
  int32_t mAudioChannelType;
  bool mIsRegToService;
  bool mVisible;
  bool mWithVideo;
};

} // namespace dom
} // namespace mozilla
#endif

