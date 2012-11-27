/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioContext.h"
#include "nsContentUtils.h"
#include "nsIDOMWindow.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AudioContextBinding.h"
#include "AudioDestinationNode.h"
#include "AudioBufferSourceNode.h"
#include "AudioBuffer.h"
#include "GainNode.h"
#include "DelayNode.h"
#include "PannerNode.h"
#include "AudioListener.h"
#include "DynamicsCompressorNode.h"
#include "BiquadFilterNode.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(AudioContext)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDestination)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER_NATIVE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDestination)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(AudioContext)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(AudioContext, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(AudioContext, Release)

AudioContext::AudioContext(nsIDOMWindow* aWindow)
  : mWindow(aWindow)
  , mDestination(new AudioDestinationNode(this))
{
  SetIsDOMBinding();
}

AudioContext::~AudioContext()
{
}

JSObject*
AudioContext::WrapObject(JSContext* aCx, JSObject* aScope,
                         bool* aTriedToWrap)
{
  return mozAudioContextBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

/* static */ already_AddRefed<AudioContext>
AudioContext::Constructor(nsISupports* aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIDOMWindow> window = do_QueryInterface(aGlobal);
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  AudioContext* object = new AudioContext(window);
  NS_ADDREF(object);
  return object;
}

already_AddRefed<AudioBufferSourceNode>
AudioContext::CreateBufferSource()
{
  nsRefPtr<AudioBufferSourceNode> bufferNode =
    new AudioBufferSourceNode(this);
  return bufferNode.forget();
}

already_AddRefed<AudioBuffer>
AudioContext::CreateBuffer(JSContext* aJSContext, uint32_t aNumberOfChannels,
                           uint32_t aLength, float aSampleRate,
                           ErrorResult& aRv)
{
  nsRefPtr<AudioBuffer> buffer = new AudioBuffer(this, aLength, aSampleRate);
  if (!buffer->InitializeBuffers(aNumberOfChannels, aJSContext)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }
  return buffer.forget();
}

already_AddRefed<GainNode>
AudioContext::CreateGain()
{
  nsRefPtr<GainNode> gainNode = new GainNode(this);
  return gainNode.forget();
}

already_AddRefed<DelayNode>
AudioContext::CreateDelay(double aMaxDelayTime, ErrorResult& aRv)
{
  if (aMaxDelayTime > 0. && aMaxDelayTime < 3.) {
    nsRefPtr<DelayNode> delayNode = new DelayNode(this, aMaxDelayTime);
    return delayNode.forget();
  }
  aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
  return nullptr;
}

already_AddRefed<PannerNode>
AudioContext::CreatePanner()
{
  nsRefPtr<PannerNode> pannerNode = new PannerNode(this);
  return pannerNode.forget();
}

already_AddRefed<DynamicsCompressorNode>
AudioContext::CreateDynamicsCompressor()
{
  nsRefPtr<DynamicsCompressorNode> compressorNode =
    new DynamicsCompressorNode(this);
  return compressorNode.forget();
}

already_AddRefed<BiquadFilterNode>
AudioContext::CreateBiquadFilter()
{
  nsRefPtr<BiquadFilterNode> filterNode =
    new BiquadFilterNode(this);
  return filterNode.forget();
}

AudioListener*
AudioContext::Listener()
{
  if (!mListener) {
    mListener = new AudioListener(this);
  }
  return mListener;
}

}
}

