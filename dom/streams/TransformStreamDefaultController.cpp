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

void TransformStreamDefaultController::SetStream(TransformStream* aStream) {
  mStream = aStream;
}

void TransformStreamDefaultController::SetAlgorithms(
    TransformerAlgorithms* aTransformerAlgorithms) {
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
    TransformerAlgorithms& aTransformerAlgorithms) {
  // Step 1. Assert: stream implements TransformStream.
  // Step 2. Assert: stream.[[controller]] is undefined.
  MOZ_ASSERT(!aStream.Controller());

  // Step 3. Set controller.[[stream]] to stream.
  aController.SetStream(&aStream);

  // Step 4. Set stream.[[controller]] to controller.
  aStream.SetController(&aController);

  // Step 5. Set controller.[[transformAlgorithm]] to transformAlgorithm.
  // Step 6. Set controller.[[flushAlgorithm]] to flushAlgorithm.
  aController.SetAlgorithms(&aTransformerAlgorithms);
}

// https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
void SetUpTransformStreamDefaultControllerFromTransformer(
    JSContext* aCx, TransformStream& aStream, JS::HandleObject aTransformer,
    Transformer& aTransformerDict) {
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
