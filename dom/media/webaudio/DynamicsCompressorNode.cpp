/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicsCompressorNode.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"
#include "nsAutoPtr.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "blink/DynamicsCompressor.h"

using WebCore::DynamicsCompressor;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DynamicsCompressorNode, AudioNode,
                                   mThreshold,
                                   mKnee,
                                   mRatio,
                                   mAttack,
                                   mRelease)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DynamicsCompressorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DynamicsCompressorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DynamicsCompressorNode, AudioNode)

class DynamicsCompressorNodeEngine final : public AudioNodeEngine
{
public:
  explicit DynamicsCompressorNodeEngine(AudioNode* aNode,
                                        AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mDestination(aDestination->Stream())
    // Keep the default value in sync with the default value in
    // DynamicsCompressorNode::DynamicsCompressorNode.
    , mThreshold(-24.f)
    , mKnee(30.f)
    , mRatio(12.f)
    , mAttack(0.003f)
    , mRelease(0.25f)
    , mCompressor(new DynamicsCompressor(mDestination->SampleRate(), 2))
  {
  }

  enum Parameters {
    THRESHOLD,
    KNEE,
    RATIO,
    ATTACK,
    RELEASE
  };
  void RecvTimelineEvent(uint32_t aIndex,
                         AudioTimelineEvent& aEvent) override
  {
    MOZ_ASSERT(mDestination);

    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent,
                                                    mDestination);

    switch (aIndex) {
    case THRESHOLD:
      mThreshold.InsertEvent<int64_t>(aEvent);
      break;
    case KNEE:
      mKnee.InsertEvent<int64_t>(aEvent);
      break;
    case RATIO:
      mRatio.InsertEvent<int64_t>(aEvent);
      break;
    case ATTACK:
      mAttack.InsertEvent<int64_t>(aEvent);
      break;
    case RELEASE:
      mRelease.InsertEvent<int64_t>(aEvent);
      break;
    default:
      NS_ERROR("Bad DynamicsCompresssorNodeEngine TimelineParameter");
    }
  }

  void ProcessBlock(AudioNodeStream* aStream,
                    GraphTime aFrom,
                    const AudioBlock& aInput,
                    AudioBlock* aOutput,
                    bool* aFinished) override
  {
    if (aInput.IsNull()) {
      // Just output silence
      *aOutput = aInput;
      return;
    }

    const uint32_t channelCount = aInput.ChannelCount();
    if (mCompressor->numberOfChannels() != channelCount) {
      // Create a new compressor object with a new channel count
      mCompressor = new WebCore::DynamicsCompressor(aStream->SampleRate(),
                                                    aInput.ChannelCount());
    }

    StreamTime pos = mDestination->GraphTimeToStreamTime(aFrom);
    mCompressor->setParameterValue(DynamicsCompressor::ParamThreshold,
                                   mThreshold.GetValueAtTime(pos));
    mCompressor->setParameterValue(DynamicsCompressor::ParamKnee,
                                   mKnee.GetValueAtTime(pos));
    mCompressor->setParameterValue(DynamicsCompressor::ParamRatio,
                                   mRatio.GetValueAtTime(pos));
    mCompressor->setParameterValue(DynamicsCompressor::ParamAttack,
                                   mAttack.GetValueAtTime(pos));
    mCompressor->setParameterValue(DynamicsCompressor::ParamRelease,
                                   mRelease.GetValueAtTime(pos));

    aOutput->AllocateChannels(channelCount);
    mCompressor->process(&aInput, aOutput, aInput.GetDuration());

    SendReductionParamToMainThread(aStream,
                                   mCompressor->parameterValue(DynamicsCompressor::ParamReduction));
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    // Not owned:
    // - mDestination (probably)
    // - Don't count the AudioParamTimelines, their inner refs are owned by the
    // AudioNode.
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mCompressor->sizeOfIncludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override
  {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

private:
  void SendReductionParamToMainThread(AudioNodeStream* aStream, float aReduction)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    class Command final : public Runnable
    {
    public:
      Command(AudioNodeStream* aStream, float aReduction)
        : mStream(aStream)
        , mReduction(aReduction)
      {
      }

      NS_IMETHOD Run() override
      {
        RefPtr<DynamicsCompressorNode> node =
          static_cast<DynamicsCompressorNode*>
            (mStream->Engine()->NodeMainThread());
        if (node) {
          node->SetReduction(mReduction);
        }
        return NS_OK;
      }

    private:
      RefPtr<AudioNodeStream> mStream;
      float mReduction;
    };

    NS_DispatchToMainThread(new Command(aStream, aReduction));
  }

private:
  AudioNodeStream* mDestination;
  AudioParamTimeline mThreshold;
  AudioParamTimeline mKnee;
  AudioParamTimeline mRatio;
  AudioParamTimeline mAttack;
  AudioParamTimeline mRelease;
  nsAutoPtr<DynamicsCompressor> mCompressor;
};

DynamicsCompressorNode::DynamicsCompressorNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Explicit,
              ChannelInterpretation::Speakers)
  , mThreshold(new AudioParam(this, DynamicsCompressorNodeEngine::THRESHOLD,
                              -24.f, "threshold"))
  , mKnee(new AudioParam(this, DynamicsCompressorNodeEngine::KNEE,
                         30.f, "knee"))
  , mRatio(new AudioParam(this, DynamicsCompressorNodeEngine::RATIO,
                          12.f, "ratio"))
  , mReduction(0)
  , mAttack(new AudioParam(this, DynamicsCompressorNodeEngine::ATTACK,
                           0.003f, "attack"))
  , mRelease(new AudioParam(this, DynamicsCompressorNodeEngine::RELEASE,
                            0.25f, "release"))
{
  DynamicsCompressorNodeEngine* engine = new DynamicsCompressorNodeEngine(this, aContext->Destination());
  mStream = AudioNodeStream::Create(aContext, engine,
                                    AudioNodeStream::NO_STREAM_FLAGS);
}

DynamicsCompressorNode::~DynamicsCompressorNode()
{
}

size_t
DynamicsCompressorNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mThreshold->SizeOfIncludingThis(aMallocSizeOf);
  amount += mKnee->SizeOfIncludingThis(aMallocSizeOf);
  amount += mRatio->SizeOfIncludingThis(aMallocSizeOf);
  amount += mAttack->SizeOfIncludingThis(aMallocSizeOf);
  amount += mRelease->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t
DynamicsCompressorNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject*
DynamicsCompressorNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return DynamicsCompressorNodeBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
