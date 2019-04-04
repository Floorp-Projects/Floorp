/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "AudioWorkletNode.h"

#include "AudioParamMap.h"
#include "mozilla/dom/AudioWorkletNodeBinding.h"
#include "mozilla/dom/MessagePort.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(AudioWorkletNode, AudioNode)

AudioWorkletNode::AudioWorkletNode(AudioContext* aAudioContext,
                                   const nsAString& aName,
                                   const AudioWorkletNodeOptions& aOptions)
    : AudioNode(aAudioContext, 2, ChannelCountMode::Max,
                ChannelInterpretation::Speakers),
      mNodeName(aName),
      mInputCount(aOptions.mNumberOfInputs),
      mOutputCount(aOptions.mNumberOfOutputs) {}

/* static */
already_AddRefed<AudioWorkletNode> AudioWorkletNode::Constructor(
    const GlobalObject& aGlobal, AudioContext& aAudioContext,
    const nsAString& aName, const AudioWorkletNodeOptions& aOptions,
    ErrorResult& aRv) {
  if (aOptions.mNumberOfInputs == 0 && aOptions.mNumberOfOutputs == 0) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  if (aOptions.mOutputChannelCount.WasPassed()) {
    if (aOptions.mOutputChannelCount.Value().Length() !=
        aOptions.mNumberOfOutputs) {
      aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
      return nullptr;
    }

    for (uint32_t channelCount : aOptions.mOutputChannelCount.Value()) {
      if (channelCount == 0 || channelCount > WebAudioUtils::MaxChannelCount) {
        aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
        return nullptr;
      }
    }
  }
  /**
   * 2. If nodeName does not exists as a key in the BaseAudioContext’s node
   *    name to parameter descriptor map, throw a NotSupportedError exception
   *    and abort these steps.
   */
  const AudioParamDescriptorMap* parameterDescriptors =
      aAudioContext.GetParamMapForWorkletName(aName);
  if (!parameterDescriptors) {
    aRv.Throw(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return nullptr;
  }

  // MSG does not support more than UINT16_MAX inputs or outputs.
  if (aOptions.mNumberOfInputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("numberOfInputs"));
    return nullptr;
  }
  if (aOptions.mNumberOfOutputs > UINT16_MAX) {
    aRv.ThrowRangeError<MSG_VALUE_OUT_OF_RANGE>(
        NS_LITERAL_STRING("numberOfOutputs"));
    return nullptr;
  }

  RefPtr<AudioWorkletNode> audioWorkletNode =
      new AudioWorkletNode(&aAudioContext, aName, aOptions);

  audioWorkletNode->Initialize(aOptions, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return audioWorkletNode.forget();
}

AudioParamMap* AudioWorkletNode::GetParameters(ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

MessagePort* AudioWorkletNode::GetPort(ErrorResult& aRv) const {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

JSObject* AudioWorkletNode::WrapObject(JSContext* aCx,
                                       JS::Handle<JSObject*> aGivenProto) {
  return AudioWorkletNode_Binding::Wrap(aCx, this, aGivenProto);
}

size_t AudioWorkletNode::SizeOfExcludingThis(MallocSizeOf aMallocSizeOf) const {
  size_t amount = AudioNode::SizeOfExcludingThis(aMallocSizeOf);
  return amount;
}

size_t AudioWorkletNode::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const {
  return aMallocSizeOf(this) + SizeOfExcludingThis(aMallocSizeOf);
}

}  // namespace dom
}  // namespace mozilla
