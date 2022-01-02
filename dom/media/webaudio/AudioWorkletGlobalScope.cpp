/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AudioWorkletGlobalScope.h"

#include "AudioNodeEngine.h"
#include "AudioNodeTrack.h"
#include "AudioWorkletImpl.h"
#include "jsapi.h"
#include "js/ForOfIterator.h"
#include "js/PropertyAndElement.h"  // JS_GetProperty
#include "mozilla/dom/AudioWorkletGlobalScopeBinding.h"
#include "mozilla/dom/AudioWorkletProcessor.h"
#include "mozilla/dom/BindingCallContext.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkletPrincipals.h"
#include "mozilla/dom/AudioParamDescriptorBinding.h"
#include "nsPrintfCString.h"
#include "nsTHashSet.h"
#include "Tracing.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope,
                                   mNameToProcessorMap);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioWorkletGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkletGlobalScope)

NS_IMPL_ADDREF_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope)
NS_IMPL_RELEASE_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope)

AudioWorkletGlobalScope::AudioWorkletGlobalScope(AudioWorkletImpl* aImpl)
    : WorkletGlobalScope(aImpl->GetAgentClusterId(),
                         aImpl->IsSharedMemoryAllowed()),
      mImpl(aImpl) {}

bool AudioWorkletGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  // |this| is being exposed to JS and content script will soon be running.
  // The graph needs a handle on the JSContext so it can interrupt JS.
  mImpl->DestinationTrack()->Graph()->NotifyJSContext(aCx);

  JS::RealmOptions options;

  // The SharedArrayBuffer global constructor property should not be present in
  // a fresh global object when shared memory objects aren't allowed (because
  // COOP/COEP support isn't enabled, or because COOP/COEP don't act to isolate
  // this worklet to a separate process).
  options.creationOptions().setDefineSharedArrayBufferConstructor(
      IsSharedMemoryAllowed());

  JS::AutoHoldPrincipals principals(aCx, new WorkletPrincipals(mImpl));
  return AudioWorkletGlobalScope_Binding::Wrap(
      aCx, this, this, options, principals.get(), true, aReflector);
}

