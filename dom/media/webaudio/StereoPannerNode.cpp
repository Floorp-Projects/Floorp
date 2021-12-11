/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StereoPannerNode.h"
#include "mozilla/dom/StereoPannerNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioDestinationNode.h"
#include "AlignmentUtils.h"
#include "WebAudioUtils.h"
#include "PanningUtils.h"
#include "AudioParamTimeline.h"
#include "AudioParam.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(StereoPannerNode, AudioNode, mPan)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StereoPannerNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(StereoPannerNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(StereoPannerNode, AudioNode)

class StereoPannerNodeEngine final : public AudioNodeEngine {
 public:
  StereoPannerNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track())
        // Keep the default value in sync with the default value in
        // StereoPannerNode::StereoPannerNode.
        ,
        mPan(0.f) {}

  enum Parameters { PAN };
  void RecvTimelineEvent(uint32_t aIndex, AudioTimelineEvent& aEvent) override {
    MOZ_ASSERT(mDestination);
    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent, mDestination);

    switch (aIndex) {
      case PAN:
        mPan.InsertEvent<int64_t>(aEvent);
        break;
      default:
        NS_ERROR("Bad StereoPannerNode TimelineParameter");
    }
  }

  void GetGainValuesForPanning(float aPanning, bool aMonoToStereo,
                               float& aLeftGain, float& aRightGain) {
    // Clamp and normalize the panning in [0; 1]
    aPanning = std::min(std::max(aPanning, -1.f), 1.f);

    if (aMonoToStereo) {
      aPanning += 1;
      aPanning /= 2;
    } else if (aPanning <= 0) {
      aPanning += 1;
    }

    aLeftGain = cos(0.5 * M_PI * aPanning);
    aRightGain = sin(0.5 * M_PI * aPanning);
  }

  void SetToSilentStereoBlock(AudioBlock* aChunk) {
    for (uint32_t channel = 0; channel < 2; channel++) {
      float* samples = aChunk->ChannelFloatsForWrite(channel);
      for (uint32_t i = 0; i < WEBAUDIO_BLOCK_SIZE; i++) {
        samples[i] = 0.f;
      }
    }
  }

  void UpmixToStereoIfNeeded(const AudioBlock& aInput, AudioBlock* aOutput) {
    if (aInput.ChannelCount() == 2) {
      *aOutput = aInput;
    } else {
      MOZ_ASSERT(aInput.ChannelCount() == 1);
      aOutput->SetBuffer(aInput.GetBuffer());
      aOutput->mChannelData.SetLength(2);
      for (uint32_t i = 0; i < 2; ++i) {
        aOutput->mChannelData[i] = aInput.ChannelData<float>()[0];
      }
      // 1/sqrt(2) multiplier is because StereoPanner up-mixing differs from
      // input up-mixing.
      aOutput->mVolume = M_SQRT1_2 * aInput.mVolume;
      aOutput->mBufferFormat = AUDIO_FORMAT_FLOAT32;
    }
  }

  virtual void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                            const AudioBlock& aInput, AudioBlock* aOutput,
                            bool* aFinished) override {
    // The output of this node is always stereo, no matter what the inputs are.
    MOZ_ASSERT(aInput.ChannelCount() <= 2);
    bool monoToStereo = aInput.ChannelCount() == 1;

    if (aInput.IsNull()) {
      // If input is silent, so is the output
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else if (mPan.HasSimpleValue()) {
      float panning = mPan.GetValue();
      // If the panning is 0.0, we can simply copy the input to the
      // output with gain applied, up-mixing to stereo if needed.
      if (panning == 0.0f) {
        UpmixToStereoIfNeeded(aInput, aOutput);
      } else {
        // Optimize the case where the panning is constant for this processing
        // block: we can just apply a constant gain on the left and right
        // channel
        float gainL, gainR;

        GetGainValuesForPanning(panning, monoToStereo, gainL, gainR);
        ApplyStereoPanning(aInput, aOutput, gainL, gainR, panning <= 0);
      }
    } else {
      float computedGain[2 * WEBAUDIO_BLOCK_SIZE + 4];
      bool onLeft[WEBAUDIO_BLOCK_SIZE];

      float values[WEBAUDIO_BLOCK_SIZE];
      TrackTime tick = mDestination->GraphTimeToTrackTime(aFrom);
      mPan.GetValuesAtTime(tick, values, WEBAUDIO_BLOCK_SIZE);

      float* alignedComputedGain = ALIGNED16(computedGain);
      ASSERT_ALIGNED16(alignedComputedGain);
      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        float left, right;
        GetGainValuesForPanning(values[counter], monoToStereo, left, right);

        alignedComputedGain[counter] = left;
        alignedComputedGain[WEBAUDIO_BLOCK_SIZE + counter] = right;
        onLeft[counter] = values[counter] <= 0;
      }

      // Apply the gain to the output buffer
      ApplyStereoPanning(aInput, aOutput, alignedComputedGain,
                         &alignedComputedGain[WEBAUDIO_BLOCK_SIZE], onLeft);
    }
  }

  virtual size_t SizeOfIncludingThis(
      MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<AudioNodeTrack> mDestination;
  AudioParamTimeline mPan;
};

StereoPannerNode::StereoPannerNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Clamped_max,
                ChannelInterpretation::Speakers) {
  mPan =
      CreateAudioParam(StereoPannerNodeEngine::PAN, u"pan"_ns, 0.f, -1.f, 1.f);
  StereoPannerNodeEngine* engine =
      new StereoPannerNodeEngine(this, aContext->Destination());
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<StereoPannerNode> StereoPannerNode::Create(
    AudioContext& aAudioContext, const StereoPannerOptions& aOptions,
    ErrorResult& aRv) {
  RefPtr<StereoPannerNode> audioNode = new StereoPannerNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->Pan()->SetInitialValue(aOptions.mPan);
  return audioNode.forget();
}

size_t StereoPannerNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mPan->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t StereoPannerNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* StereoPannerNode::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return StereoPannerNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
