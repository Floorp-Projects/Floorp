/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayNode.h"
#include "mozilla/dom/DelayNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "DelayProcessor.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(DelayNode, AudioNode,
                                     mDelay)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DelayNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DelayNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DelayNode, AudioNode)

class DelayNodeEngine : public AudioNodeEngine
{
  typedef PlayingRefChangeHandler<DelayNode> PlayingRefChanged;
public:
  DelayNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination,
                  int aMaxDelayFrames)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    // Keep the default value in sync with the default value in DelayNode::DelayNode.
    , mDelay(0.f)
    // Use a smoothing range of 20ms
    , mProcessor(aMaxDelayFrames,
                 WebAudioUtils::ComputeSmoothingRate(0.02,
                                                     mDestination->SampleRate()))
    , mLeftOverData(INT32_MIN)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    DELAY,
  };
  void SetTimelineParameter(uint32_t aIndex,
                            const AudioParamTimeline& aValue,
                            TrackRate aSampleRate) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case DELAY:
      MOZ_ASSERT(mSource && mDestination);
      mDelay = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mDelay, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad DelayNodeEngine TimelineParameter");
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");
    MOZ_ASSERT(aStream->SampleRate() == mDestination->SampleRate());

    const uint32_t numChannels = aInput.IsNull() ?
                                 mProcessor.BufferChannelCount() :
                                 aInput.mChannelData.Length();

    bool playedBackAllLeftOvers = false;
    if (mProcessor.BufferChannelCount() &&
        mLeftOverData == INT32_MIN &&
        aStream->AllInputsFinished()) {
      mLeftOverData = mProcessor.CurrentDelayFrames() - WEBAUDIO_BLOCK_SIZE;

      if (mLeftOverData > 0) {
        nsRefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::ADDREF);
        NS_DispatchToMainThread(refchanged);
      }
    } else if (mLeftOverData != INT32_MIN) {
      mLeftOverData -= WEBAUDIO_BLOCK_SIZE;
      if (mLeftOverData <= 0) {
        // Continue spamming the main thread with messages until we are destroyed.
        // This isn't great, but it ensures a message will get through even if
        // some are ignored by DelayNode::AcceptPlayingRefRelease
        mLeftOverData = 0;
        playedBackAllLeftOvers = true;

        nsRefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::RELEASE);
        NS_DispatchToMainThread(refchanged);
      }
    }

    AllocateAudioBlock(numChannels, aOutput);

    AudioChunk input = aInput;
    if (!aInput.IsNull() && aInput.mVolume != 1.0f) {
      // Pre-multiply the input's volume
      AllocateAudioBlock(numChannels, &input);
      for (uint32_t i = 0; i < numChannels; ++i) {
        const float* src = static_cast<const float*>(aInput.mChannelData[i]);
        float* dest = static_cast<float*>(const_cast<void*>(input.mChannelData[i]));
        AudioBlockCopyChannelWithScale(src, aInput.mVolume, dest);
      }
    }

    const float* const* inputChannels = input.IsNull() ? nullptr :
      reinterpret_cast<const float* const*>(input.mChannelData.Elements());
    float* const* outputChannels = reinterpret_cast<float* const*>
      (const_cast<void* const*>(aOutput->mChannelData.Elements()));

    double sampleRate = aStream->SampleRate();
    if (mDelay.HasSimpleValue()) {
      double delayFrames = mDelay.GetValue() * sampleRate;
      mProcessor.Process(delayFrames, inputChannels, outputChannels,
                         numChannels, WEBAUDIO_BLOCK_SIZE);
    } else {
      // Compute the delay values for the duration of the input AudioChunk
      double computedDelay[WEBAUDIO_BLOCK_SIZE];
      TrackTicks tick = aStream->GetCurrentPosition();
      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        computedDelay[counter] =
          mDelay.GetValueAtTime(tick, counter) * sampleRate;
      }
      mProcessor.Process(computedDelay, inputChannels, outputChannels,
                         numChannels, WEBAUDIO_BLOCK_SIZE);
    }


    if (playedBackAllLeftOvers) {
      // Delete our buffered data once we no longer need it
      mProcessor.Reset();
    }
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mDelay;
  DelayProcessor mProcessor;
  // How much data we have in our buffer which needs to be flushed out when our inputs
  // finish.
  int32_t mLeftOverData;
};

DelayNode::DelayNode(AudioContext* aContext, double aMaxDelay)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mMediaStreamGraphUpdateIndexAtLastInputConnection(0)
  , mDelay(new AudioParam(MOZ_THIS_IN_INITIALIZER_LIST(),
                          SendDelayToStream, 0.0f))
{
  DelayNodeEngine* engine =
    new DelayNodeEngine(this, aContext->Destination(),
                        ceil(aContext->SampleRate() * aMaxDelay));
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

JSObject*
DelayNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return DelayNodeBinding::Wrap(aCx, aScope, this);
}

void
DelayNode::SendDelayToStream(AudioNode* aNode)
{
  DelayNode* This = static_cast<DelayNode*>(aNode);
  SendTimelineParameterToStream(This, DelayNodeEngine::DELAY, *This->mDelay);
}

}
}

