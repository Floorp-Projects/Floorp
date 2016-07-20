/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ScriptProcessorNode_h_
#define ScriptProcessorNode_h_

#include "AudioNode.h"

namespace mozilla {
namespace dom {

class AudioContext;
class SharedBuffers;

class ScriptProcessorNode final : public AudioNode
{
public:
  ScriptProcessorNode(AudioContext* aContext,
                      uint32_t aBufferSize,
                      uint32_t aNumberOfInputChannels,
                      uint32_t aNumberOfOutputChannels);

  NS_DECL_ISUPPORTS_INHERITED

  IMPL_EVENT_HANDLER(audioprocess)

  void EventListenerAdded(nsIAtom* aType) override;
  void EventListenerRemoved(nsIAtom* aType) override;

  JSObject* WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  AudioNode* Connect(AudioNode& aDestination, uint32_t aOutput,
                     uint32_t aInput, ErrorResult& aRv) override
  {
    AudioNode* node = AudioNode::Connect(aDestination, aOutput, aInput, aRv);
    if (!aRv.Failed()) {
      UpdateConnectedStatus();
    }
    return node;
  }

  void Connect(AudioParam& aDestination, uint32_t aOutput,
               ErrorResult& aRv) override
  {
    AudioNode::Connect(aDestination, aOutput, aRv);
    if (!aRv.Failed()) {
      UpdateConnectedStatus();
    }
  }
  void Disconnect(ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aRv);
    UpdateConnectedStatus();
  }
  void Disconnect(uint32_t aOutput, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aOutput, aRv);
    UpdateConnectedStatus();
  }
  void NotifyInputsChanged() override
  {
    UpdateConnectedStatus();
  }
  void NotifyHasPhantomInput() override
  {
    mHasPhantomInput = true;
    // No need to UpdateConnectedStatus() because there was previously an
    // input in InputNodes().
  }
  void Disconnect(AudioNode& aDestination, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aDestination, aRv);
    UpdateConnectedStatus();
  }
  void Disconnect(AudioNode& aDestination, uint32_t aOutput, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aDestination, aOutput, aRv);
    UpdateConnectedStatus();
  }
  void Disconnect(AudioNode& aDestination, uint32_t aOutput, uint32_t aInput, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aDestination, aOutput, aInput, aRv);
    UpdateConnectedStatus();
  }
  void Disconnect(AudioParam& aDestination, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aDestination, aRv);
    UpdateConnectedStatus();
  }
  void Disconnect(AudioParam& aDestination, uint32_t aOutput, ErrorResult& aRv) override
  {
    AudioNode::Disconnect(aDestination, aOutput, aRv);
    UpdateConnectedStatus();
  }
  void SetChannelCount(uint32_t aChannelCount, ErrorResult& aRv) override
  {
    if (aChannelCount != ChannelCount()) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    }
    return;
  }
  void SetChannelCountModeValue(ChannelCountMode aMode, ErrorResult& aRv) override
  {
    if (aMode != ChannelCountMode::Explicit) {
      aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    }
    return;
  }

  uint32_t BufferSize() const
  {
    return mBufferSize;
  }

  uint32_t NumberOfOutputChannels() const
  {
    return mNumberOfOutputChannels;
  }

  using DOMEventTargetHelper::DispatchTrustedEvent;

  const char* NodeType() const override
  {
    return "ScriptProcessorNode";
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override;
  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override;

private:
  virtual ~ScriptProcessorNode();

  void UpdateConnectedStatus();

  const uint32_t mBufferSize;
  const uint32_t mNumberOfOutputChannels;
  bool mHasPhantomInput = false;
};

} // namespace dom
} // namespace mozilla

#endif

