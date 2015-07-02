/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AudioDestinationNode_h_
#define AudioDestinationNode_h_

#include "mozilla/dom/AudioChannelBinding.h"
#include "AudioNode.h"
#include "nsIDOMEventListener.h"
#include "nsIAudioChannelAgent.h"
#include "AudioChannelCommon.h"

namespace mozilla {
namespace dom {

class AudioContext;
class EventProxyHandler;

class AudioDestinationNode final : public AudioNode
                                 , public nsIDOMEventListener
                                 , public nsIAudioChannelAgentCallback
                                 , public MainThreadMediaStreamListener
{
public:
  // This node type knows what MediaStreamGraph to use based on
  // whether it's in offline mode.
  AudioDestinationNode(AudioContext* aContext,
                       bool aIsOffline,
                       AudioChannel aChannel = AudioChannel::Normal,
                       uint32_t aNumberOfChannels = 0,
                       uint32_t aLength = 0,
                       float aSampleRate = 0.0f);

  virtual void DestroyMediaStream() override;

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioDestinationNode, AudioNode)
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  virtual JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  virtual uint16_t NumberOfOutputs() const final override
  {
    return 0;
  }

  uint32_t MaxChannelCount() const;
  virtual void SetChannelCount(uint32_t aChannelCount,
                               ErrorResult& aRv) override;

  // Returns the stream or null after unlink.
  AudioNodeStream* Stream() { return mStream; }

  void Mute();
  void Unmute();

  void StartRendering(Promise* aPromise);

  void OfflineShutdown();

  // nsIDOMEventListener - by proxy
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) override;

  AudioChannel MozAudioChannelType() const;
  void SetMozAudioChannelType(AudioChannel aValue, ErrorResult& aRv);

  virtual void NotifyMainThreadStreamFinished() override;
  void FireOfflineCompletionEvent();

  // An amount that should be added to the MediaStream's current time to
  // get the AudioContext.currentTime.
  double ExtraCurrentTime();

  // When aIsOnlyNode is true, this is the only node for the AudioContext.
  void SetIsOnlyNodeForContext(bool aIsOnlyNode);

  void CreateAudioChannelAgent();

  virtual const char* NodeType() const override
  {
    return "AudioDestinationNode";
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

  void InputMuted(bool aInputMuted);
  void ResolvePromise(AudioBuffer* aRenderedBuffer);

protected:
  virtual ~AudioDestinationNode();

private:
  bool CheckAudioChannelPermissions(AudioChannel aValue);

  void SetCanPlay(bool aCanPlay);

  void NotifyStableState();
  void ScheduleStableStateNotification();

  SelfReference<AudioDestinationNode> mOfflineRenderingRef;
  uint32_t mFramesToProduce;

  nsCOMPtr<nsIAudioChannelAgent> mAudioChannelAgent;

  nsRefPtr<EventProxyHandler> mEventProxyHelper;
  nsRefPtr<Promise> mOfflineRenderingPromise;

  // Audio Channel Type.
  AudioChannel mAudioChannel;
  bool mIsOffline;
  bool mAudioChannelAgentPlaying;

  TimeStamp mStartedBlockingDueToBeingOnlyNode;
  double mExtraCurrentTime;
  double mExtraCurrentTimeSinceLastStartedBlocking;
  bool mExtraCurrentTimeUpdatedSinceLastStableState;
};

}
}

#endif

