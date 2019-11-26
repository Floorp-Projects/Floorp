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
#include "mozilla/dom/AudioWorkletGlobalScopeBinding.h"
#include "mozilla/dom/AudioWorkletProcessor.h"
#include "mozilla/dom/StructuredCloneHolder.h"
#include "mozilla/dom/WorkletPrincipals.h"
#include "mozilla/dom/AudioParamDescriptorBinding.h"
#include "nsPrintfCString.h"
#include "nsTHashtable.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope,
                                   mNameToProcessorMap);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AudioWorkletGlobalScope)
NS_INTERFACE_MAP_END_INHERITING(WorkletGlobalScope)

NS_IMPL_ADDREF_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope)
NS_IMPL_RELEASE_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope)

AudioWorkletGlobalScope::AudioWorkletGlobalScope(AudioWorkletImpl* aImpl)
    : mImpl(aImpl) {}

bool AudioWorkletGlobalScope::WrapGlobalObject(
    JSContext* aCx, JS::MutableHandle<JSObject*> aReflector) {
  JS::RealmOptions options;
  JS::AutoHoldPrincipals principals(aCx, new WorkletPrincipals(mImpl));
  return AudioWorkletGlobalScope_Binding::Wrap(
      aCx, this, this, options, principals.get(), true, aReflector);
}

