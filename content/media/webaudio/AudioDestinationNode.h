/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioDestinationNode_h_
#define AudioDestinationNode_h_

#include "mozilla/dom/AudioContextBinding.h"
#include "AudioNode.h"
#include "nsIDOMEventListener.h"
#include "nsIAudioChannelAgent.h"
#include "AudioChannelCommon.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace dom {

class AudioContext;

class AudioDestinationNode : public AudioNode
                           , public nsIDOMEventListener
                           , public nsIAudioChannelAgentCallback
                           , public nsSupportsWeakReference
{
public:
  // This node type knows what MediaStreamGraph to use based on
  // whether it's in offline mode.
  AudioDestinationNode(AudioContext* aContext,
                       bool aIsOffline,
                       uint32_t aNumberOfChannels = 0,
                       uint32_t aLength = 0,
                       float aSampleRate = 0.0f);

  virtual void DestroyMediaStream() MOZ_OVERRIDE;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationNode, AudioNode)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual uint16_t NumberOfOutputs() const MOZ_FINAL MOZ_OVERRIDE
  {
    return 0;
  }

  uint32_t MaxChannelCount() const;
  virtual void SetChannelCount(uint32_t aChannelCount,
                               ErrorResult& aRv) MOZ_OVERRIDE;

  void Mute();
  void Unmute();

  void StartRendering();

  void OfflineShutdown();

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIAudioChannelAgentCallback
  NS_IMETHOD CanPlayChanged(int32_t aCanPlay);

  AudioChannel MozAudioChannelType() const;
  void SetMozAudioChannelType(AudioChannel aValue, ErrorResult& aRv);

private:
  bool CheckAudioChannelPermissions(AudioChannel aValue);
  void CreateAudioChannelAgent();

  void SetCanPlay(bool aCanPlay);

  SelfReference<AudioDestinationNode> mOfflineRenderingRef;
  uint32_t mFramesToProduce;

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;

  // Audio Channel Type.
  AudioChannel mAudioChannel;
};

}
}

#endif

