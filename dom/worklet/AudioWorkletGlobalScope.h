/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AudioWorkletGlobalScope_h
#define mozilla_dom_AudioWorkletGlobalScope_h

#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/WorkletGlobalScope.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {

class WorkletImpl;

namespace dom {


class AudioWorkletGlobalScope final : public WorkletGlobalScope
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(AudioWorkletGlobalScope, WorkletGlobalScope);

  explicit AudioWorkletGlobalScope(WorkletImpl* aImpl);

  bool
  WrapGlobalObject(JSContext* aCx,
                   JS::MutableHandle<JSObject*> aReflector) override;

  void
  RegisterProcessor(JSContext* aCx, const nsAString& aName,
                    VoidFunction& aProcessorCtor,
                    ErrorResult& aRv);

  uint64_t CurrentFrame() const;

  double CurrentTime() const;

  float SampleRate() const;

private:
  ~AudioWorkletGlobalScope() = default;

  uint64_t mCurrentFrame;
  double mCurrentTime;
  float mSampleRate;

  typedef nsRefPtrHashtable<nsStringHashKey, VoidFunction> NodeNameToProcessorDefinitionMap;
  NodeNameToProcessorDefinitionMap mNameToProcessorMap;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_AudioWorkletGlobalScope_h
