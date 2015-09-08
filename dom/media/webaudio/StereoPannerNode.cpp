/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StereoPannerNode.h"
#include "mozilla/dom/StereoPannerNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "PanningUtils.h"
#include "AudioParamTimeline.h"
#include "AudioParam.h"

namespace mozilla {
namespace dom {

using namespace std;

NS_IMPL_CYCLE_COLLECTION_INHERITED(StereoPannerNode, AudioNode, mPan)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(StereoPannerNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(StereoPannerNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(StereoPannerNode, AudioNode)

class StereoPannerNodeEngine final : public AudioNodeEngine
{
public:
  StereoPannerNodeEngine(AudioNode* aNode,
                         AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(aDestination->Stream())
    // Keep the default value in sync with the default value in
    // StereoPannerNode::StereoPannerNode.
    , mPan(0.f)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    PAN
  };
  void SetTimelineParameter(uint32_t aIndex,
                            const AudioParamTimeline& aValue,
                            TrackRate aSampleRate) override
  {
    switch (aIndex) {
    case PAN:
      MOZ_ASSERT(mSource && mDestination);
      mPan = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mPan, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad StereoPannerNode TimelineParameter");
    }
  }

  void GetGainValuesForPanning(float aPanning,
                               bool aMonoToStereo,
                               float& aLeftGain,
                               float& aRightGain)
  {
    // Clamp and normalize the panning in [0; 1]
    aPanning = std::min(std::max(aPanning, -1.f), 1.f);

    if (aMonoToStereo) {
      aPanning += 1;
      aPanning /= 2;
    } else if (aPanning <= 0) {
      aPanning += 1;
    }

    aLeftGain  = cos(0.5 * M_PI * aPanning);
    aRightGain = sin(0.5 * M_PI * aPanning);
  }

  void SetToSilentStereoBlock(AudioBlock* aChunk)
  {
    for (uint32_t channel = 0; channel < 2; channel++) {
      float* samples = aChunk->ChannelFloatsForWrite(channel);
      for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i++) {
        samples[i] = 0.f;
      }
    }
  }

  void UpmixToStereoIfNeeded(const AudioBlock& aInput, AudioBlock* aOutput)
  {
    if (aInput.ChannelCount() == 2) {
      *aOutput = aInput;
    } else {
      MOZ_ASSERT(aInput.ChannelCount() == 1);
      aOutput->AllocateChannels(2);
      const float* input = static_cast<const float*>(aInput.mChannelData[0]);
      for (uint32_t channel = 0; channel < 2; channel++) {
        float* output = aOutput->ChannelFloatsForWrite(channel);
        PodCopy(output, input, WEBAUDIO_BLOCK_SIZE);
      }
    }
  }

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioBlock& aInput,
                            AudioBlock* aOutput,
                            bool *aFinished) override
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");

    // The output of this node is always stereo, no matter what the inputs are.
    MOZ_ASSERT(aInput.ChannelCount() <= 2);
    aOutput->AllocateChannels(2);
    bool monoToStereo = aInput.ChannelCount() == 1;

    if (aInput.IsNull()) {
      // If input is silent, so is the output
      SetToSilentStereoBlock(aOutput);
    } else if (mPan.HasSimpleValue()) {
      float panning = mPan.GetValue();
      // If the panning is 0.0, we can simply copy the input to the
      // output, up-mixing to stereo if needed.
      if (panning == 0.0f) {
        UpmixToStereoIfNeeded(aInput, aOutput);
      } else {
        // Optimize the case where the panning is constant for this processing
        // block: we can just apply a constant gain on the left and right
        // channel
        float gainL, gainR;

        GetGainValuesForPanning(panning, monoToStereo, gainL, gainR);
        ApplyStereoPanning(aInput, aOutput,
                           gainL * aInput.mVolume,
                           gainR * aInput.mVolume,
                           panning <= 0);
      }
    } else {
      float computedGain[2][WEBAUDIO_BLOCK_SIZE];
      bool onLeft[WEBAUDIO_BLOCK_SIZE];

      float values[WEBAUDIO_BLOCK_SIZE];
      StreamTime tick = aStream->GetCurrentPosition();
      mPan.GetValuesAtTime(tick, values, WEBAUDIO_BLOCK_SIZE);

      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        float left, right;
        GetGainValuesForPanning(values[counter], monoToStereo, left, right);

        computedGain[0][counter] = left * aInput.mVolume;
        computedGain[1][counter] = right * aInput.mVolume;
        onLeft[counter] = values[counter] <= 0;
      }

      // Apply the gain to the output buffer
      ApplyStereoPanning(aInput, aOutput, computedGain[0], computedGain[1], onLeft);
    }
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mPan;
};

StereoPannerNode::StereoPannerNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Clamped_max,
              ChannelInterpretation::Speakers)
  , mPan(new AudioParam(this, SendPanToStream, 0.f, "pan"))
{
  StereoPannerNodeEngine* engine = new StereoPannerNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(aContext, engine,
                                    AudioNodeStream::NO_STREAM_FLAGS);
  engine->SetSourceStream(mStream);
}

StereoPannerNode::~StereoPannerNode()
{
}

size_t
StereoPannerNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mPan->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
StereoPannerNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
StereoPannerNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StereoPannerNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
StereoPannerNode::SendPanToStream(AudioNode* aNode)
{
  StereoPannerNode* This = static_cast<StereoPannerNode*>(aNode);
  SendTimelineParameterToStream(This, StereoPannerNodeEngine::PAN, *This->mPan);
}

} // namespace dom
} // namespace mozilla
