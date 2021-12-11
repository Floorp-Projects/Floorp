/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioWorkletGlobalScope_h
#define mozilla_dom_AudioWorkletGlobalScope_h

#include "mozilla/dom/AudioParamDescriptorMap.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "js/ForOfIterator.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {

class AudioWorkletImpl;

namespace dom {

class AudioWorkletProcessorConstructor;
class MessagePort;
class StructuredCloneHolder;
class UniqueMessagePortId;

class AudioWorkletGlobalScope final : public WorkletGlobalScope {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioWorkletGlobalScope,
                                           WorkletGlobalScope);

  explicit AudioWorkletGlobalScope(AudioWorkletImpl* aImpl);

  bool WrapGlobalObject(JSContext* aCx,
                        JS::MutableHandle<JSObject*> aReflector) override;

  void RegisterProcessor(JSContext* aCx, const nsAString& aName,
                         AudioWorkletProcessorConstructor& aProcessorCtor,
                         ErrorResult& aRv);

  WorkletImpl* Impl() const override;

  uint64_t CurrentFrame() const;

  double CurrentTime() const;

  float SampleRate() const;

  // If successful, returns true and sets aRetProcessor, which will be in the
  // compartment for the realm of this global.  Returns false on failure.
  MOZ_CAN_RUN_SCRIPT
  bool ConstructProcessor(JSContext* aCx, const nsAString& aName,
                          NotNull<StructuredCloneHolder*> aSerializedOptions,
                          UniqueMessagePortId& aPortIdentifier,
                          JS::MutableHandle<JSObject*> aRetProcessor);

  // Returns null if not called during ConstructProcessor() or if the port has
  // already been taken.
  RefPtr<MessagePort> TakePortForProcessorCtor();

 private:
  ~AudioWorkletGlobalScope() = default;

  // Returns an AudioParamDescriptorMap filled with AudioParamDescriptor
  // objects, extracted from JS. Returns an empty map in case of error and set
  // aRv accordingly.
  AudioParamDescriptorMap DescriptorsFromJS(JSContext* aCx,
                                            JS::ForOfIterator* aIter,
                                            ErrorResult& aRv);

  const RefPtr<AudioWorkletImpl> mImpl;

  typedef nsRefPtrHashtable<nsStringHashKey, AudioWorkletProcessorConstructor>
      NodeNameToProcessorDefinitionMap;
  NodeNameToProcessorDefinitionMap mNameToProcessorMap;
  // https://webaudio.github.io/web-audio-api/#pending-processor-construction-data-transferred-port
  // This does not need to be traversed during cycle-collection because it is
  // only set while this AudioWorkletGlobalScope is on the stack.
  RefPtr<MessagePort> mPortForProcessor;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_AudioWorkletGlobalScope_h
