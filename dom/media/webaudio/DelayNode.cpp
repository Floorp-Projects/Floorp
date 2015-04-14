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
#include "DelayBuffer.h"
#include "PlayingRefChangeHandler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DelayNode, AudioNode,
                                   mDelay)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DelayNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DelayNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DelayNode, AudioNode)

class DelayNodeEngine : public AudioNodeEngine
{
  typedef PlayingRefChangeHandler PlayingRefChanged;
public:
  DelayNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination,
                  double aMaxDelayTicks)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    // Keep the default value in sync with the default value in DelayNode::DelayNode.
    , mDelay(0.f)
    // Use a smoothing range of 20ms
    , mBuffer(std::max(aMaxDelayTicks,
                       static_cast<double>(WEBAUDIO_BLOCK_SIZE)),
              WebAudioUtils::ComputeSmoothingRate(0.02,
                                                  mDestination->SampleRate()))
    , mMaxDelay(aMaxDelayTicks)
    , mHaveProducedBeforeInput(false)
    , mLeftOverData(INT32_MIN)
  {
  }

  virtual DelayNodeEngine* AsDelayNodeEngine() override
  {
    return this;
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
                            TrackRate aSampleRate) override
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

  virtual void ProcessBlock(AudioNodeStream* aStream,
                            const AudioChunk& aInput,
                            AudioChunk* aOutput,
                            bool* aFinished) override
  {
    MOZ_ASSERT(mSource == aStream, "Invalid source stream");
    MOZ_ASSERT(aStream->SampleRate() == mDestination->SampleRate());

    if (!aInput.IsNull()) {
      if (mLeftOverData <= 0) {
        nsRefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::ADDREF);
        aStream->Graph()->
          DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
      }
      mLeftOverData = mBuffer.MaxDelayTicks();
    } else if (mLeftOverData > 0) {
      mLeftOverData -= WEBAUDIO_BLOCK_SIZE;
    } else {
      if (mLeftOverData != INT32_MIN) {
        mLeftOverData = INT32_MIN;
        // Delete our buffered data now we no longer need it
        mBuffer.Reset();

        nsRefPtr<PlayingRefChanged> refchanged =
          new PlayingRefChanged(aStream, PlayingRefChanged::RELEASE);
        aStream->Graph()->
          DispatchToMainThreadAfterStreamStateUpdate(refchanged.forget());
      }
      *aOutput = aInput;
      return;
    }

    mBuffer.Write(aInput);

    // Skip output update if mLastChunks has already been set by
    // ProduceBlockBeforeInput() when in a cycle.
    if (!mHaveProducedBeforeInput) {
      UpdateOutputBlock(aOutput, 0.0);
    }
    mHaveProducedBeforeInput = false;
    mBuffer.NextBlock();
  }

  void UpdateOutputBlock(AudioChunk* aOutput, double minDelay)
  {
    double maxDelay = mMaxDelay;
    double sampleRate = mSource->SampleRate();
    ChannelInterpretation channelInterpretation =
      mSource->GetChannelInterpretation();
    if (mDelay.HasSimpleValue()) {
      // If this DelayNode is in a cycle, make sure the delay value is at least
      // one block, even if that is greater than maxDelay.
      double delayFrames = mDelay.GetValue() * sampleRate;
      double delayFramesClamped =
        std::max(minDelay, std::min(delayFrames, maxDelay));
      mBuffer.Read(delayFramesClamped, aOutput, channelInterpretation);
    } else {
      // Compute the delay values for the duration of the input AudioChunk
      // If this DelayNode is in a cycle, make sure the delay value is at least
      // one block.
      StreamTime tick = mSource->GetCurrentPosition();
      double computedDelay[WEBAUDIO_BLOCK_SIZE];
      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        double delayAtTick = mDelay.GetValueAtTime(tick, counter) * sampleRate;
        double delayAtTickClamped =
          std::max(minDelay, std::min(delayAtTick, maxDelay));
        computedDelay[counter] = delayAtTickClamped;
      }
      mBuffer.Read(computedDelay, aOutput, channelInterpretation);
    }
  }

  virtual void ProduceBlockBeforeInput(AudioChunk* aOutput) override
  {
    if (mLeftOverData <= 0) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else {
      UpdateOutputBlock(aOutput, WEBAUDIO_BLOCK_SIZE);
    }
    mHaveProducedBeforeInput = true;
  }

  virtual size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mSource - probably not owned
    // - mDestination - probably not owned
    // - mDelay - shares ref with AudioNode, don't count
    amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  virtual size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mDelay;
  DelayBuffer mBuffer;
  double mMaxDelay;
  bool mHaveProducedBeforeInput;
  // How much data we have in our buffer which needs to be flushed out when our inputs
  // finish.
  int32_t mLeftOverData;
};

DelayNode::DelayNode(AudioContext* aContext, double aMaxDelay)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Max,
              ChannelInterpretation::Speakers)
  , mDelay(new AudioParam(this, SendDelayToStream, 0.0f, "delayTime"))
{
  DelayNodeEngine* engine =
    new DelayNodeEngine(this, aContext->Destination(),
                        aContext->SampleRate() * aMaxDelay);
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

DelayNode::~DelayNode()
{
}

size_t
DelayNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mDelay->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
DelayNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
DelayNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DelayNodeBinding::Wrap(aCx, this, aGivenProto);
}

void
DelayNode::SendDelayToStream(AudioNode* aNode)
{
  DelayNode* This = static_cast<DelayNode*>(aNode);
  SendTimelineParameterToStream(This, DelayNodeEngine::DELAY, *This->mDelay);
}

}
}
