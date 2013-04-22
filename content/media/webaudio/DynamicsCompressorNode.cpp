/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicsCompressorNode.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_6(DynamicsCompressorNode, AudioNode,
                                     mThreshold,
                                     mKnee,
                                     mRatio,
                                     mReduction,
                                     mAttack,
                                     mRelease)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DynamicsCompressorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DynamicsCompressorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DynamicsCompressorNode, AudioNode)

class DynamicsCompressorNodeEngine : public AudioNodeEngine
{
public:
  explicit DynamicsCompressorNodeEngine(AudioNode* aNode,
                                        AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    // Keep the default value in sync with the default value in
    // DynamicsCompressorNode::DynamicsCompressorNode.
    , mThreshold(-24.f)
    , mKnee(30.f)
    , mRatio(12.f)
    , mReduction(0.f)
    , mAttack(0.003f)
    , mRelease(0.25f)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameters {
    THRESHOLD,
    KNEE,
    RATIO,
    REDUCTION,
    ATTACK,
    RELEASE
  };
  void SetTimelineParameter(uint32_t aIndex, const AudioParamTimeline& aValue) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mSource && mDestination);
    switch (aIndex) {
    case THRESHOLD:
      mThreshold = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mThreshold, mSource, mDestination);
      break;
    case KNEE:
      mKnee = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mKnee, mSource, mDestination);
      break;
    case RATIO:
      mRatio = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mRatio, mSource, mDestination);
      break;
    case REDUCTION:
      mReduction = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mReduction, mSource, mDestination);
      break;
    case ATTACK:
      mAttack = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mAttack, mSource, mDestination);
      break;
    case RELEASE:
      mRelease = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mRelease, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad DynamicsCompresssorNodeEngine TimelineParameter");
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    // TODO: do the necessary computation here
    *aOutput = aInput;
  }

private:
  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  AudioParamTimeline mThreshold;
  AudioParamTimeline mKnee;
  AudioParamTimeline mRatio;
  AudioParamTimeline mReduction;
  AudioParamTimeline mAttack;
  AudioParamTimeline mRelease;
};

DynamicsCompressorNode::DynamicsCompressorNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mThreshold(new AudioParam(this, SendThresholdToStream, -24.f))
  , mKnee(new AudioParam(this, SendKneeToStream, 30.f))
  , mRatio(new AudioParam(this, SendRatioToStream, 12.f))
  , mReduction(new AudioParam(this, SendReductionToStream, 0.f))
  , mAttack(new AudioParam(this, SendAttackToStream, 0.003f))
  , mRelease(new AudioParam(this, SendReleaseToStream, 0.25f))
{
  DynamicsCompressorNodeEngine* engine = new DynamicsCompressorNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

JSObject*
DynamicsCompressorNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return DynamicsCompressorNodeBinding::Wrap(aCx, aScope, this);
}

void
DynamicsCompressorNode::SendThresholdToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::THRESHOLD, *This->mThreshold);
}

void
DynamicsCompressorNode::SendKneeToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::KNEE, *This->mKnee);
}

void
DynamicsCompressorNode::SendRatioToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::RATIO, *This->mRatio);
}

void
DynamicsCompressorNode::SendReductionToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::REDUCTION, *This->mReduction);
}

void
DynamicsCompressorNode::SendAttackToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::ATTACK, *This->mAttack);
}

void
DynamicsCompressorNode::SendReleaseToStream(AudioNode* aNode)
{
  DynamicsCompressorNode* This = static_cast<DynamicsCompressorNode*>(aNode);
  SendTimelineParameterToStream(This, DynamicsCompressorNodeEngine::RELEASE, *This->mRelease);
}

}
}

