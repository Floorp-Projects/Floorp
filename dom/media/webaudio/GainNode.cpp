/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GainNode.h"
#include "mozilla/dom/GainNodeBinding.h"
#include "AlignmentUtils.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(GainNode, AudioNode, mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GainNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(GainNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(GainNode, AudioNode)

class GainNodeEngine final : public AudioNodeEngine {
 public:
  GainNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Stream())
        // Keep the default value in sync with the default value in
        // GainNode::GainNode.
        ,
        mGain(1.f) {}

  enum Parameters { GAIN };
  void RecvTimelineEvent(uint32_t aIndex, AudioTimelineEvent& aEvent) override {
    MOZ_ASSERT(mDestination);
    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent, mDestination);

    switch (aIndex) {
      case GAIN:
        mGain.InsertEvent<int64_t>(aEvent);
        break;
      default:
        NS_ERROR("Bad GainNodeEngine TimelineParameter");
    }
  }

  void ProcessBlock(AudioNodeStream* aStream, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
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
      aOutput->AllocateChannels(aInput.ChannelCount());

      // Compute the gain values for the duration of the input AudioChunk
      StreamTime tick = mDestination->GraphTimeToStreamTime(aFrom);
      float computedGain[WEBAUDIO_BLOCK_SIZE + 4];
      float* alignedComputedGain = ALIGNED16(computedGain);
      ASSERT_ALIGNED16(alignedComputedGain);
      mGain.GetValuesAtTime(tick, alignedComputedGain, WEBAUDIO_BLOCK_SIZE);

      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        alignedComputedGain[counter] *= aInput.mVolume;
      }

      // Apply the gain to the output buffer
      for (size_t channel = 0; channel < aOutput->ChannelCount(); ++channel) {
        const float* inputBuffer =
            static_cast<const float*>(aInput.mChannelData[channel]);
        float* buffer = aOutput->ChannelFloatsForWrite(channel);
        AudioBlockCopyChannelWithScale(inputBuffer, alignedComputedGain,
                                       buffer);
      }
    }
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Not owned:
    // - mDestination - MediaStreamGraphImpl::CollectSizesForMemoryReport()
    // accounts for mDestination.
    // - mGain - Internal ref owned by AudioNode
    return AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<AudioNodeStream> mDestination;
  AudioParamTimeline mGain;
};

GainNode::GainNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers) {
  CreateAudioParam(mGain, GainNodeEngine::GAIN, "gain", 1.0f);
  GainNodeEngine* engine = new GainNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(
      aContext, engine, AudioNodeStream::NO_STREAM_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<GainNode> GainNode::Create(AudioContext& aAudioContext,
                                            const GainOptions& aOptions,
                                            ErrorResult& aRv) {
  RefPtr<GainNode> audioNode = new GainNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->Gain()->SetValue(aOptions.mGain);
  return audioNode.forget();
}

size_t GainNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mGain->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t GainNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* GainNode::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) {
  return GainNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
