/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_STREAMS_TRANSFORMSTREAMDEFAULTCONTROLLER_H_
#define DOM_STREAMS_TRANSFORMSTREAMDEFAULTCONTROLLER_H_

#include "js/TypeDecls.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"

#include "mozilla/dom/TransformerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#ifndef MOZ_DOM_STREAMS
#  error "Shouldn't be compiling with this header without MOZ_DOM_STREAMS set"
#endif

namespace mozilla::dom {

class TransformStream;

class TransformStreamDefaultController final : public nsISupports,
                                               public nsWrapperCache {
 public:
  using TransformAlgorithm = already_AddRefed<Promise> (*)(
      JSContext* aCx, TransformStreamDefaultController& aController,
      JS::HandleValue aChunk, ErrorResult& aRv);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TransformStreamDefaultController)

  void SetTransformAlgorithm(TransformAlgorithm aTransformAlgorithm) {
    mTransformAlgorithm = aTransformAlgorithm;
  }

  void SetTransformerMembers(TransformerTransformCallback* aTransformCallback,
                             JS::HandleObject aTransformer) {
    mTransformCallback = aTransformCallback;
    mTransformer = aTransformer;
  }
  TransformerTransformCallback* GetTransformCallback() const {
    return mTransformCallback;
  }
  JS::Heap<JSObject*>& GetTransformer() { return mTransformer; }

  explicit TransformStreamDefaultController(nsIGlobalObject* aGlobal);

 protected:
  ~TransformStreamDefaultController();

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Nullable<double> GetDesiredSize() const;

  void Enqueue(JSContext* aCx, JS::Handle<JS::Value> aChunk, ErrorResult& aRv);
  void Error(JSContext* aCx, JS::Handle<JS::Value> aError, ErrorResult& aRv);
  void Terminate(ErrorResult& aRv);

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Internal slots
  TransformAlgorithm mTransformAlgorithm;

  // Members for [[transformAlgorithm]]
  RefPtr<TransformerTransformCallback> mTransformCallback;
  JS::Heap<JSObject*> mTransformer;
};

void SetUpTransformStreamDefaultControllerFromTransformer(
    JSContext* aCx, TransformStream& aStream, JS::HandleObject aTransformer,
    Transformer& aTransformerDict);

}  // namespace mozilla::dom

#endif  // DOM_STREAMS_TRANSFORMSTREAMDEFAULTCONTROLLER_H_
