/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioParam.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AudioParamBinding.h"
#include "AudioNodeEngine.h"
#include "AudioNodeStream.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioParam)
  tmp->DisconnectFromGraphAndDestroyStream();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioParam)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AudioParam)

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(AudioParam)

NS_IMETHODIMP_(nsrefcnt)
AudioParam::Release()
{
  if (mRefCnt.get() == 1) {
    // We are about to be deleted, disconnect the object from the graph before
    // the derived type is destroyed.
    DisconnectFromGraphAndDestroyStream();
  }
  NS_IMPL_CC_NATIVE_RELEASE_BODY(AudioParam)
}

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioParam, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioParam, Release)

AudioParam::AudioParam(AudioNode* aNode,
                       AudioParam::CallbackType aCallback,
                       float aDefaultValue)
  : AudioParamTimeline(aDefaultValue)
  , mNode(aNode)
  , mCallback(aCallback)
  , mDefaultValue(aDefaultValue)
{
  SetIsDOMBinding();
}

AudioParam::~AudioParam()
{
  MOZ_ASSERT(mInputNodes.IsEmpty());
}

JSObject*
AudioParam::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return AudioParamBinding::Wrap(aCx, aScope, this);
}

void
AudioParam::DisconnectFromGraphAndDestroyStream()
{
  // Addref this temporarily so the refcount bumping below doesn't destroy us
  // prematurely
  nsRefPtr<AudioParam> kungFuDeathGrip = this;

  while (!mInputNodes.IsEmpty()) {
    uint32_t i = mInputNodes.Length() - 1;
    nsRefPtr<AudioNode> input = mInputNodes[i].mInputNode;
    mInputNodes.RemoveElementAt(i);
    input->RemoveOutputParam(this);
  }

  if (mNodeStreamPort) {
    mNodeStreamPort->Destroy();
    mNodeStreamPort = nullptr;
  }

  if (mStream) {
    mStream->Destroy();
    mStream = nullptr;
  }
}

MediaStream*
AudioParam::Stream()
{
  if (mStream) {
    return mStream;
  }

  AudioNodeEngine* engine = new AudioNodeEngine(nullptr);
  nsRefPtr<AudioNodeStream> stream = mNode->Context()->Graph()->CreateAudioNodeStream(engine, MediaStreamGraph::INTERNAL_STREAM);

  // Force the input to have only one channel, and make it down-mix using
  // the speaker rules if needed.
  stream->SetChannelMixingParametersImpl(1, ChannelCountMode::Explicit, ChannelInterpretation::Speakers);
  // Mark as an AudioParam helper stream
  stream->SetAudioParamHelperStream();

  mStream = stream.forget();

  // Setup the AudioParam's stream as an input to the owner AudioNode's stream
  MediaStream* nodeStream = mNode->Stream();
  MOZ_ASSERT(nodeStream->AsProcessedStream());
  ProcessedMediaStream* ps = static_cast<ProcessedMediaStream*>(nodeStream);
  mNodeStreamPort = ps->AllocateInputPort(mStream, MediaInputPort::FLAG_BLOCK_INPUT);

  // Let the MSG's copy of AudioParamTimeline know about the change in the stream
  mCallback(mNode);

  return mStream;
}

}
}

