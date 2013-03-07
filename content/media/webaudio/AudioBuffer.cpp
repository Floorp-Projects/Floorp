/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioBuffer.h"
#include <speex/speex_resampler.h>
#include "mozilla/dom/AudioBufferBinding.h"
#include "nsContentUtils.h"
#include "AudioContext.h"
#include "jsfriendapi.h"
#include "mozilla/ErrorResult.h"
#include "AudioSegment.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mContext)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mJSChannels)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->ClearJSChannels();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mContext)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(AudioBuffer)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  for (uint32_t i = 0; i < tmp->mJSChannels.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mJSChannels[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AudioBuffer)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AudioBuffer)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioBuffer)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

AudioBuffer::AudioBuffer(AudioContext* aContext, uint32_t aLength,
                         float aSampleRate)
  : mContext(aContext),
    mLength(aLength),
    mSampleRate(aSampleRate)
{
  SetIsDOMBinding();

  NS_HOLD_JS_OBJECTS(this, AudioBuffer);
}

AudioBuffer::~AudioBuffer()
{
  ClearJSChannels();
}

void
AudioBuffer::ClearJSChannels()
{
  mJSChannels.Clear();
  NS_DROP_JS_OBJECTS(this, AudioBuffer);
}

bool
AudioBuffer::InitializeBuffers(uint32_t aNumberOfChannels, JSContext* aJSContext)
{
  if (!mJSChannels.SetCapacity(aNumberOfChannels)) {
    return false;
  }
  for (uint32_t i = 0; i < aNumberOfChannels; ++i) {
    JSObject* array = JS_NewFloat32Array(aJSContext, mLength);
    if (!array) {
      return false;
    }
    mJSChannels.AppendElement(array);
  }

  return true;
}

JSObject*
AudioBuffer::WrapObject(JSContext* aCx, JSObject* aScope,
                        bool* aTriedToWrap)
{
  return AudioBufferBinding::Wrap(aCx, aScope, this, aTriedToWrap);
}

void
AudioBuffer::RestoreJSChannelData(JSContext* aJSContext)
{
  if (mSharedChannels) {
    for (uint32_t i = 0; i < mJSChannels.Length(); ++i) {
      const float* data = mSharedChannels->GetData(i);
      // The following code first zeroes the array and then copies our data
      // into it. We could avoid this with additional JS APIs to construct
      // an array (or ArrayBuffer) containing initial data.
      JSObject* array = JS_NewFloat32Array(aJSContext, mLength);
      memcpy(JS_GetFloat32ArrayData(array), data, sizeof(float)*mLength);
      mJSChannels[i] = array;
    }

    mSharedChannels = nullptr;
    mResampledChannels = nullptr;
  }
}

JSObject*
AudioBuffer::GetChannelData(JSContext* aJSContext, uint32_t aChannel,
                            ErrorResult& aRv)
{
  if (aChannel >= NumberOfChannels()) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  RestoreJSChannelData(aJSContext);

  return mJSChannels[aChannel];
}

void
AudioBuffer::SetChannelDataFromArrayBufferContents(JSContext* aJSContext,
                                                   uint32_t aChannel,
                                                   void* aContents)
{
  RestoreJSChannelData(aJSContext);

  MOZ_ASSERT(aChannel < NumberOfChannels());
  JSObject* arrayBuffer = JS_NewArrayBufferWithContents(aJSContext, aContents);
  mJSChannels[aChannel] = JS_NewFloat32ArrayWithBuffer(aJSContext, arrayBuffer,
                                                       0, -1);
  MOZ_ASSERT(mLength == JS_GetTypedArrayLength(mJSChannels[aChannel]));
}

static already_AddRefed<ThreadSharedFloatArrayBufferList>
StealJSArrayDataIntoThreadSharedFloatArrayBufferList(JSContext* aJSContext,
                                                     const nsTArray<JSObject*>& aJSArrays)
{
  nsRefPtr<ThreadSharedFloatArrayBufferList> result =
    new ThreadSharedFloatArrayBufferList(aJSArrays.Length());
  for (uint32_t i = 0; i < aJSArrays.Length(); ++i) {
    JSObject* arrayBuffer = JS_GetArrayBufferViewBuffer(aJSArrays[i]);
    void* dataToFree = nullptr;
    uint8_t* stolenData = nullptr;
    if (arrayBuffer &&
        JS_StealArrayBufferContents(aJSContext, arrayBuffer, &dataToFree,
                                    &stolenData)) {
      result->SetData(i, dataToFree, reinterpret_cast<float*>(stolenData));
    } else {
      result->Clear();
      return result.forget();
    }
  }
  return result.forget();
}

ThreadSharedFloatArrayBufferList*
AudioBuffer::GetThreadSharedChannelsForRate(JSContext* aJSContext, uint32_t aRate,
                                            uint32_t* aLength)
{
  if (mResampledChannels && mResampledChannelsRate == aRate) {
    // return cached data
    *aLength = mResampledChannelsLength;
    return mResampledChannels;
  }

  if (!mSharedChannels) {
    // Steal JS data
    mSharedChannels =
      StealJSArrayDataIntoThreadSharedFloatArrayBufferList(aJSContext, mJSChannels);
  }

  if (mSampleRate == aRate) {
    *aLength = mLength;
    return mSharedChannels;
  }

  mResampledChannels = new ThreadSharedFloatArrayBufferList(NumberOfChannels());

  double newLengthD = ceil(Duration()*aRate);
  uint32_t newLength = uint32_t(newLengthD);
  *aLength = newLength;
  double size = sizeof(float)*NumberOfChannels()*newLengthD;
  if (size != uint32_t(size)) {
    return mResampledChannels;
  }
  float* outputData = static_cast<float*>(malloc(uint32_t(size)));
  if (!outputData) {
    return mResampledChannels;
  }

  SpeexResamplerState* resampler = speex_resampler_init(NumberOfChannels(),
                                                        mSampleRate,
                                                        aRate,
                                                        SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                                        nullptr);

  for (uint32_t i = 0; i < NumberOfChannels(); ++i) {
    const float* inputData = mSharedChannels->GetData(i);
    uint32_t inSamples = mLength;
    uint32_t outSamples = newLength;

    speex_resampler_process_float(resampler, i,
                                  inputData, &inSamples,
                                  outputData, &outSamples);

    mResampledChannels->SetData(i, i == 0 ? outputData : nullptr, outputData);
    outputData += newLength;
  }

  speex_resampler_destroy(resampler);

  mResampledChannelsRate = aRate;
  mResampledChannelsLength = newLength;
  return mResampledChannels;
}

}
}
