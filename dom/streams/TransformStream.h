/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_STREAMS_TRANSFORMSTREAM_H_
#define DOM_STREAMS_TRANSFORMSTREAM_H_

#include "TransformStreamDefaultController.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/QueuingStrategyBinding.h"

#include "mozilla/dom/TransformerBinding.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

#ifndef MOZ_DOM_STREAMS
#  error "Shouldn't be compiling with this header without MOZ_DOM_STREAMS set"
#endif

namespace mozilla::dom {

class WritableStream;
class ReadableStream;

class TransformStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TransformStream)

  TransformStreamDefaultController* Controller() { return mController; }
  void SetController(TransformStreamDefaultController* aController) {
    mController = aController;
  }

 protected:
  ~TransformStream();
  explicit TransformStream(nsIGlobalObject* aGlobal);

 public:
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL methods
  // TODO: Mark as MOZ_CAN_RUN_SCRIPT when IDL constructors can be (bug 1749042)
  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<TransformStream>
  Constructor(const GlobalObject& aGlobal,
              const Optional<JS::Handle<JSObject*>>& aTransformer,
              const QueuingStrategy& aWritableStrategy,
              const QueuingStrategy& aReadableStrategy, ErrorResult& aRv);

  already_AddRefed<ReadableStream> GetReadable(ErrorResult& aRv);
  already_AddRefed<WritableStream> GetWritable(ErrorResult& aRv);

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Internal slots
  RefPtr<TransformStreamDefaultController> mController;
};

}  // namespace mozilla::dom

#endif  // DOM_STREAMS_TRANSFORMSTREAM_H_
