/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DynamicsCompressorNode.h"
#include "mozilla/dom/DynamicsCompressorNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "blink/DynamicsCompressor.h"

using WebCore::DynamicsCompressor;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(DynamicsCompressorNode, AudioNode,
                                   mThreshold, mKnee, mRatio, mAttack, mRelease)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DynamicsCompressorNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(DynamicsCompressorNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(DynamicsCompressorNode, AudioNode)

class DynamicsCompressorNodeEngine final : public AudioNodeEngine {
 public:
  explicit DynamicsCompressorNodeEngine(AudioNode* aNode,
                                        AudioDestinationNode* aDestination)
      : AudioNodeEngine(aNode),
        mDestination(aDestination->Track())
        // Keep the default value in sync with the default value in
        // DynamicsCompressorNode::DynamicsCompressorNode.
        ,
        mThreshold(-24.f),
        mKnee(30.f),
        mRatio(12.f),
        mAttack(0.003f),
        mRelease(0.25f),
        mCompressor(new DynamicsCompressor(mDestination->mSampleRate, 2)) {}

  enum Parameters { THRESHOLD, KNEE, RATIO, ATTACK, RELEASE };
  void RecvTimelineEvent(uint32_t aIndex, AudioTimelineEvent& aEvent) override {
    MOZ_ASSERT(mDestination);

    WebAudioUtils::ConvertAudioTimelineEventToTicks(aEvent, mDestination);

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

  void ProcessBlock(AudioNodeTrack* aTrack, GraphTime aFrom,
                    const AudioBlock& aInput, AudioBlock* aOutput,
                    bool* aFinished) override {
    if (aInput.IsNull()) {
      // Just output silence
      *aOutput = aInput;
      return;
    }

    const uint32_t channelCount = aInput.ChannelCount();
    if (mCompressor->numberOfChannels() != channelCount) {
      // Create a new compressor object with a new channel count
      mCompressor = MakeUnique<WebCore::DynamicsCompressor>(
          aTrack->mSampleRate, aInput.ChannelCount());
    }

    TrackTime pos = mDestination->GraphTimeToTrackTime(aFrom);
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

    SendReductionParamToMainThread(
        aTrack,
        mCompressor->parameterValue(DynamicsCompressor::ParamReduction));
  }

  size_t SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const override {
    // Not owned:
    // - mDestination (probably)
    // - Don't count the AudioParamTimelines, their inner refs are owned by the
    // AudioNode.
    size_t amount = AudioNodeEngine::SizeOfExcludingThis(aMallocSizeOf);
    amount += mCompressor->sizeOfIncludingThis(aMallocSizeOf);
    return amount;
  }

  size_t SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const override {
    return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
  }

 private:
  void SendReductionParamToMainThread(AudioNodeTrack* aTrack,
                                      float aReduction) {
    MOZ_ASSERT(!NS_IsMainThread());

    class Command final : public Runnable {
     public:
      Command(AudioNodeTrack* aTrack, float aReduction)
          : mozilla::Runnable("Command"),
            mTrack(aTrack),
            mReduction(aReduction) {}

      NS_IMETHOD Run() override {
        RefPtr<DynamicsCompressorNode> node =
            static_cast<DynamicsCompressorNode*>(
                mTrack->Engine()->NodeMainThread());
        if (node) {
          node->SetReduction(mReduction);
        }
        return NS_OK;
      }

     private:
      RefPtr<AudioNodeTrack> mTrack;
      float mReduction;
    };

    mAbstractMainThread->Dispatch(do_AddRef(new Command(aTrack, aReduction)));
  }

 private:
  RefPtr<AudioNodeTrack> mDestination;
  AudioParamTimeline mThreshold;
  AudioParamTimeline mKnee;
  AudioParamTimeline mRatio;
  AudioParamTimeline mAttack;
  AudioParamTimeline mRelease;
  UniquePtr<DynamicsCompressor> mCompressor;
};

DynamicsCompressorNode::DynamicsCompressorNode(AudioContext* aContext)
    : AudioNode(aContext, 2, ChannelCountMode::Clamped_max,
                ChannelInterpretation::Speakers),
      mReduction(0) {
  mThreshold = CreateAudioParam(DynamicsCompressorNodeEngine::THRESHOLD,
                                u"threshold"_ns, -24.f, -100.f, 0.f);
  mKnee = CreateAudioParam(DynamicsCompressorNodeEngine::KNEE, u"knee"_ns, 30.f,
                           0.f, 40.f);
  mRatio = CreateAudioParam(DynamicsCompressorNodeEngine::RATIO, u"ratio"_ns,
                            12.f, 1.f, 20.f);
  mAttack = CreateAudioParam(DynamicsCompressorNodeEngine::ATTACK, u"attack"_ns,
                             0.003f, 0.f, 1.f);
  mRelease = CreateAudioParam(DynamicsCompressorNodeEngine::RELEASE,
                              u"release"_ns, 0.25f, 0.f, 1.f);
  DynamicsCompressorNodeEngine* engine =
      new DynamicsCompressorNodeEngine(this, aContext->Destination());
  mTrack = AudioNodeTrack::Create(
      aContext, engine, AudioNodeTrack::NO_TRACK_FLAGS, aContext->Graph());
}

/* static */
already_AddRefed<DynamicsCompressorNode> DynamicsCompressorNode::Create(
    AudioContext& aAudioContext, const DynamicsCompressorOptions& aOptions,
    ErrorResult& aRv) {
  RefPtr<DynamicsCompressorNode> audioNode =
      new DynamicsCompressorNode(&aAudioContext);

  audioNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  audioNode->Attack()->SetInitialValue(aOptions.mAttack);
  audioNode->Knee()->SetInitialValue(aOptions.mKnee);
  audioNode->Ratio()->SetInitialValue(aOptions.mRatio);
  audioNode->GetRelease()->SetInitialValue(aOptions.mRelease);
  audioNode->Threshold()->SetInitialValue(aOptions.mThreshold);

  return audioNode.forget();
}

size_t DynamicsCompressorNode::SizeOfExcludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  amount += mThreshold->SizeOfIncludingThis(aMallocSizeOf);
  amount += mKnee->SizeOfIncludingThis(aMallocSizeOf);
  amount += mRatio->SizeOfIncludingThis(aMallocSizeOf);
  amount += mAttack->SizeOfIncludingThis(aMallocSizeOf);
  amount += mRelease->SizeOfIncludingThis(aMallocSizeOf);
  return amount;
}

size_t DynamicsCompressorNode::SizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

JSObject* DynamicsCompressorNode::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return DynamicsCompressorNode_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
