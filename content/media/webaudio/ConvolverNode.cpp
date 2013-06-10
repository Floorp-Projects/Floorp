/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConvolverNode.h"
#include "mozilla/dom/ConvolverNodeBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

#include <cmath>
#include "nsMathUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED_1(ConvolverNode, AudioNode, mBuffer)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ConvolverNode)
NS_INTERFACE_MAP_END_INHERITING(AudioNode)

NS_IMPL_ADDREF_INHERITED(ConvolverNode, AudioNode)
NS_IMPL_RELEASE_INHERITED(ConvolverNode, AudioNode)

class ConvolverNodeEngine : public AudioNodeEngine
{
public:
  ConvolverNodeEngine(AudioNode* aNode, bool aNormalize)
    : AudioNodeEngine(aNode)
    , mNormalize(aNormalize)
  {
  }

  enum Parameters {
    NORMALIZE
  };
  virtual void SetInt32Parameter(uint32_t aIndex, int32_t aParam) MOZ_OVERRIDE
  {
    switch (aIndex) {
    case NORMALIZE:
      mNormalize = !!aParam;
      break;
    default:
      NS_ERROR("Bad ConvolverNodeEngine Int32Parameter");
    }
  }
  virtual void SetBuffer(already_AddRefed<ThreadSharedFloatArrayBufferList> aBuffer)
  {
    mBuffer = aBuffer;
  }

  virtual void ProduceAudioBlock(AudioNodeStream* aStream,
                                 const AudioChunk& aInput,
                                 AudioChunk* aOutput,
                                 bool* aFinished)
  {
    *aOutput = aInput;
  }

private:
  nsRefPtr<ThreadSharedFloatArrayBufferList> mBuffer;
  bool mNormalize;
};

ConvolverNode::ConvolverNode(AudioContext* aContext)
  : AudioNode(aContext,
              2,
              ChannelCountMode::Clamped_max,
              ChannelInterpretation::Speakers)
  , mNormalize(true)
{
  ConvolverNodeEngine* engine = new ConvolverNodeEngine(this, mNormalize);
  mStream = aContext->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);
}

JSObject*
ConvolverNode::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return ConvolverNodeBinding::Wrap(aCx, aScope, this);
}

void
ConvolverNode::SetBuffer(JSContext* aCx, AudioBuffer* aBuffer, ErrorResult& aRv)
{
  switch (aBuffer->NumberOfChannels()) {
  case 1:
  case 2:
  case 4:
    // Supported number of channels
    break;
  default:
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return;
  }

  mBuffer = aBuffer;

  // Send the buffer to the stream
  AudioNodeStream* ns = static_cast<AudioNodeStream*>(mStream.get());
  MOZ_ASSERT(ns, "Why don't we have a stream here?");
  if (mBuffer) {
    nsRefPtr<ThreadSharedFloatArrayBufferList> data =
      mBuffer->GetThreadSharedChannelsForRate(aCx);
    ns->SetBuffer(data.forget());
  } else {
    ns->SetBuffer(nullptr);
  }
}

void
ConvolverNode::SetNormalize(bool aNormalize)
{
  mNormalize = aNormalize;
  SendInt32ParameterToStream(ConvolverNodeEngine::NORMALIZE, aNormalize);
}

}
}

