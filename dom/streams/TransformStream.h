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

namespace mozilla::dom {

class WritableStream;
class ReadableStream;
class UniqueMessagePortId;
class MessagePort;
class TransformerAlgorithmsWrapper;

class TransformStream final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TransformStream)

  // https://streams.spec.whatwg.org/#transformstream-set-up
  // Intended to be used by interfaces using GenericTransformStream mixin.
  MOZ_CAN_RUN_SCRIPT static already_AddRefed<TransformStream> CreateGeneric(
      const GlobalObject& aGlobal, TransformerAlgorithmsWrapper& aAlgorithms,
      ErrorResult& aRv);

  TransformStream(nsIGlobalObject* aGlobal, ReadableStream* aReadable,
                  WritableStream* aWritable);

  // Internal slot accessors
  bool Backpressure() const { return mBackpressure; }
  Promise* BackpressureChangePromise() { return mBackpressureChangePromise; }
  void SetBackpressure(bool aBackpressure, ErrorResult& aRv);
  MOZ_KNOWN_LIVE TransformStreamDefaultController* Controller() {
    return mController;
  }
  void SetController(TransformStreamDefaultController& aController) {
    MOZ_ASSERT(!mController);
    mController = &aController;
  }

  // [Transferable]
  // https://html.spec.whatwg.org/multipage/structured-data.html#transfer-steps
  MOZ_CAN_RUN_SCRIPT bool Transfer(JSContext* aCx,
                                   UniqueMessagePortId& aPortId1,
                                   UniqueMessagePortId& aPortId2);
  // https://html.spec.whatwg.org/multipage/structured-data.html#transfer-receiving-steps
  static MOZ_CAN_RUN_SCRIPT bool ReceiveTransfer(
      JSContext* aCx, nsIGlobalObject* aGlobal, MessagePort& aPort1,
      MessagePort& aPort2, JS::MutableHandle<JSObject*> aReturnObject);

 protected:
  ~TransformStream();
  explicit TransformStream(nsIGlobalObject* aGlobal);

  MOZ_CAN_RUN_SCRIPT void Initialize(
      JSContext* aCx, Promise* aStartPromise, double aWritableHighWaterMark,
      QueuingStrategySize* aWritableSizeAlgorithm,
      double aReadableHighWaterMark,
      QueuingStrategySize* aReadableSizeAlgorithm, ErrorResult& aRv);

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

  ReadableStream* Readable() const { return mReadable; }
  WritableStream* Writable() const { return mWritable; }

 private:
  nsCOMPtr<nsIGlobalObject> mGlobal;

  // Internal slots
  // MOZ_KNOWN_LIVE for slots that will never be reassigned
  bool mBackpressure = false;
  RefPtr<Promise> mBackpressureChangePromise;
  RefPtr<TransformStreamDefaultController> mController;
  RefPtr<ReadableStream> mReadable;
  RefPtr<WritableStream> mWritable;
};

MOZ_CAN_RUN_SCRIPT void TransformStreamErrorWritableAndUnblockWrite(
    JSContext* aCx, TransformStream* aStream, JS::Handle<JS::Value> aError,
    ErrorResult& aRv);

MOZ_CAN_RUN_SCRIPT void TransformStreamError(JSContext* aCx,
                                             TransformStream* aStream,
                                             JS::Handle<JS::Value> aError,
                                             ErrorResult& aRv);

}  // namespace mozilla::dom

#endif  // DOM_STREAMS_TRANSFORMSTREAM_H_
