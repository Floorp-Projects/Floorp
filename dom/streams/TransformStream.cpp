/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStream.h"

#include "TransformerCallbackHelpers.h"
#include "UnderlyingSourceCallbackHelpers.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Promise-inl.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TransformStreamBinding.h"
#include "mozilla/dom/TransformerBinding.h"
#include "mozilla/dom/StreamUtils.h"
#include "nsWrapperCache.h"

// XXX: GCC somehow does not allow attributes before lambda return types, while
// clang requires so. See also bug 1627007.
#ifdef __clang__
#  define MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA MOZ_CAN_RUN_SCRIPT_BOUNDARY
#else
#  define MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA
#endif

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

// https://streams.spec.whatwg.org/#transformstream-set-up
// (except this instead creates a new TransformStream rather than accepting an
// existing instance)
already_AddRefed<TransformStream> TransformStream::CreateGeneric(
    const GlobalObject& aGlobal, TransformerAlgorithmsWrapper& aAlgorithms,
    ErrorResult& aRv) {
  // Step 1. Let writableHighWaterMark be 1.
  double writableHighWaterMark = 1;

  // Step 2. Let writableSizeAlgorithm be an algorithm that returns 1.
  // Note: Callers should recognize nullptr as a callback that returns 1. See
  // also WritableStream::Constructor for this design decision.
  RefPtr<QueuingStrategySize> writableSizeAlgorithm;

  // Step 3. Let readableHighWaterMark be 0.
  double readableHighWaterMark = 0;

  // Step 4. Let readableSizeAlgorithm be an algorithm that returns 1.
  // Note: Callers should recognize nullptr as a callback that returns 1. See
  // also ReadableStream::Constructor for this design decision.
  RefPtr<QueuingStrategySize> readableSizeAlgorithm;

  // Step 5. Let transformAlgorithmWrapper be an algorithm that runs these steps
  // given a value chunk:
  // Step 6. Let flushAlgorithmWrapper be an algorithm that runs these steps:
  // (Done by TransformerAlgorithmsWrapper)

  // Step 7. Let startPromise be a promise resolved with undefined.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<Promise> startPromise =
      Promise::CreateResolvedWithUndefined(global, aRv);
  if (!startPromise) {
    return nullptr;
  }

  // Step 8. Perform ! InitializeTransformStream(stream, startPromise,
  // writableHighWaterMark, writableSizeAlgorithm, readableHighWaterMark,
  // readableSizeAlgorithm).
  auto stream = MakeRefPtr<TransformStream>(global, nullptr, nullptr);
  stream->Initialize(aGlobal.Context(), startPromise, writableHighWaterMark,
                     writableSizeAlgorithm, readableHighWaterMark,
                     readableSizeAlgorithm, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 9. Let controller be a new TransformStreamDefaultController.
  auto controller = MakeRefPtr<TransformStreamDefaultController>(global);

  // Step 10. Perform ! SetUpTransformStreamDefaultController(stream,
  // controller, transformAlgorithmWrapper, flushAlgorithmWrapper).
  SetUpTransformStreamDefaultController(aGlobal.Context(), *stream, *controller,
                                        aAlgorithms);

  return stream.forget();
}

TransformStream::TransformStream(nsIGlobalObject* aGlobal) : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

TransformStream::TransformStream(nsIGlobalObject* aGlobal,
                                 ReadableStream* aReadable,
                                 WritableStream* aWritable)
    : mGlobal(aGlobal), mReadable(aReadable), mWritable(aWritable) {
  mozilla::HoldJSObjects(this);
}

TransformStream::~TransformStream() { mozilla::DropJSObjects(this); }

JSObject* TransformStream::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return TransformStream_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#transform-stream-error-writable-and-unblock-write
void TransformStreamErrorWritableAndUnblockWrite(JSContext* aCx,
                                                 TransformStream* aStream,
                                                 JS::Handle<JS::Value> aError,
                                                 ErrorResult& aRv) {
  // Step 1: Perform !
  // TransformStreamDefaultControllerClearAlgorithms(stream.[[controller]]).
  aStream->Controller()->SetAlgorithms(nullptr);

  // Step 2: Perform !
  // WritableStreamDefaultControllerErrorIfNeeded(stream.[[writable]].[[controller]],
  // e).
  // TODO: Remove MOZ_KnownLive (bug 1761577)
  WritableStreamDefaultControllerErrorIfNeeded(
      aCx, MOZ_KnownLive(aStream->Writable()->Controller()), aError, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 3: If stream.[[backpressure]] is true, perform !
  // TransformStreamSetBackpressure(stream, false).
  if (aStream->Backpressure()) {
    aStream->SetBackpressure(false, aRv);
  }
}

// https://streams.spec.whatwg.org/#transform-stream-error
void TransformStreamError(JSContext* aCx, TransformStream* aStream,
                          JS::Handle<JS::Value> aError, ErrorResult& aRv) {
  // Step 1: Perform !
  // ReadableStreamDefaultControllerError(stream.[[readable]].[[controller]],
  // e).
  ReadableStreamDefaultControllerError(
      aCx, aStream->Readable()->Controller()->AsDefault(), aError, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Step 2: Perform ! TransformStreamErrorWritableAndUnblockWrite(stream, e).
  TransformStreamErrorWritableAndUnblockWrite(aCx, aStream, aError, aRv);
}

// https://streams.spec.whatwg.org/#transform-stream-default-controller-perform-transform
MOZ_CAN_RUN_SCRIPT static already_AddRefed<Promise>
TransformStreamDefaultControllerPerformTransform(
    JSContext* aCx, TransformStreamDefaultController* aController,
    JS::Handle<JS::Value> aChunk, ErrorResult& aRv) {
  // Step 1: Let transformPromise be the result of performing
  // controller.[[transformAlgorithm]], passing chunk.
  RefPtr<TransformerAlgorithmsBase> algorithms = aController->Algorithms();
  RefPtr<Promise> transformPromise =
      algorithms->TransformCallback(aCx, aChunk, *aController, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Step 2: Return the result of reacting to transformPromise with the
  // following rejection steps given the argument r:
  auto result = transformPromise->CatchWithCycleCollectedArgs(
      [](JSContext* aCx, JS::Handle<JS::Value> aError, ErrorResult& aRv,
         const RefPtr<TransformStreamDefaultController>& aController)
          MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA -> already_AddRefed<Promise> {
            // Step 2.1: Perform ! TransformStreamError(controller.[[stream]],
            // r).
            // TODO: Remove MOZ_KnownLive (bug 1761577)
            TransformStreamError(aCx, MOZ_KnownLive(aController->Stream()),
                                 aError, aRv);
            if (aRv.Failed()) {
              return nullptr;
            }

            // Step 2.2: Throw r.
            JS::Rooted<JS::Value> r(aCx, aError);
            aRv.MightThrowJSException();
            aRv.ThrowJSException(aCx, r);
            return nullptr;
          },
      RefPtr(aController));
  if (result.isErr()) {
    aRv.Throw(result.unwrapErr());
    return nullptr;
  }
  return result.unwrap().forget();
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

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> WriteCallback(
      JSContext* aCx, JS::Handle<JS::Value> aChunk,
      WritableStreamDefaultController& aController, ErrorResult& aRv) override {
    // Step 2. Let writeAlgorithm be the following steps, taking a chunk
    // argument:
    // Step 2.1. Return ! TransformStreamDefaultSinkWriteAlgorithm(stream,
    // chunk).

    // Inlining TransformStreamDefaultSinkWriteAlgorithm here:
    // https://streams.spec.whatwg.org/#transform-stream-default-sink-write-algorithm

    // Step 1: Assert: stream.[[writable]].[[state]] is "writable".
    MOZ_ASSERT(mStream->Writable()->State() ==
               WritableStream::WriterState::Writable);

    // Step 2: Let controller be stream.[[controller]].
    RefPtr<TransformStreamDefaultController> controller = mStream->Controller();

    // Step 3: If stream.[[backpressure]] is true,
    if (mStream->Backpressure()) {
      // Step 3.1: Let backpressureChangePromise be
      // stream.[[backpressureChangePromise]].
      RefPtr<Promise> backpressureChangePromise =
          mStream->BackpressureChangePromise();

      // Step 3.2: Assert: backpressureChangePromise is not undefined.
      MOZ_ASSERT(backpressureChangePromise);

      // Step 3.3: Return the result of reacting to backpressureChangePromise
      // with the following fulfillment steps:
      auto result = backpressureChangePromise->ThenWithCycleCollectedArgsJS(
          [](JSContext* aCx, JS::Handle<JS::Value>, ErrorResult& aRv,
             const RefPtr<TransformStream>& aStream,
             const RefPtr<TransformStreamDefaultController>& aController,
             JS::Handle<JS::Value> aChunk)
              MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA -> already_AddRefed<Promise> {
                // Step 1: Let writable be stream.[[writable]].
                RefPtr<WritableStream> writable = aStream->Writable();

                // Step 2: Let state be writable.[[state]].
                WritableStream::WriterState state = writable->State();

                // Step 3: If state is "erroring", throw
                // writable.[[storedError]].
                if (state == WritableStream::WriterState::Erroring) {
                  JS::Rooted<JS::Value> storedError(aCx,
                                                    writable->StoredError());
                  aRv.MightThrowJSException();
                  aRv.ThrowJSException(aCx, storedError);
                  return nullptr;
                }

                // Step 4: Assert: state is "writable".
                MOZ_ASSERT(state == WritableStream::WriterState::Writable);

                // Step 5: Return !
                // TransformStreamDefaultControllerPerformTransform(controller,
                // chunk).
                return TransformStreamDefaultControllerPerformTransform(
                    aCx, aController, aChunk, aRv);
              },
          std::make_tuple(mStream, controller), std::make_tuple(aChunk));

      if (result.isErr()) {
        aRv.Throw(result.unwrapErr());
        return nullptr;
      }
      return result.unwrap().forget();
    }

    // Step 4: Return !
    // TransformStreamDefaultControllerPerformTransform(controller, chunk).
    return TransformStreamDefaultControllerPerformTransform(aCx, controller,
                                                            aChunk, aRv);
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> AbortCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 3. Let abortAlgorithm be the following steps, taking a reason
    // argument:
    // Step 3.1. Return ! TransformStreamDefaultSinkAbortAlgorithm(stream,
    // reason).

    // Inlining TransformStreamDefaultSinkAbortAlgorithm here:
    // https://streams.spec.whatwg.org/#transform-stream-default-sink-abort-algorithm

    // Step 1:Perform ! TransformStreamError(stream, reason).
    TransformStreamError(
        aCx, mStream,
        aReason.WasPassed() ? aReason.Value() : JS::UndefinedHandleValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 2: Return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CloseCallback(
      JSContext* aCx, ErrorResult& aRv) override {
    // Step 4. Let closeAlgorithm be the following steps:
    // Step 4.1. Return ! TransformStreamDefaultSinkCloseAlgorithm(stream).

    // Inlining TransformStreamDefaultSinkCloseAlgorithm here:
    // https://streams.spec.whatwg.org/#transform-stream-default-sink-close-algorithm

    // Step 1: Let readable be stream.[[readable]].
    RefPtr<ReadableStream> readable = mStream->Readable();

    // Step 2: Let controller be stream.[[controller]].
    RefPtr<TransformStreamDefaultController> controller = mStream->Controller();

    // Step 3: Let flushPromise be the result of performing
    // controller.[[flushAlgorithm]].
    RefPtr<TransformerAlgorithmsBase> algorithms = controller->Algorithms();
    RefPtr<Promise> flushPromise =
        algorithms->FlushCallback(aCx, *controller, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 4: Perform !
    // TransformStreamDefaultControllerClearAlgorithms(controller).
    controller->SetAlgorithms(nullptr);

    // Step 5: Return the result of reacting to flushPromise:
    Result<RefPtr<Promise>, nsresult> result =
        flushPromise->ThenCatchWithCycleCollectedArgs(
            [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
               const RefPtr<ReadableStream>& aReadable,
               const RefPtr<TransformStream>& aStream)
                MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA
            -> already_AddRefed<Promise> {
                  // Step 5.1: If flushPromise was fulfilled, then:

                  // Step 5.1.1: If readable.[[state]] is "errored", throw
                  // readable.[[storedError]].
                  if (aReadable->State() ==
                      ReadableStream::ReaderState::Errored) {
                    JS::Rooted<JS::Value> storedError(aCx,
                                                      aReadable->StoredError());
                    aRv.MightThrowJSException();
                    aRv.ThrowJSException(aCx, storedError);
                    return nullptr;
                  }

                  // Step 5.1.2: Perform !
                  // ReadableStreamDefaultControllerClose(readable.[[controller]]).
                  ReadableStreamDefaultControllerClose(
                      aCx, MOZ_KnownLive(aReadable->Controller()->AsDefault()),
                      aRv);
                  return nullptr;
                },
            [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
               const RefPtr<ReadableStream>& aReadable,
               const RefPtr<TransformStream>& aStream)
                MOZ_CAN_RUN_SCRIPT_BOUNDARY_LAMBDA
            -> already_AddRefed<Promise> {
                  // Step 5.2: If flushPromise was rejected with reason r, then:

                  // Step 5.2.1: Perform ! TransformStreamError(stream, r).
                  TransformStreamError(aCx, aStream, aValue, aRv);
                  if (aRv.Failed()) {
                    return nullptr;
                  }

                  // Step 5.2.2: Throw readable.[[storedError]].
                  JS::Rooted<JS::Value> storedError(aCx,
                                                    aReadable->StoredError());
                  aRv.MightThrowJSException();
                  aRv.ThrowJSException(aCx, storedError);
                  return nullptr;
                },
            readable, mStream);

    if (result.isErr()) {
      aRv.Throw(result.unwrapErr());
      return nullptr;
    }
    return result.unwrap().forget();
  }

 protected:
  ~TransformStreamUnderlyingSinkAlgorithms() override = default;

 private:
  RefPtr<Promise> mStartPromise;
  // MOZ_KNOWN_LIVE because it won't be reassigned
  MOZ_KNOWN_LIVE RefPtr<TransformStream> mStream;
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
    // Step 6.1. Return ! TransformStreamDefaultSourcePullAlgorithm(stream).

    // Inlining TransformStreamDefaultSourcePullAlgorithm here:
    // https://streams.spec.whatwg.org/#transform-stream-default-source-pull-algorithm

    // Step 1: Assert: stream.[[backpressure]] is true.
    MOZ_ASSERT(mStream->Backpressure());

    // Step 2: Assert: stream.[[backpressureChangePromise]] is not undefined.
    MOZ_ASSERT(mStream->BackpressureChangePromise());

    // Step 3: Perform ! TransformStreamSetBackpressure(stream, false).
    mStream->SetBackpressure(false, aRv);

    // Step 4: Return stream.[[backpressureChangePromise]].
    return do_AddRef(mStream->BackpressureChangePromise());
  }

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> CancelCallback(
      JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
      ErrorResult& aRv) override {
    // Step 7. Let cancelAlgorithm be the following steps, taking a reason
    // argument:
    // Step 7.1. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    // reason).
    TransformStreamErrorWritableAndUnblockWrite(
        aCx, mStream,
        aReason.WasPassed() ? aReason.Value() : JS::UndefinedHandleValue, aRv);
    if (aRv.Failed()) {
      return nullptr;
    }

    // Step 7.2. Return a promise resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mStream->GetParentObject(),
                                                aRv);
  }

  void ErrorCallback() override {}

 protected:
  ~TransformStreamUnderlyingSourceAlgorithms() override = default;

 private:
  RefPtr<Promise> mStartPromise;
  // MOZ_KNOWNLIVE because it will never be reassigned
  MOZ_KNOWN_LIVE RefPtr<TransformStream> mStream;
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
void TransformStream::SetBackpressure(bool aBackpressure, ErrorResult& aRv) {
  // Step 1. Assert: stream.[[backpressure]] is not backpressure.
  MOZ_ASSERT(Backpressure() != aBackpressure);

  // Step 2. If stream.[[backpressureChangePromise]] is not undefined, resolve
  // stream.[[backpressureChangePromise]] with undefined.
  if (Promise* promise = BackpressureChangePromise()) {
    promise->MaybeResolveWithUndefined();
  }

  // Step 3. Set stream.[[backpressureChangePromise]] to a new promise.
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), aRv);
  if (aRv.Failed()) {
    return;
  }
  mBackpressureChangePromise = promise;

  // Step 4. Set stream.[[backpressure]] to backpressure.
  mBackpressure = aBackpressure;
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
  // Note(krosylight): The spec allows setting [[backpressure]] as undefined,
  // but I don't see why it should be. Since the spec also allows strict boolean
  // type, and this is only to not trigger assertion inside the setter, we just
  // set it as false.
  mBackpressure = false;
  mBackpressureChangePromise = nullptr;

  // Step 10. Perform ! TransformStreamSetBackpressure(stream, true).
  SetBackpressure(true, aRv);
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
  // also ReadableStream::Constructor for this design decision.
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

}  // namespace mozilla::dom
