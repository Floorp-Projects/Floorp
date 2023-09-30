/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DelayNode.h"
#include "mozilla/dom/DelayNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "DelayBuffer.h"
#include "PlayingRefChangeHandler.h"
#include "Tracing.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DelayNode, AudioNode, mDelay)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DelayNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DelayNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DelayNode, AudioNode)

class DelayNodeEngine final : public AudioNodeEngine {
  typedef PlayingRefChangeHandler PlayingRefChanged;

 public:
  DelayNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination,
                  float aMaxDelayTicks)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track())
        // Keep the default value in sync with the default value in
        // DelayNode::DelayNode.
        ,
        mDelay(0.f)
        // Use a smoothing range of 20ms
        ,
        mBuffer(
            std::max(aMaxDelayTicks, static_cast<float>(WEBAUDIO_BLOCK_SIZE))),
        mMaxDelay(aMaxDelayTicks),
        mHaveProducedBeforeInput(false),
        mLeftOverData(INT32_MIN) {}

  DelayNodeEngine* AsDelayNodeEngine() override { return this; }

  enum Parameters {
    DELAY,
  };
  void RecvTimelineEvent(uint32_t aIndex, AudioParamEvent& aEvent) override {
    MOZ_ASSERT(mDestination);
    aEvent.ConvertToTicks(mDestination);

    switch (aIndex) {
      case DELAY:
        mDelay.InsertEvent<int64_t>(aEvent);
        break;
      default:
        NS_ERROR("Bad DelayNodeEngine TimelineParameter");
    }
  }

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    MOZ_ASSERT(aTrack->mSampleRate == mDestination->mSampleRate);
    TRACE("DelayNodeEngine::ProcessBlock");

    if (!aInput.IsSilentOrSubnormal()) {
      if (mLeftOverData <= 0) {
        RefPtr<PlayingRefChanged> refchanged =
            new PlayingRefChanged(aTrack, PlayingRefChanged::ADDREF);
        aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      }
      mLeftOverData = mBuffer.MaxDelayTicks();
    } else if (mLeftOverData > 0) {
      mLeftOverData -= WEBAUDIO_BLOCK_SIZE;
    } else {
      if (mLeftOverData != INT32_MIN) {
        mLeftOverData = INT32_MIN;
        aTrack->ScheduleCheckForInactive();

        // Delete our buffered data now we no longer need it
        mBuffer.Reset();

        RefPtr<PlayingRefChanged> refchanged =
            new PlayingRefChanged(aTrack, PlayingRefChanged::RELEASE);
        aTrack->Graph()->DispatchToMainThreadStableState(refchanged.forget());
      }
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    mBuffer.Write(aInput);

    // Skip output update if mLastChunks has already been set by
    // ProduceBlockBeforeInput() when in a cycle.
    if (!mHaveProducedBeforeInput) {
      UpdateOutputBlock(aTrack, aFrom, aOutput, 0.0);
    }
    mHaveProducedBeforeInput = false;
    mBuffer.NextBlock();
  }

  void UpdateOutputBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                         AudioBlock* aOutput, float minDelay) {
    float maxDelay = mMaxDelay;
    float sampleRate = aTrack->mSampleRate;
    ChannelInterpretation channelInterpretation =
        aTrack->GetChannelInterpretation();
    if (mDelay.HasSimpleValue()) {
      // If this DelayNode is in a cycle, make sure the delay value is at least
      // one block, even if that is greater than maxDelay.
      float delayFrames = mDelay.GetValue() * sampleRate;
      float delayFramesClamped =
          std::max(minDelay, std::min(delayFrames, maxDelay));
      mBuffer.Read(delayFramesClamped, aOutput, channelInterpretation);
    } else {
      // Compute the delay values for the duration of the input AudioChunk
      // If this DelayNode is in a cycle, make sure the delay value is at least
      // one block.
      TrackTime tick = mDestination->GraphTimeToTrackTime(aFrom);
      float values[WEBAUDIO_BLOCK_SIZE];
      mDelay.GetValuesAtTime(tick, values, WEBAUDIO_BLOCK_SIZE);

      float computedDelay[WEBAUDIO_BLOCK_SIZE];
      for (size_t counter = 0; counter < WEBAUDIO_BLOCK_SIZE; ++counter) {
        float delayAtTick = values[counter] * sampleRate;
        float delayAtTickClamped =
            std::max(minDelay, std::min(delayAtTick, maxDelay));
        computedDelay[counter] = delayAtTickClamped;
      }
      mBuffer.Read(computedDelay, aOutput, channelInterpretation);
    }
  }

  void ProduceBlockBeforeInput(AudioNodeTrack* aTrack, GraphTime aFrom,
                               AudioBlock* aOutput) override {
    if (mLeftOverData <= 0) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
    } else {
      UpdateOutputBlock(aTrack, aFrom, aOutput, WEBAUDIO_BLOCK_SIZE);
    }
    mHaveProducedBeforeInput = true;
  }

  bool IsActive() const override { return mLeftOverData != INT32_MIN; }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    // Not owned:
    // - mDestination - probably not owned
    // - mDelay - shares ref with AudioNode, don't count
    amount += mBuffer.SizeOfExcludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

  RefPtr<AudioNodeTrack> mDestination;
  AudioParamTimeline mDelay;
  DelayBuffer mBuffer;
  float mMaxDelay;
  bool mHaveProducedBeforeInput;
  // How much data we have in our buffer which needs to be flushed out when our
  // inputs finish.
  int32_t mLeftOverData;
};

DelayNode::DelayNode(AudioContext* aContext, double aMaxDelay)
    : AudioNode(aContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers) {
  mDelay = CreateAudioParam(DelayNodeEngine::DELAY, u"delayTime"_ns, 0.0f, 0.f,
                            aMaxDelay);
  DelayNodeEngine* engine = new DelayNodeEngine(
      this, aContext->Destination(), aContext->SampleRate() * aMaxDelay);
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<DelayNode> DelayNode::Create(AudioContext& aAudioContext,
                                              const DelayOptions& aOptions,
                                              ErrorResult& aRv) {
  if (aOptions.mMaxDelayTime <= 0. || aOptions.mMaxDelayTime >= 180.) {
    aRv.ThrowNotSupportedError(
        nsPrintfCString("\"maxDelayTime\" (%g) is not in the range (0,180)",
                        aOptions.mMaxDelayTime));
    return nullptr;
  }

  RefPtr<DelayNode> audioNode =
      new DelayNode(&aAudioContext, aOptions.mMaxDelayTime);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->DelayTime()->SetInitialValue(aOptions.mDelayTime);
  return audioNode.forget();
}

size_t DelayNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mDelay->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t DelayNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* DelayNode::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return DelayNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
