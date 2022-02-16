/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStreamDefaultController.h"

#include "mozilla/dom/TransformStream.h"
#include "mozilla/dom/TransformStreamDefaultControllerBinding.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransformStreamDefaultController, mGlobal)
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

}  // namespace mozilla::dom