void AudioWorkletGlobalScope::RegisterProcessor(
    JSContext* aCx, const nsAString& aName,
    AudioWorkletProcessorConstructor& aProcessorCtor, ErrorResult& aRv) {
  JS::Rooted<JSObject*> processorConstructor(aCx,
                                             aProcessorCtor.CallableOrNull());

  /**
   * 1. If the name is the empty string, throw a NotSupportedError
   *    exception and abort these steps because the empty string is not
   *    a valid key.
   */
  if (aName.IsEmpty()) {
    aRv.ThrowDOMException(
        NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        "Argument 1 of AudioWorkletGlobalScope.registerProcessor should not be "
        "an empty string.");
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
    aRv.ThrowDOMException(
        NS_ERROR_DOM_NOT_SUPPORTED_ERR,
        "Argument 1 of AudioWorkletGlobalScope.registerProcessor is invalid: a "
        "class with the same name is already registered.");
    return;
  }

  // We know processorConstructor is callable, so not a WindowProxy or Location.
  JS::Rooted<JSObject*> constructorUnwrapped(
      aCx, js::CheckedUnwrapStatic(processorConstructor));
  if (!constructorUnwrapped) {
    // If the caller's compartment does not have permission to access the
    // unwrapped constructor then throw.
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  /**
   * 3. If the result of IsConstructor(argument=processorCtor) is false,
   *    throw a TypeError and abort these steps.
   */
  if (!JS::IsConstructor(constructorUnwrapped)) {
    aRv.ThrowTypeError<MSG_NOT_CONSTRUCTOR>(NS_LITERAL_STRING(
        "Argument 2 of AudioWorkletGlobalScope.registerProcessor"));
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
    aRv.ThrowTypeError<MSG_NOT_OBJECT>(NS_LITERAL_STRING(
        "Argument 2 of AudioWorkletGlobalScope.registerProcessor "
        "processorCtor.prototype"));
    return;
  }

  /**
   * 6. If the result of IsCallable(argument=Get(O=prototype, P="process"))
   *    is false, throw a TypeError and abort these steps.
   */
  JS::Rooted<JS::Value> process(aCx);
  JS::Rooted<JSObject*> prototypeObject(aCx, &prototype.toObject());
  if (!JS_GetProperty(aCx, prototypeObject, "process", &process)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  if (!process.isObjectOrNull() || !JS::IsCallable(process.toObjectOrNull())) {
    aRv.ThrowTypeError<MSG_NOT_CALLABLE>(NS_LITERAL_STRING(
        "Argument 2 of AudioWorkletGlobalScope.registerProcessor "
        "constructor.process"));
    return;
  }

  /**
   * 7. Let descriptors be the result of Get(O=processorCtor,
   *    P="parameterDescriptors").
   */
  JS::Rooted<JS::Value> descriptors(aCx);
  if (!JS_GetProperty(aCx, processorConstructor, "parameterDescriptors",
                      &descriptors)) {
    aRv.NoteJSContextException(aCx);
    return;
  }

  /**
   * 8. If descriptors is neither an array nor undefined, throw a
   *    TypeError and abort these steps.
   */
  bool isArray = false;
  if (!JS_IsArrayObject(aCx, descriptors, &isArray)) {
    // I would assume isArray won't be set to true if JS_IsArrayObject
    // failed, but just in case, force it to false
    isArray = false;
    JS_ClearPendingException(aCx);
  }

  if (!descriptors.isUndefined() && !isArray) {
    aRv.ThrowTypeError<MSG_NOT_ARRAY_NOR_UNDEFINED>(NS_LITERAL_STRING(
        "Argument 2 of AudioWorkletGlobalScope.registerProcessor "
        "constructor.parameterDescriptors"));
    return;
  }

  /**
   * 9. Let definition be a new AudioWorkletProcessor definition with:
   *    - node name being name
   *    - processor class constructor being processorCtor
   * 10. Add the key-value pair (name - definition) to the node name to
   *     processor definition map of the associated AudioWorkletGlobalScope.
   */
  if (!mNameToProcessorMap.Put(aName, &aProcessorCtor, fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  /**
   * 11. Queue a task to the control thread to add the key-value pair
   *     (name - descriptors) to the node name to parameter descriptor
   *     map of the associated BaseAudioContext.
   */
  AudioParamDescriptorMap map = DescriptorsFromJS(aCx, descriptors, aRv);
  if (aRv.Failed()) {
    return;
  }

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
    JSContext* aCx, const JS::Rooted<JS::Value>& aDescriptors,
    ErrorResult& aRv) {
  // We already checked if aDescriptors is an array or undefined in step 8 of
  // registerProcessor, so we should be confident aDescriptors if valid here
  if (aDescriptors.isUndefined()) {
    return AudioParamDescriptorMap();
  }
  MOZ_ASSERT(aDescriptors.isObject());

  AudioParamDescriptorMap res;
  // To check for duplicates
  nsTHashtable<nsStringHashKey> namesSet;

  JS::Rooted<JSObject*> aDescriptorsArray(aCx, &aDescriptors.toObject());
  uint32_t length = 0;
  if (!JS_GetArrayLength(aCx, aDescriptorsArray, &length)) {
    aRv.NoteJSContextException(aCx);
    return AudioParamDescriptorMap();
  }

  for (uint32_t i = 0; i < length; ++i) {
    JS::Rooted<JS::Value> descriptorElement(aCx);
    if (!JS_GetElement(aCx, aDescriptorsArray, i, &descriptorElement)) {
      aRv.NoteJSContextException(aCx);
      return AudioParamDescriptorMap();
    }

    AudioParamDescriptor descriptor;
    nsPrintfCString sourceDescription("Element %u in parameterDescriptors", i);
    if (!descriptor.Init(aCx, descriptorElement, sourceDescription.get())) {
      aRv.NoteJSContextException(aCx);
      return AudioParamDescriptorMap();
    }

    if (namesSet.Contains(descriptor.mName)) {
      aRv.ThrowDOMException(
          NS_ERROR_DOM_NOT_SUPPORTED_ERR,
          NS_LITERAL_CSTRING("Duplicated name \"") +
              NS_ConvertUTF16toUTF8(descriptor.mName) +
              NS_LITERAL_CSTRING("\" in parameterDescriptors."));
      return AudioParamDescriptorMap();
    }

    if (descriptor.mMinValue > descriptor.mMaxValue) {
      aRv.ThrowDOMException(
          NS_ERROR_DOM_NOT_SUPPORTED_ERR,
          NS_LITERAL_CSTRING("In parameterDescriptors, ") +
              NS_ConvertUTF16toUTF8(descriptor.mName) +
              NS_LITERAL_CSTRING(" minValue should be smaller than maxValue."));
      return AudioParamDescriptorMap();
    }

    if (descriptor.mDefaultValue < descriptor.mMinValue ||
        descriptor.mDefaultValue > descriptor.mMaxValue) {
      aRv.ThrowDOMException(
          NS_ERROR_DOM_NOT_SUPPORTED_ERR,
          NS_LITERAL_CSTRING("In parameterDescriptors, ") +
              NS_ConvertUTF16toUTF8(descriptor.mName) +
              NS_LITERAL_CSTRING(" defaultValue is out of the range defined by "
                                 "minValue and maxValue."));
      return AudioParamDescriptorMap();
    }

    if (!res.AppendElement(descriptor)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return AudioParamDescriptorMap();
    }

    if (!namesSet.PutEntry(descriptor.mName, fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return AudioParamDescriptorMap();
    }
  }

  return res;
}

bool AudioWorkletGlobalScope::ConstructProcessor(
    const nsAString& aName, NotNull<StructuredCloneHolder*> aSerializedOptions,
    JS::MutableHandle<JSObject*> aRetProcessor) {
  /**
   * See
   * https://webaudio.github.io/web-audio-api/#AudioWorkletProcessor-instantiation
   */
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(this))) {
    return false;
  }
  JSContext* cx = jsapi.cx();
  ErrorResult rv;
  /** TODO https://bugzilla.mozilla.org/show_bug.cgi?id=1565956
   * 4. Let deserializedPort be the result of
   *    StructuredDeserialize(serializedPort, the current Realm).
   */
  /**
   * 5. Let deserializedOptions be the result of
   *    StructuredDeserialize(serializedOptions, the current Realm).
   */
  JS::Rooted<JS::Value> deserializedOptions(cx);
  aSerializedOptions->Read(this, cx, &deserializedOptions, rv);
  if (rv.MaybeSetPendingException(cx)) {
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
  /** TODO https://bugzilla.mozilla.org/show_bug.cgi?id=1565956
   * 7. Store nodeReference and deserializedPort to node reference and
   *    transferred port of this AudioWorkletGlobalScope's pending processor
   *    construction data respectively.
   */
  /**
   * 8. Construct a callback function from processorCtor with the argument
   *    of deserializedOptions.
   */
  // The options were an object before serialization and so will be an object
  // if deserialization succeeded above.  toObject() asserts.
  JS::Rooted<JSObject*> options(cx, &deserializedOptions.toObject());
  RefPtr<AudioWorkletProcessor> processor = processorCtor->Construct(
      options, rv, "AudioWorkletProcessor construction",
      CallbackFunction::eReportExceptions);
  if (rv.Failed()) {
    rv.SuppressException();  // already reported
    return false;
  }
  JS::Rooted<JS::Value> processorVal(cx);
  if (NS_WARN_IF(!ToJSValue(cx, processor, &processorVal))) {
    return false;
  }
  MOZ_ASSERT(processorVal.isObject());
  aRetProcessor.set(&processorVal.toObject());
  return true;
}

}  // namespace dom
}  // namespace mozilla
