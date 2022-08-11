/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_STREAMS_TRANSFORMERCALLBACKHELPERS_H_
#define DOM_STREAMS_TRANSFORMERCALLBACKHELPERS_H_

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/TransformerBinding.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla::dom {

class Promise;

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
class TransformerAlgorithms final {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(TransformerAlgorithms)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(TransformerAlgorithms)

  TransformerAlgorithms(nsIGlobalObject* aGlobal,
                        JS::Handle<JSObject*> aTransformer,
                        Transformer& aTransformerDict)
      : mGlobal(aGlobal), mTransformer(aTransformer) {
    // Step 4. (Step 2 is implicitly done through the initialization of
    // mTransformCallback to null)
    if (aTransformerDict.mTransform.WasPassed()) {
      mTransformCallback = aTransformerDict.mTransform.Value();
    }

    // Step 5. (Step 3 is implicitly done through the initialization of
    // mTransformCallback to null)
    if (aTransformerDict.mFlush.WasPassed()) {
      mFlushCallback = aTransformerDict.mFlush.Value();
    }

    mozilla::HoldJSObjects(this);
  };

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> TransformCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      TransformStreamDefaultController& aController, ErrorResult& aRv);

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> FlushCallback(
      JSContext* aCx, TransformStreamDefaultController& aController,
      ErrorResult& aRv);

 protected:
  ~TransformerAlgorithms() { mozilla::DropJSObjects(this); }

 private:
  // Virtually const, but are cycle collected
  nsCOMPtr<nsIGlobalObject> mGlobal;
  JS::Heap<JSObject*> mTransformer;
  MOZ_KNOWN_LIVE RefPtr<TransformerTransformCallback> mTransformCallback;
  MOZ_KNOWN_LIVE RefPtr<TransformerFlushCallback> mFlushCallback;
};

}  // namespace mozilla::dom

#endif  // DOM_STREAMS_TRANSFORMERCALLBACKHELPERS_H_
