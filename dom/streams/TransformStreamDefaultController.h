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

namespace mozilla::dom {

class TransformStream;
class TransformerAlgorithmsBase;

class TransformStreamDefaultController final : public nsISupports,
                                               public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TransformStreamDefaultController)

  MOZ_KNOWN_LIVE TransformStream* Stream();
  void SetStream(TransformStream& aStream);
  TransformerAlgorithmsBase* Algorithms();
  void SetAlgorithms(TransformerAlgorithmsBase* aTransformerAlgorithms);

  explicit TransformStreamDefaultController(nsIGlobalObject* aGlobal);

 protected:
  ~TransformStreamDefaultController();

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  Nullable<double> GetDesiredSize() const;

  MOZ_CAN_RUN_SCRIPT void Enqueue(JSContext* aCx, JS::Handle<JS::Value> aChunk,
                                  ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void Error(JSContext* aCx, JS::Handle<JS::Value> aError,
                                ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT void Terminate(JSContext* aCx, ErrorResult& aRv);

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Internal slots
  RefPtr<TransformStream> mStream;
  RefPtr<TransformerAlgorithmsBase> mTransformerAlgorithms;
};

void SetUpTransformStreamDefaultController(
    JSContext* aCx, TransformStream& aStream,
    TransformStreamDefaultController& aController,
    TransformerAlgorithmsBase& aTransformerAlgorithms);

void SetUpTransformStreamDefaultControllerFromTransformer(
    JSContext* aCx, TransformStream& aStream,
    JS::Handle<JSObject*> aTransformer, Transformer& aTransformerDict);

}  // namespace mozilla::dom

#endif  // DOM_STREAMS_TRANSFORMSTREAMDEFAULTCONTROLLER_H_
