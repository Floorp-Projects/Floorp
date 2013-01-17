/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBuffer.h"
#include "mozilla/dom/AudioBufferBinding.h"
#include "nsContentUtils.h"
#include "AudioContext.h"
#include "jsfriendapi.h"
#include "mozilla/ErrorResult.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mChannels)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  for (uint32_t i = 0; i < tmp->mChannels.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mChannels[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioBuffer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioBuffer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioBuffer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AudioBuffer::AudioBuffer(AudioContext* aContext, uint32_t aLength,
                         uint32_t aSampleRate)
  : mContext(aContext),
    mLength(aLength),
    mSampleRate(aSampleRate)
{
  SetIsDOMBinding();

  NS_HOLD_JS_OBJECTS(this, AudioBuffer);
}

AudioBuffer::~AudioBuffer()
{
  mChannels.Clear();
  NS_DROP_JS_OBJECTS(this, AudioBuffer);
}

bool
AudioBuffer::InitializeBuffers(uint32_t aNumberOfChannels, JSContext* aJSContext)
{
  if (!mChannels.SetCapacity(aNumberOfChannels)) {
    return false;
  }
  for (uint32_t i = 0; i < aNumberOfChannels; ++i) {
    JSObject* array = JS_NewFloat32Array(aJSContext, mLength);
    if (!array) {
      return false;
    }
    mChannels.AppendElement(array);
  }

  return true;
}

JSObject*
AudioBuffer::WrapObject(JSContext* aCx, JSObject* aScope,
                        bool* aTriedToWrap)
{
  return AudioBufferBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

JSObject*
AudioBuffer::GetChannelData(JSContext* aJSContext, uint32_t aChannel,
                            ErrorResult& aRv) const
{
  if (aChannel >= mChannels.Length()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }
  return mChannels[aChannel];
}

}
}

