/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/AnalyserNode.h"
#include "mozilla/dom/AnalyserNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED0(AnalyserNode, AudioNode)

class AnalyserNodeEngine : public AudioNodeEngine
{
public:
  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    *aOutput = aInput;
  }
};

AnalyserNode::AnalyserNode(AudioContext* aContext)
  : AudioNode(aContext)
  , mFFTSize(2048)
  , mMinDecibels(-100.)
  , mMaxDecibels(-30.)
  , mSmoothingTimeConstant(.8)
{
  mStream = aContext->Graph()->CreateAudioNodeStream(new AnalyserNodeEngine(),
                                                     MediaStreamGraph::INTERNAL_STREAM);
}

AnalyserNode::~AnalyserNode()
{
  DestroyMediaStream();
}

JSObject*
AnalyserNode::WrapObject(JSContext* aCx, JSObject* aScope)
{
  return AnalyserNodeBinding::Wrap(aCx, aScope, this);
}

void
AnalyserNode::SetFftSize(uint32_t aValue, ErrorResult& aRv)
{
  // Disallow values that are either less than 2 or not a power of 2
  if (aValue < 2 ||
      (aValue & (aValue - 1)) != 0) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mFFTSize = aValue;
}

void
AnalyserNode::SetMinDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue >= mMaxDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMinDecibels = aValue;
}

void
AnalyserNode::SetMaxDecibels(double aValue, ErrorResult& aRv)
{
  if (aValue <= mMinDecibels) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mMaxDecibels = aValue;
}

void
AnalyserNode::SetSmoothingTimeConstant(double aValue, ErrorResult& aRv)
{
  if (aValue < 0 || aValue > 1) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }
  mSmoothingTimeConstant = aValue;
}

void
AnalyserNode::GetFloatFrequencyData(Float32Array& aArray)
{
}

void
AnalyserNode::GetByteFrequencyData(Uint8Array& aArray)
{
}

void
AnalyserNode::GetByteTimeDomainData(Uint8Array& aArray)
{
}

}
}

