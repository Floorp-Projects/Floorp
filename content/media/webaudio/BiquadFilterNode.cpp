/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BiquadFilterNode.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"
#include "AudioDestinationNode.h"
#include "WebAudioUtils.h"
#include "blink/Biquad.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_4(BiquadFilterNode, AudioNode,
                                     mFrequency, mDetune, mQ, mGain)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(BiquadFilterNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(BiquadFilterNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(BiquadFilterNode, AudioNode)

class BiquadFilterNodeEngine : public AudioNodeEngine
{
public:
  BiquadFilterNodeEngine(AudioNode* aNode, AudioDestinationNode* aDestination)
    : AudioNodeEngine(aNode)
    , mSource(nullptr)
    , mDestination(static_cast<AudioNodeStream*> (aDestination->Stream()))
    // Keep the default values in sync with the default values in
    // BiquadFilterNode::BiquadFilterNode
    , mType(BiquadFilterType::Lowpass)
    , mFrequency(350.f)
    , mDetune(0.f)
    , mQ(1.f)
    , mGain(0.f)
  {
  }

  void SetSourceStream(AudioNodeStream* aSource)
  {
    mSource = aSource;
  }

  enum Parameteres {
    TYPE,
    FREQUENCY,
    DETUNE,
    Q,
    GAIN
  };
  void SetInt32Parameter(uint32_t aIndex, int32_t aValue) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case TYPE: mType = static_cast<BiquadFilterType>(aValue); break;
    default:
      NS_ERROR("Bad BiquadFilterNode Int32Parameter");
    }
  }
  void SetTimelineParameter(uint32_t aIndex, const AudioParamTimeline& aValue) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mSource && mDestination);
    switch (aIndex) {
    case FREQUENCY:
      mFrequency = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mFrequency, mSource, mDestination);
      break;
    case DETUNE:
      mDetune = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mDetune, mSource, mDestination);
      break;
    case Q:
      mQ = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mQ, mSource, mDestination);
      break;
    case GAIN:
      mGain = aValue;
      WebAudioUtils::ConvertAudioParamToTicks(mGain, mSource, mDestination);
      break;
    default:
      NS_ERROR("Bad BiquadFilterNodeEngine TimelineParameter");
    }
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished) MOZ_OVERRIDE
  {
    if (aInput.IsNull()) {
      aOutput->SetNull(WEBAUDIO_BLOCK_SIZE);
      return;
    }

    // Adjust the number of biquads based on the number of channels
    const uint32_t numberOfChannels = aInput.mChannelData.Length();
    mBiquads.SetLength(numberOfChannels);

    AllocateAudioBlock(numberOfChannels, aOutput);

    TrackTicks pos = aStream->GetCurrentPosition();

    for (uint32_t i = 0; i < numberOfChannels; ++i) {
      double freq = mFrequency.GetValueAtTime(pos);
      double q = mQ.GetValueAtTime(pos);
      double gain = mGain.GetValueAtTime(pos);
      double detune = mDetune.GetValueAtTime(pos);

      double nyquist = IdealAudioRate() * 0.5;
      double normalizedFrequency = freq / nyquist;

      if (detune) {
        normalizedFrequency *= std::pow(2.0, detune / 1200);
      }

      switch (mType) {
      case BiquadFilterType::Lowpass:
        mBiquads[i].setLowpassParams(normalizedFrequency, q);
        break;
      case BiquadFilterType::Highpass:
        mBiquads[i].setHighpassParams(normalizedFrequency, q);
        break;
      case BiquadFilterType::Bandpass:
        mBiquads[i].setBandpassParams(normalizedFrequency, q);
        break;
      case BiquadFilterType::Lowshelf:
        mBiquads[i].setLowShelfParams(normalizedFrequency, gain);
        break;
      case BiquadFilterType::Highshelf:
        mBiquads[i].setHighShelfParams(normalizedFrequency, gain);
        break;
      case BiquadFilterType::Peaking:
        mBiquads[i].setPeakingParams(normalizedFrequency, q, gain);
        break;
      case BiquadFilterType::Notch:
        mBiquads[i].setNotchParams(normalizedFrequency, q);
        break;
      case BiquadFilterType::Allpass:
        mBiquads[i].setAllpassParams(normalizedFrequency, q);
        break;
      }

      mBiquads[i].process(static_cast<const float*>(aInput.mChannelData[i]),
                          static_cast<float*>(const_cast<void*>(aOutput->mChannelData[i])),
                          aInput.GetDuration());
    }
  }

private:
  AudioNodeStream* mSource;
  AudioNodeStream* mDestination;
  BiquadFilterType mType;
  AudioParamTimeline mFrequency;
  AudioParamTimeline mDetune;
  AudioParamTimeline mQ;
  AudioParamTimeline mGain;
  nsTArray<WebCore::Biquad> mBiquads;
};

BiquadFilterNode::BiquadFilterNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mType(BiquadFilterType::Lowpass)
  , mFrequency(new AudioParam(this, SendFrequencyToStream, 350.f))
  , mDetune(new AudioParam(this, SendDetuneToStream, 0.f))
  , mQ(new AudioParam(this, SendQToStream, 1.f))
  , mGain(new AudioParam(this, SendGainToStream, 0.f))
{
  BiquadFilterNodeEngine* engine = new BiquadFilterNodeEngine(this, aContext->Destination());
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
  engine->SetSourceStream(static_cast<AudioNodeStream*> (mStream.get()));
}

JSObject*
BiquadFilterNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return BiquadFilterNodeBinding::Wrap(aCx, aScope, this);
}

void
BiquadFilterNode::SetType(BiquadFilterType aType)
{
  mType = aType;
  SendInt32ParameterToStream(BiquadFilterNodeEngine::TYPE,
                             static_cast<int32_t>(aType));
}

void
BiquadFilterNode::SendFrequencyToStream(AudioNode* aNode)
{
  BiquadFilterNode* This = static_cast<BiquadFilterNode*>(aNode);
  SendTimelineParameterToStream(This, BiquadFilterNodeEngine::FREQUENCY, *This->mFrequency);
}

void
BiquadFilterNode::SendDetuneToStream(AudioNode* aNode)
{
  BiquadFilterNode* This = static_cast<BiquadFilterNode*>(aNode);
  SendTimelineParameterToStream(This, BiquadFilterNodeEngine::DETUNE, *This->mDetune);
}

void
BiquadFilterNode::SendQToStream(AudioNode* aNode)
{
  BiquadFilterNode* This = static_cast<BiquadFilterNode*>(aNode);
  SendTimelineParameterToStream(This, BiquadFilterNodeEngine::Q, *This->mQ);
}

void
BiquadFilterNode::SendGainToStream(AudioNode* aNode)
{
  BiquadFilterNode* This = static_cast<BiquadFilterNode*>(aNode);
  SendTimelineParameterToStream(This, BiquadFilterNodeEngine::GAIN, *This->mGain);
}

}
}

