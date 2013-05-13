/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GainNode.h"
#include "mozilla/dom/GainNodeBinding.h"
#include "AudioNodeEngine.h"
#include "GainProcessor.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(GainNode, AudioNode,
                                     mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GainNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(GainNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(GainNode, AudioNode)

class GainNodeEngine : public AudioNodeEngine,
                       public GainProcessor
{
public:
  GainNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , GainProcessor(aDestination)
  {
  }

  enum Parameters {
    GAIN
  };
  void SetTimelineParameter(uint32_t aIndex, const AudioParamTimeline& aValue) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case GAIN:
      SetGainParameter(aValue);
      break;
    default:
      NS_ERROR("Bad GainNodeEngine TimelineParameter");
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    if (aInput.IsNull()) {
      // If input is silent, so is the output
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else {
      if (mGain.HasSimpleValue()) {
        // Copy the input chunk to the output chunk, since we will only be
        // changing the mVolume member.
        *aOutput = aInput;
      } else {
        // Create a new output chunk to avoid modifying the input chunk.
        AllocateAudioBlock(aInput.mChannelData.Length(), aOutput);
      }
      ProcessGain(aStream, aInput.mVolume, aInput.mChannelData, aOutput);
    }
  }
};

GainNode::GainNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mGain(new AudioParam(this, SendGainToStream, 1.0f))
{
  GainNodeEngine* engine = new GainNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

JSObject*
GainNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return GainNodeBinding::Wrap(aCx, aScope, this);
}

void
GainNode::SendGainToStream(AudioNode* aNode)
{
  GainNode* This = static_cast<GainNode*>(aNode);
  SendTimelineParameterToStream(This, GainNodeEngine::GAIN, *This->mGain);
}

}
}

