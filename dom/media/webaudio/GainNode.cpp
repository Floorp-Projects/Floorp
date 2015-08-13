/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GainNode.h"
#include "mozilla/dom/GainNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(GainNode, AudioNode,
                                   mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GainNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(GainNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(GainNode, AudioNode)

class GainNodeEngine final : public AudioNodeEngine
{
public:
  GainNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(aDestination->Stream())
    // Keep the default value in sync with the default value in GainNode::GainNode.
    , mGain(1.f)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    GAIN
  };
  void SetTimelineParameter(uint32_t aIndex,
                            const AudioParamTimeline& aValue,
                            TrackRate aSampleRate) override
  {
    switch (aIndex) {
    case GAIN:
      MOZ_ASSERT(mSource && mDestination);
      mGain = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mGain, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad GainNodeEngine TimelineParameter");
    }
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) override
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    if (aInput.IsNull()) {
      // If input is silent, so is the output
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else if (mGain.HasSimpleValue()) {
      // Optimize the case where we only have a single value set as the volume
      float gain = mGain.GetValue();
      if (gain == 0.0f) {
        aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      } else {
        *aOutput = aInput;
        aOutput->mVolume *= gain;
      }
    } else {
      // First, compute a vector of gains for each track tick based on the
      // timeline at hand, and then for each channel, multiply the values
      // in the buffer with the gain vector.
      AllocateAudioBlock(aInput.mChannelData.Length(), aOutput);

      // Compute the gain values for the duration of the input AudioChunk
      StreamTime tick = aStream->GetCurrentPosition();
      float computedGain[WEBAUDIO_BLOCK_SIZE];
      mGain.GetValuesAtTime(tick, computedGain, WEBAUDIO_BLOCK_SIZE);

      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        computedGain[counter] *= aInput.mVolume;
      }

      // Apply the gain to the output buffer
      for (size_t channel = 0; channel < aOutput->mChannelData.Length(); ++channel) {
        const float* inputBuffer = static_cast<const float*> (aInput.mChannelData[channel]);
        float* buffer = aOutput->ChannelFloatsForWrite(channel);
        AudioBlockCopyChannelWithScale(inputBuffer, computedGain, buffer);
      }
    }
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mSource (probably)
    // - mDestination (probably)
    // - mGain - Internal ref owned by AudioNode
    return AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mGain;
};

GainNode::GainNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mGain(new AudioParam(this, SendGainToStream, 1.0f, "gain"))
{
  GainNodeEngine* engine = new GainNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(aContext->Graph(), engine,
                                    AudioNodeStream::NO_STREAM_FLAGS);
  engine->SetSourceStream(mStream);
}

GainNode::~GainNode()
{
}

size_t
GainNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mGain->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
GainNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
GainNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return GainNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
GainNode::SendGainToStream(AudioNode* aNode)
{
  GainNode* This = static_cast<GainNode*>(aNode);
  SendTimelineParameterToStream(This, GainNodeEngine::GAIN, *This->mGain);
}

} // namespace dom
} // namespace mozilla
