/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptProcessorNode_h_
#define ScriptProcessorNode_h_

#include "AudioNode.h"
#include "nsAutoPtr.h"

namespace mozilla {

class AudioNodeStream;

namespace dom {

class AudioContext;
class ScriptProcessorNodeEngine;
class SharedBuffers;

class ScriptProcessorNode : public AudioNode
{
public:
  ScriptProcessorNode(AudioContext* aContext,
                      uint32_t aBufferSize,
                      uint32_t aNumberOfInputChannels,
                      uint32_t aNumberOfOutputChannels);
  virtual ~ScriptProcessorNode();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(ScriptProcessorNode, AudioNode)

  IMPL_EVENT_HANDLER(audioprocess)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  virtual void Connect(AudioNode& aDestination, uint32_t aOutput,
                       uint32_t aInput, ErrorResult& aRv) MOZ_OVERRIDE
  {
    AudioNode::Connect(aDestination, aOutput, aInput, aRv);
    if (!aRv.Failed()) {
      mPlayingRef.Take(this);
    }
  }

  virtual void Connect(AudioParam& aDestination, uint32_t aOutput,
                       ErrorResult& aRv) MOZ_OVERRIDE
  {
    AudioNode::Connect(aDestination, aOutput, aRv);
    if (!aRv.Failed()) {
      mPlayingRef.Take(this);
    }
  }

  virtual void Disconnect(uint32_t aOutput, ErrorResult& aRv) MOZ_OVERRIDE
  {
    AudioNode::Disconnect(aOutput, aRv);
    if (!aRv.Failed()) {
      mPlayingRef.Drop(this);
    }
  }

  uint32_t BufferSize() const
  {
    return mBufferSize;
  }

  SharedBuffers* GetSharedBuffers() const
  {
    return mSharedBuffers;
  }

  uint32_t NumberOfOutputChannels() const
  {
    return mNumberOfOutputChannels;
  }

  using nsDOMEventTargetHelper::DispatchTrustedEvent;

  void Stop()
  {
    mPlayingRef.Drop(this);
  }

private:
  nsAutoPtr<SharedBuffers> mSharedBuffers;
  const uint32_t mBufferSize;
  const uint32_t mNumberOfOutputChannels;
  SelfReference<ScriptProcessorNode> mPlayingRef; // a reference to self while planing
};

}
}

#endif

