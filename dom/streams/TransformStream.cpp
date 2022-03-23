/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStream.h"

#include "UnderlyingSourceCallbackHelpers.h"
#include "js/TypeDecls.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TransformStreamBinding.h"
#include "mozilla/dom/TransformerBinding.h"
#include "mozilla/dom/StreamUtils.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransformStream, mGlobal,
                                      mBackpressureChangePromise, mController,
                                      mReadable, mWritable)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TransformStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TransformStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransformStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TransformStream::TransformStream(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

TransformStream::~TransformStream() { mozilla::DropJSObjects(this); }

JSObject* TransformStream::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return TransformStream_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#initialize-transform-stream
class TransformStreamUnderlyingSinkAlgorithms final
    : public UnderlyingSinkAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      TransformStreamUnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)

  TransformStreamUnderlyingSinkAlgorithms(Promise* aStartPromise,
                                          TransformStream* aStream)
      : mStartPromise(aStartPromise), mStream(aStream) {}

  void StartCallback(JSContext* aCx,
                     WritableStreamDefaultController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) override {
    // Step 1. Let startAlgorithm be an algorithm that returns startPromise.
    // (Same as TransformStreamUnderlyingSourceAlgorithms::StartCallback)
    aRetVal.setObject(*mStartPromise->PromiseObj());
  }

  already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override {
    // Step 2. Let writeAlgorithm be the following steps, taking a chunk
    // argument:
    //  Step 1. Return ! TransformStreamDefaultSinkWriteAlgorithm(stream,
    //  chunk).
    // TODO
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 3. Let abortAlgorithm be the following steps, taking a reason
    // argument:
    //  Step 1. Return ! TransformStreamDefaultSinkAbortAlgorithm(stream,
    //  reason).
    // TODO
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  already_AddRefed<Promise> CloseCallback(JSContext* aCx,
                                          ErrorResult& aRv) override {
    // Step 4. Let closeAlgorithm be the following steps:

    //   Step 1. Return ! TransformStreamDefaultSinkCloseAlgorithm(stream).
    // TODO
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

 protected:
  ~TransformStreamUnderlyingSinkAlgorithms() override = default;

 private:
  RefPtr<Promise> mStartPromise;
  RefPtr<TransformStream> mStream;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(TransformStreamUnderlyingSinkAlgorithms,
                                   UnderlyingSinkAlgorithmsBase, mStartPromise,
                                   mStream)
NS_IMPL_ADDREF_INHERITED(TransformStreamUnderlyingSinkAlgorithms,
                         UnderlyingSinkAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(TransformStreamUnderlyingSinkAlgorithms,
                          UnderlyingSinkAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransformStreamUnderlyingSinkAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(TransformStreamUnderlyingSinkAlgorithms)

// https://streams.spec.whatwg.org/#initialize-transform-stream
class TransformStreamUnderlyingSourceAlgorithms final
    : public UnderlyingSourceAlgorithmsBase {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(
      TransformStreamUnderlyingSourceAlgorithms, UnderlyingSourceAlgorithmsBase)

  TransformStreamUnderlyingSourceAlgorithms(Promise* aStartPromise,
                                            TransformStream* aStream)
      : mStartPromise(aStartPromise), mStream(aStream) {}

  void StartCallback(JSContext* aCx, ReadableStreamController& aController,
                     JS::MutableHandle<JS::Value> aRetVal,
                     ErrorResult& aRv) override {
    // Step 1. Let startAlgorithm be an algorithm that returns startPromise.
    // (Same as TransformStreamUnderlyingSinkAlgorithms::StartCallback)
    aRetVal.setObject(*mStartPromise->PromiseObj());
  }

  already_AddRefed<Promise> PullCallback(JSContext* aCx,
                                         ReadableStreamController& aController,
                                         ErrorResult& aRv) override {
    // Step 6. Let pullAlgorithm be the following steps:
    //   Step 1. Return ! TransformStreamDefaultSourcePullAlgorithm(stream).
    // TODO
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 7. Let cancelAlgorithm be the following steps, taking a reason
    // argument:
    //  Step 1. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    //  reason).
    //  Step 2. Return a promise resolved with undefined.
    // TODO
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  void ErrorCallback() override {}

 protected:
  ~TransformStreamUnderlyingSourceAlgorithms() override = default;

 private:
  RefPtr<Promise> mStartPromise;
  RefPtr<TransformStream> mStream;
};

NS_IMPL_CYCLE_COLLECTION_INHERITED(TransformStreamUnderlyingSourceAlgorithms,
                                   UnderlyingSourceAlgorithmsBase,
                                   mStartPromise, mStream)
NS_IMPL_ADDREF_INHERITED(TransformStreamUnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(TransformStreamUnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    TransformStreamUnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(TransformStreamUnderlyingSourceAlgorithms)

// https://streams.spec.whatwg.org/#transform-stream-set-backpressure
static void TransformStreamSetBackpressure(TransformStream* aStream,
                                           bool aBackpressure,
                                           ErrorResult& aRv) {
  // Step 1. Assert: stream.[[backpressure]] is not backpressure.
  MOZ_ASSERT(aStream->Backpressure() != aBackpressure);

  // Step 2. If stream.[[backpressureChangePromise]] is not undefined, resolve
  // stream.[[backpressureChangePromise]] with undefined.
  if (Promise* promise = aStream->BackpressureChangePromise()) {
    promise->MaybeResolveWithUndefined();
  }

  // Step 3. Set stream.[[backpressureChangePromise]] to a new promise.
  RefPtr<Promise> promise = Promise::Create(aStream->GetParentObject(), aRv);
  if (aRv.Failed()) {
    return;
  }
  aStream->SetBackpressureChangePromise(promise);

  // Step 4. Set stream.[[backpressure]] to backpressure.
  aStream->SetBackpressure(aBackpressure);
}

// https://streams.spec.whatwg.org/#initialize-transform-stream
void TransformStream::Initialize(JSContext* aCx, Promise* aStartPromise,
                                 double aWritableHighWaterMark,
                                 QueuingStrategySize* aWritableSizeAlgorithm,
                                 double aReadableHighWaterMark,
                                 QueuingStrategySize* aReadableSizeAlgorithm,
                                 ErrorResult& aRv) {
  // Step 1 - 4
  auto sinkAlgorithms =
      MakeRefPtr<TransformStreamUnderlyingSinkAlgorithms>(aStartPromise, this);

  // Step 5. Set stream.[[writable]] to ! CreateWritableStream(startAlgorithm,
  // writeAlgorithm, closeAlgorithm, abortAlgorithm, writableHighWaterMark,
  // writableSizeAlgorithm).
  mWritable =
      CreateWritableStream(aCx, MOZ_KnownLive(mGlobal), sinkAlgorithms,
                           aWritableHighWaterMark, aWritableSizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 6 - 7
  auto sourceAlgorithms = MakeRefPtr<TransformStreamUnderlyingSourceAlgorithms>(
      aStartPromise, this);

  // Step 8. Set stream.[[readable]] to ! CreateReadableStream(startAlgorithm,
  // pullAlgorithm, cancelAlgorithm, readableHighWaterMark,
  // readableSizeAlgorithm).
  mReadable = CreateReadableStream(
      aCx, MOZ_KnownLive(mGlobal), sourceAlgorithms,
      Some(aReadableHighWaterMark), aReadableSizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 9. Set stream.[[backpressure]] and
  // stream.[[backpressureChangePromise]] to undefined.
  mBackpressureChangePromise = nullptr;

  // Step 10. Perform ! TransformStreamSetBackpressure(stream, true).
  TransformStreamSetBackpressure(this, true, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 11. Set stream.[[controller]] to undefined.
  mController = nullptr;
}

// https://streams.spec.whatwg.org/#ts-constructor
already_AddRefed<TransformStream> TransformStream::Constructor(
    const GlobalObject& aGlobal,
    const Optional<JS::Handle<JSObject*>>& aTransformer,
    const QueuingStrategy& aWritableStrategy,
    const QueuingStrategy& aReadableStrategy, ErrorResult& aRv) {
  // Step 1. If transformer is missing, set it to null.
  JS::Rooted<JSObject*> transformerObj(
      aGlobal.Context(),
      aTransformer.WasPassed() ? aTransformer.Value() : nullptr);

  // Step 2. Let transformerDict be transformer, converted to an IDL value of
  // type Transformer.
  RootedDictionary<Transformer> transformerDict(aGlobal.Context());
  if (transformerObj) {
    JS::Rooted<JS::Value> objValue(aGlobal.Context(),
                                   JS::ObjectValue(*transformerObj));
    dom::BindingCallContext callCx(aGlobal.Context(),
                                   "TransformStream.constructor");
    aRv.MightThrowJSException();
    if (!transformerDict.Init(callCx, objValue)) {
      aRv.StealExceptionFromJSContext(aGlobal.Context());
      return nullptr;
    }
  }

  // Step 3. If transformerDict["readableType"] exists, throw a RangeError
  // exception.
  if (!transformerDict.mReadableType.isUndefined()) {
    aRv.ThrowRangeError(
        "`readableType` is unsupported and preserved for future use");
    return nullptr;
  }

  // Step 4. If transformerDict["writableType"] exists, throw a RangeError
  // exception.
  if (!transformerDict.mWritableType.isUndefined()) {
    aRv.ThrowRangeError(
        "`writableType` is unsupported and preserved for future use");
    return nullptr;
  }

  // Step 5. Let readableHighWaterMark be ?
  // ExtractHighWaterMark(readableStrategy, 0).
  double readableHighWaterMark =
      ExtractHighWaterMark(aReadableStrategy, 0, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 6. Let readableSizeAlgorithm be !
  // ExtractSizeAlgorithm(readableStrategy).
  // Note: Callers should recognize nullptr as a callback that returns 1. See
  // also WritableStream::Constructor for this design decision.
  RefPtr<QueuingStrategySize> readableSizeAlgorithm =
      aReadableStrategy.mSize.WasPassed() ? &aReadableStrategy.mSize.Value()
                                          : nullptr;

  // Step 7. Let writableHighWaterMark be ?
  // ExtractHighWaterMark(writableStrategy, 1).
  double writableHighWaterMark =
      ExtractHighWaterMark(aWritableStrategy, 1, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 8. Let writableSizeAlgorithm be !
  // ExtractSizeAlgorithm(writableStrategy).
  // Note: Callers should recognize nullptr as a callback that returns 1. See
  // also WritableStream::Constructor for this design decision.
  RefPtr<QueuingStrategySize> writableSizeAlgorithm =
      aWritableStrategy.mSize.WasPassed() ? &aWritableStrategy.mSize.Value()
                                          : nullptr;

  // Step 9. Let startPromise be a new promise.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> startPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 10. Perform ! InitializeTransformStream(this, startPromise,
  // writableHighWaterMark, writableSizeAlgorithm, readableHighWaterMark,
  // readableSizeAlgorithm).
  RefPtr<TransformStream> transformStream = new TransformStream(global);
  transformStream->Initialize(
      aGlobal.Context(), startPromise, writableHighWaterMark,
      writableSizeAlgorithm, readableHighWaterMark, readableSizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 11. Perform ?
  // SetUpTransformStreamDefaultControllerFromTransformer(this, transformer,
  // transformerDict).
  SetUpTransformStreamDefaultControllerFromTransformer(
      aGlobal.Context(), *transformStream, transformerObj, transformerDict);

  // Step 12. If transformerDict["start"] exists, then resolve startPromise with
  // the result of invoking transformerDict["start"] with argument list «
  // this.[[controller]] » and callback this value transformer.
  if (transformerDict.mStart.WasPassed()) {
    RefPtr<TransformerStartCallback> callback = transformerDict.mStart.Value();
    RefPtr<TransformStreamDefaultController> controller =
        transformStream->Controller();
    JS::Rooted<JS::Value> retVal(aGlobal.Context());
    callback->Call(transformerObj, *controller, &retVal, aRv,
                   "Transformer.start", CallbackFunction::eRethrowExceptions);
    if (aRv.Failed()) {
      return nullptr;
    }

    startPromise->MaybeResolve(retVal);
  } else {
    // Step 13. Otherwise, resolve startPromise with undefined.
    startPromise->MaybeResolveWithUndefined();
  }

  return transformStream.forget();
}

already_AddRefed<ReadableStream> TransformStream::GetReadable(
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<WritableStream> TransformStream::GetWritable(
    ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

}  // namespace mozilla::dom