void AudioWorkletGlobalScope::RegisterProcessor(
    JSContext* aCx, const nsAString& aName,
    AudioWorkletProcessorConstructor& aProcessorCtor, ErrorResult& aRv) {
  TRACE_COMMENT("AudioWorkletGlobalScope::RegisterProcessor", "%s",
                NS_ConvertUTF16toUTF8(aName).get());

  JS::Rooted<JSObject*> processorConstructor(aCx,
                                             aProcessorCtor.CallableOrNull());

  /**
   * 1. If the name is the empty string, throw a NotSupportedError
   *    exception and abort these steps because the empty string is not
   *    a valid key.
   */
  if (aName.IsEmpty()) {
    aRv.ThrowNotSupportedError("Argument 1 should not be an empty string.");
    return;
  }

  /**
   * 2. If the name exists as a key in the node name to processor
   *    definition map, throw a NotSupportedError exception and abort
   *    these steps because registering a definition with a duplicated
   *    key is not allowed.
   */
  if (mNameToProcessorMap.GetWeak(aName)) {
    // Duplicate names are not allowed
    aRv.ThrowNotSupportedError(
        "Argument 1 is invalid: a class with the same name is already "
        "registered.");
    return;
  }

  // We know processorConstructor is callable, so not a WindowProxy or Location.
  JS::Rooted<JSObject*> constructorUnwrapped(
      aCx, js::CheckedUnwrapStatic(processorConstructor));
  if (!constructorUnwrapped) {
    // If the caller's compartment does not have permission to access the
    // unwrapped constructor then throw.
    aRv.ThrowSecurityError("Constructor cannot be called");
    return;
  }

  /**
   * 3. If the result of IsConstructor(argument=processorCtor) is false,
   *    throw a TypeError and abort these steps.
   */
  if (!JS::IsConstructor(constructorUnwrapped)) {
    aRv.ThrowTypeError<MSG_NOT_CONSTRUCTOR>("Argument 2");
    return;
  }

  /**
   * 4. Let prototype be the result of Get(O=processorCtor, P="prototype").
   */
  // The .prototype on the constructor passed could be an "expando" of a
  // wrapper. So we should get it from wrapper instead of the underlying
  // object.
  JS::Rooted<JS::Value> prototype(aCx);
  if (!JS_GetProperty(aCx, processorConstructor, "prototype", &prototype)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  /**
   * 5. If the result of Type(argument=prototype) is not Object, throw a
   *    TypeError and abort all these steps.
   */
  if (!prototype.isObject()) {
    aRv.ThrowTypeError<MSG_NOT_OBJECT>("processorCtor.prototype");
    return;
  }
  /**
   * 6. Let parameterDescriptorsValue be the result of Get(O=processorCtor,
   *    P="parameterDescriptors").
   */
  JS::Rooted<JS::Value> descriptors(aCx);
  if (!JS_GetProperty(aCx, processorConstructor, "parameterDescriptors",
                      &descriptors)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  AudioParamDescriptorMap map;
  /*
   * 7. If parameterDescriptorsValue is not undefined
   */
  if (!descriptors.isUndefined()) {
    /*
     * 7.1. Let parameterDescriptorSequence be the result of the conversion
     *    from parameterDescriptorsValue to an IDL value of type
     *    sequence<AudioParamDescriptor>.
     */
    JS::Rooted<JS::Value> objectValue(aCx, descriptors);
    JS::ForOfIterator iter(aCx);
    if (!iter.init(objectValue, JS::ForOfIterator::AllowNonIterable)) {
      aRv.NoteJSContextException(aCx);
      return;
    }
    if (!iter.valueIsIterable()) {
      aRv.ThrowTypeError<MSG_CONVERSION_ERROR>(
          "AudioWorkletProcessor.parameterDescriptors", "sequence");
      return;
    }
    /*
     * 7.2 and 7.3 (and substeps)
     */
    map = DescriptorsFromJS(aCx, &iter, aRv);
    if (aRv.Failed()) {
      return;
    }
  }

  /**
   * 8. Append the key-value pair name → processorCtor to node name to processor
   * constructor map of the associated AudioWorkletGlobalScope.
   */
  if (!mNameToProcessorMap.InsertOrUpdate(aName, RefPtr{&aProcessorCtor},
                                          fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  /**
   * 9. Queue a task to the control thread to add the key-value pair
   *     (name - descriptors) to the node name to parameter descriptor
   *     map of the associated BaseAudioContext.
   */
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "AudioWorkletGlobalScope: parameter descriptors",
      [impl = mImpl, name = nsString(aName), map = std::move(map)]() mutable {
        AudioNode* destinationNode =
            impl->DestinationTrack()->Engine()->NodeMainThread();
        if (!destinationNode) {
          return;
        }
        destinationNode->Context()->SetParamMapForWorkletName(name, &map);
      }));
}

WorkletImpl* AudioWorkletGlobalScope::Impl() const { return mImpl; }

uint64_t AudioWorkletGlobalScope::CurrentFrame() const {
  AudioNodeTrack* destinationTrack = mImpl->DestinationTrack();
  GraphTime processedTime = destinationTrack->Graph()->ProcessedTime();
  return destinationTrack->GraphTimeToTrackTime(processedTime);
}

double AudioWorkletGlobalScope::CurrentTime() const {
  return static_cast<double>(CurrentFrame()) / SampleRate();
}

float AudioWorkletGlobalScope::SampleRate() const {
  return static_cast<float>(mImpl->DestinationTrack()->mSampleRate);
}

AudioParamDescriptorMap AudioWorkletGlobalScope::DescriptorsFromJS(
    JSContext* aCx, JS::ForOfIterator* aIter, ErrorResult& aRv) {
  AudioParamDescriptorMap res;
  // To check for duplicates
  nsTHashSet<nsString> namesSet;

  JS::Rooted<JS::Value> nextValue(aCx);
  bool done = false;
  size_t i = 0;
  while (true) {
    if (!aIter->next(&nextValue, &done)) {
      aRv.NoteJSContextException(aCx);
      return AudioParamDescriptorMap();
    }
    if (done) {
      break;
    }

    BindingCallContext callCx(aCx, "AudioWorkletGlobalScope.registerProcessor");
    nsPrintfCString sourceDescription("Element %zu in parameterDescriptors", i);
    i++;
    AudioParamDescriptor* descriptor = res.AppendElement(fallible);
    if (!descriptor) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return AudioParamDescriptorMap();
    }
    if (!descriptor->Init(callCx, nextValue, sourceDescription.get())) {
      aRv.NoteJSContextException(aCx);
      return AudioParamDescriptorMap();
    }
  }

  for (const auto& descriptor : res) {
    if (namesSet.Contains(descriptor.mName)) {
      aRv.ThrowNotSupportedError("Duplicated name \""_ns +
                                 NS_ConvertUTF16toUTF8(descriptor.mName) +
                                 "\" in parameterDescriptors."_ns);
      return AudioParamDescriptorMap();
    }

    if (!namesSet.Insert(descriptor.mName, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return AudioParamDescriptorMap();
    }

    if (descriptor.mMinValue > descriptor.mMaxValue) {
      aRv.ThrowInvalidStateError(
          "In parameterDescriptors, "_ns +
          NS_ConvertUTF16toUTF8(descriptor.mName) +
          " minValue should be smaller than maxValue."_ns);
      return AudioParamDescriptorMap();
    }

    if (descriptor.mDefaultValue < descriptor.mMinValue ||
        descriptor.mDefaultValue > descriptor.mMaxValue) {
      aRv.ThrowInvalidStateError(
          "In parameterDescriptors, "_ns +
          NS_ConvertUTF16toUTF8(descriptor.mName) +
          nsLiteralCString(" defaultValue is out of the range defined by "
                           "minValue and maxValue."));
      return AudioParamDescriptorMap();
    }
  }

  return res;
}

bool AudioWorkletGlobalScope::ConstructProcessor(
    JSContext* aCx, const nsAString& aName,
    NotNull<StructuredCloneHolder*> aSerializedOptions,
    UniqueMessagePortId& aPortIdentifier,
    JS::MutableHandle<JSObject*> aRetProcessor) {
  TRACE_COMMENT("AudioWorkletProcessor::ConstructProcessor", "%s",
                NS_ConvertUTF16toUTF8(aName).get());
  /**
   * See
   * https://webaudio.github.io/web-audio-api/#AudioWorkletProcessor-instantiation
   */
  ErrorResult rv;
  /**
   * 4. Let deserializedPort be the result of
   *    StructuredDeserialize(serializedPort, the current Realm).
   */
  RefPtr<MessagePort> deserializedPort =
      MessagePort::Create(this, aPortIdentifier, rv);
  if (NS_WARN_IF(rv.MaybeSetPendingException(aCx))) {
    return false;
  }
  /**
   * 5. Let deserializedOptions be the result of
   *    StructuredDeserialize(serializedOptions, the current Realm).
   */
  JS::CloneDataPolicy cloneDataPolicy;
  cloneDataPolicy.allowIntraClusterClonableSharedObjects();
  cloneDataPolicy.allowSharedMemoryObjects();

  JS::Rooted<JS::Value> deserializedOptions(aCx);
  aSerializedOptions->Read(this, aCx, &deserializedOptions, cloneDataPolicy,
                           rv);
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }
  /**
   * 6. Let processorCtor be the result of looking up processorName on the
   *    AudioWorkletGlobalScope's node name to processor definition map.
   */
  RefPtr<AudioWorkletProcessorConstructor> processorCtor =
      mNameToProcessorMap.Get(aName);
  // AudioWorkletNode has already checked the definition exists.
  // See also https://github.com/WebAudio/web-audio-api/issues/1854
  MOZ_ASSERT(processorCtor);
  /**
   * 7. Store nodeReference and deserializedPort to node reference and
   *    transferred port of this AudioWorkletGlobalScope's pending processor
   *    construction data respectively.
   */
  // |nodeReference| is not required here because the "processorerror" event
  // is thrown by WorkletNodeEngine::ConstructProcessor().
  mPortForProcessor = std::move(deserializedPort);
  /**
   * 8. Construct a callback function from processorCtor with the argument
   *    of deserializedOptions.
   */
  // The options were an object before serialization and so will be an object
  // if deserialization succeeded above.  toObject() asserts.
  JS::Rooted<JSObject*> options(aCx, &deserializedOptions.toObject());
  RefPtr<AudioWorkletProcessor> processor = processorCtor->Construct(
      options, rv, "AudioWorkletProcessor construction",
      CallbackFunction::eRethrowExceptions);
  // https://github.com/WebAudio/web-audio-api/issues/2096
  mPortForProcessor = nullptr;
  if (rv.MaybeSetPendingException(aCx)) {
    return false;
  }
  JS::Rooted<JS::Value> processorVal(aCx);
  if (NS_WARN_IF(!ToJSValue(aCx, processor, &processorVal))) {
    return false;
  }
  MOZ_ASSERT(processorVal.isObject());
  aRetProcessor.set(&processorVal.toObject());
  return true;
}

RefPtr<MessagePort> AudioWorkletGlobalScope::TakePortForProcessorCtor() {
  return std::move(mPortForProcessor);
}

}  // namespace mozilla::dom
