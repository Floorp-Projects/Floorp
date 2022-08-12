/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStreamDefaultController.h"

#include "TransformerCallbackHelpers.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/TransformStream.h"
#include "mozilla/dom/TransformStreamDefaultControllerBinding.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransformStreamDefaultController, mGlobal,
                                      mStream, mTransformerAlgorithms)
NS_IMPL_CYCLE_COLLECTING_ADDREF(TransformStreamDefaultController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TransformStreamDefaultController)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransformStreamDefaultController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TransformStream* TransformStreamDefaultController::Stream() { return mStream; }

void TransformStreamDefaultController::SetStream(TransformStream& aStream) {
  MOZ_ASSERT(!mStream);
  mStream = &aStream;
}

TransformerAlgorithmsBase* TransformStreamDefaultController::Algorithms() {
  return mTransformerAlgorithms;
}

void TransformStreamDefaultController::SetAlgorithms(
    TransformerAlgorithmsBase* aTransformerAlgorithms) {
  mTransformerAlgorithms = aTransformerAlgorithms;
}

TransformStreamDefaultController::TransformStreamDefaultController(
    nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

TransformStreamDefaultController::~TransformStreamDefaultController() {
  mozilla::DropJSObjects(this);
}

JSObject* TransformStreamDefaultController::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TransformStreamDefaultController_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#ts-default-controller-desired-size
Nullable<double> TransformStreamDefaultController::GetDesiredSize() const {
  // Step 1. Let readableController be
  // this.[[stream]].[[readable]].[[controller]].
  RefPtr<ReadableStreamDefaultController> readableController =
      mStream->Readable()->Controller()->AsDefault();

  // Step 2. Return !
  // ReadableStreamDefaultControllerGetDesiredSize(readableController).
  return ReadableStreamDefaultControllerGetDesiredSize(readableController);
}

// https://streams.spec.whatwg.org/#rs-default-controller-has-backpressure
// Looks like a readable stream thing but the spec explicitly says this is for
// TransformStream.
static bool ReadableStreamDefaultControllerHasBackpressure(
    ReadableStreamDefaultController* aController) {
  // Step 1: If ! ReadableStreamDefaultControllerShouldCallPull(controller) is
  // true, return false.
  // Step 2: Otherwise, return true.
  return !ReadableStreamDefaultControllerShouldCallPull(aController);
}

void TransformStreamDefaultController::Enqueue(JSContext* aCx,
                                               JS::Handle<JS::Value> aChunk,
                                               ErrorResult& aRv) {
  // Step 1: Perform ? TransformStreamDefaultControllerEnqueue(this, chunk).

  // Inlining TransformStreamDefaultControllerEnqueue here.
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-enqueue

  // Step 1: Let stream be controller.[[stream]].
  RefPtr<TransformStream> stream = mStream;

  // Step 2: Let readableController be stream.[[readable]].[[controller]].
  RefPtr<ReadableStreamDefaultController> readableController =
      stream->Readable()->Controller()->AsDefault();

  // Step 3: If !
  // ReadableStreamDefaultControllerCanCloseOrEnqueue(readableController) is
  // false, throw a TypeError exception.
  if (!ReadableStreamDefaultControllerCanCloseOrEnqueueAndThrow(
          readableController, CloseOrEnqueue::Enqueue, aRv)) {
    return;
  }

  // Step 4: Let enqueueResult be
  // ReadableStreamDefaultControllerEnqueue(readableController, chunk).
  ErrorResult rv;
  ReadableStreamDefaultControllerEnqueue(aCx, readableController, aChunk, rv);

  // Step 5: If enqueueResult is an abrupt completion,
  if (rv.MaybeSetPendingException(aCx)) {
    JS::Rooted<JS::Value> error(aCx);
    if (!JS_GetPendingException(aCx, &error)) {
      // Uncatchable exception; we should mark aRv and return.
      aRv.StealExceptionFromJSContext(aCx);
      return;
    }
    JS_ClearPendingException(aCx);

    // Step 5.1: Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    // enqueueResult.[[Value]]).
    TransformStreamErrorWritableAndUnblockWrite(aCx, stream, error, aRv);

    // Step 5.2: Throw stream.[[readable]].[[storedError]].
    JS::Rooted<JS::Value> storedError(aCx, stream->Readable()->StoredError());
    aRv.MightThrowJSException();
    aRv.ThrowJSException(aCx, storedError);
    return;
  }

  // Step 6: Let backpressure be !
  // ReadableStreamDefaultControllerHasBackpressure(readableController).
  bool backpressure =
      ReadableStreamDefaultControllerHasBackpressure(readableController);

  // Step 7: If backpressure is not stream.[[backpressure]],
  if (backpressure != stream->Backpressure()) {
    // Step 7.1: Assert: backpressure is true.
    MOZ_ASSERT(backpressure);

    // Step 7.2: Perform ! TransformStreamSetBackpressure(true).
    stream->SetBackpressure(true, aRv);
  }
}

// https://streams.spec.whatwg.org/#ts-default-controller-error
void TransformStreamDefaultController::Error(JSContext* aCx,
                                             JS::Handle<JS::Value> aError,
                                             ErrorResult& aRv) {
  // Step 1: Perform ? TransformStreamDefaultControllerError(this, e).

  // Inlining TransformStreamDefaultControllerError here.
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-error

  // Perform ! TransformStreamError(controller.[[stream]], e).
  // mStream is set in initialization step and only modified in cycle
  // collection.
  // TODO: Move mStream initialization to a method/constructor and make it
  // MOZ_KNOWN_LIVE again. (See bug 1769854)
  TransformStreamError(aCx, MOZ_KnownLive(mStream), aError, aRv);
}

// https://streams.spec.whatwg.org/#ts-default-controller-terminate

void TransformStreamDefaultController::Terminate(JSContext* aCx,
                                                 ErrorResult& aRv) {
  // Step 1: Perform ? TransformStreamDefaultControllerTerminate(this).

  // Inlining TransformStreamDefaultControllerTerminate here.
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-terminate

  // Step 1: Let stream be controller.[[stream]].
  RefPtr<TransformStream> stream = mStream;

  // Step 2: Let readableController be stream.[[readable]].[[controller]].
  RefPtr<ReadableStreamDefaultController> readableController =
      stream->Readable()->Controller()->AsDefault();

  // Step 3: Perform ! ReadableStreamDefaultControllerClose(readableController).
  ReadableStreamDefaultControllerClose(aCx, readableController, aRv);

  // Step 4: Let error be a TypeError exception indicating that the stream has
  // been terminated.
  ErrorResult rv;
  rv.ThrowTypeError("Terminating the stream");
  JS::Rooted<JS::Value> error(aCx);
  MOZ_ALWAYS_TRUE(ToJSValue(aCx, std::move(rv), &error));

  // Step 5: Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
  // error).
  TransformStreamErrorWritableAndUnblockWrite(aCx, stream, error, aRv);
}

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller
void SetUpTransformStreamDefaultController(
    JSContext* aCx, TransformStream& aStream,
    TransformStreamDefaultController& aController,
    TransformerAlgorithmsBase& aTransformerAlgorithms) {
  // Step 1. Assert: stream implements TransformStream.
  // Step 2. Assert: stream.[[controller]] is undefined.
  MOZ_ASSERT(!aStream.Controller());

  // Step 3. Set controller.[[stream]] to stream.
  aController.SetStream(aStream);

  // Step 4. Set stream.[[controller]] to controller.
  aStream.SetController(aController);

  // Step 5. Set controller.[[transformAlgorithm]] to transformAlgorithm.
  // Step 6. Set controller.[[flushAlgorithm]] to flushAlgorithm.
  aController.SetAlgorithms(&aTransformerAlgorithms);
}

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
void SetUpTransformStreamDefaultControllerFromTransformer(
    JSContext* aCx, TransformStream& aStream,
    JS::Handle<JSObject*> aTransformer, Transformer& aTransformerDict) {
  // Step 1. Let controller be a new TransformStreamDefaultController.
  auto controller =
      MakeRefPtr<TransformStreamDefaultController>(aStream.GetParentObject());

  // Step 2 - 5:
  auto algorithms = MakeRefPtr<TransformerAlgorithms>(
      aStream.GetParentObject(), aTransformer, aTransformerDict);

  // Step 6: Perform ! SetUpTransformStreamDefaultController(stream, controller,
  // transformAlgorithm, flushAlgorithm).
  SetUpTransformStreamDefaultController(aCx, aStream, *controller, *algorithms);
}

}  // namespace mozilla::dom
