/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStreamDefaultController.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/TransformStream.h"
#include "mozilla/dom/TransformStreamDefaultControllerBinding.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(
    TransformStreamDefaultController, (mGlobal, mTransformCallback),
    (mTransformer))
NS_IMPL_CYCLE_COLLECTING_ADDREF(TransformStreamDefaultController)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TransformStreamDefaultController)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TransformStreamDefaultController)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

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

Nullable<double> TransformStreamDefaultController::GetDesiredSize() const {
  // Step 1. Let readableController be
  // this.[[stream]].[[readable]].[[controller]].
  // TODO

  // Step 2. Return !
  // ReadableStreamDefaultControllerGetDesiredSize(readableController).
  // TODO
  return 0;
}

void TransformStreamDefaultController::Enqueue(JSContext* aCx,
                                               JS::Handle<JS::Value> aChunk,
                                               ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TransformStreamDefaultController::Error(JSContext* aCx,
                                             JS::Handle<JS::Value> aError,
                                             ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

void TransformStreamDefaultController::Terminate(ErrorResult& aRv) {
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller
void SetUpTransformStreamDefaultController(
    JSContext* aCx, TransformStream& aStream,
    TransformStreamDefaultController& aController,
    TransformStreamDefaultController::TransformAlgorithm aTransformAlgorithm) {
  // Step 1. Assert: stream implements TransformStream.
  // Step 2. Assert: stream.[[controller]] is undefined.
  MOZ_ASSERT(!aStream.Controller());

  // Step 3. Set controller.[[stream]] to stream.
  // TODO

  // Step 4. Set stream.[[controller]] to controller.
  aStream.SetController(&aController);

  // Step 5. Set controller.[[transformAlgorithm]] to transformAlgorithm.
  aController.SetTransformAlgorithm(aTransformAlgorithm);

  // Step 6. Set controller.[[flushAlgorithm]] to flushAlgorithm.
  // TODO
}

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
void SetUpTransformStreamDefaultControllerFromTransformer(
    JSContext* aCx, TransformStream& aStream, JS::HandleObject aTransformer,
    Transformer& aTransformerDict) {
  // Step 1. Let controller be a new TransformStreamDefaultController.
  RefPtr<TransformStreamDefaultController> controller =
      new TransformStreamDefaultController(aStream.GetParentObject());

  TransformStreamDefaultController::TransformAlgorithm transformAlgorithm;
  if (!aTransformerDict.mTransform.WasPassed()) {
    // Step 2. Let transformAlgorithm be the following steps, taking a chunk
    // argument:
    transformAlgorithm = [](JSContext* aCx,
                            TransformStreamDefaultController& aController,
                            JS::HandleValue aChunk,
                            ErrorResult& aRv) -> already_AddRefed<Promise> {
      MOZ_ASSERT(!aController.GetTransformCallback());
      MOZ_ASSERT(!aController.GetTransformer());

      // Step 2.1. Let result be
      // TransformStreamDefaultControllerEnqueue(controller, chunk).
      // TODO

      // Step 2.2. If result is an abrupt completion, return a promise rejected
      // with result.[[Value]].
      // TODO

      // Step 2.3. Otherwise, return a promise resolved with undefined.
      return Promise::CreateResolvedWithUndefined(aController.GetParentObject(),
                                                  aRv);
    };
  } else {
    // Step 4. If transformerDict["transform"] exists, set transformAlgorithm to
    // an algorithm which takes an argument chunk and returns the result of
    // invoking transformerDict["transform"] with argument list « chunk,
    // controller » and callback this value transformer.
    controller->SetTransformerMembers(aTransformerDict.mTransform.Value(),
                                      aTransformer);
    transformAlgorithm =
        [](JSContext* aCx, TransformStreamDefaultController& aController,
           JS::HandleValue aChunk, ErrorResult& aRv)
            MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION -> already_AddRefed<Promise> {
      MOZ_ASSERT(aController.GetTransformCallback());
      MOZ_ASSERT(aController.GetTransformer());
      JS::RootedObject thisObj(aCx, aController.GetTransformer());
      RefPtr<TransformerTransformCallback> callback =
          aController.GetTransformCallback();
      return callback->Call(
          thisObj, aChunk, aController, aRv,
          "TransformStreamDefaultController.[[transformAlgorithm]]",
          CallbackObject::eRethrowExceptions);
    };
  }

  // Step 3. Let flushAlgorithm be an algorithm which returns a promise
  // resolved with undefined.
  // TODO

  // Step 5. If transformerDict["flush"] exists, set flushAlgorithm to an
  // algorithm which returns the result of invoking transformerDict["flush"]
  // with argument list « controller » and callback this value transformer.
  // TODO

  // Perform ! SetUpTransformStreamDefaultController(stream, controller,
  // transformAlgorithm, flushAlgorithm).
  SetUpTransformStreamDefaultController(aCx, aStream, *controller,
                                        transformAlgorithm);
}

}  // namespace mozilla::dom
