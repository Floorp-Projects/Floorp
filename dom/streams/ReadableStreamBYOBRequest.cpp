/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamBYOBRequest.h"

#include "mozilla/dom/ByteStreamHelpers.h"
#include "js/ArrayBuffer.h"
#include "js/TypeDecls.h"
#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/ReadableStreamBYOBRequestBinding.h"
#include "js/experimental/TypedData.h"
#include "mozilla/dom/ReadableStreamController.h"
#include "nsCOMPtr.h"
#include "nsIGlobalObject.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

using namespace streams_abstract;

ReadableStreamBYOBRequest::ReadableStreamBYOBRequest(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  mozilla::HoldJSObjects(this);
}

ReadableStreamBYOBRequest::~ReadableStreamBYOBRequest() {
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_WITH_JS_MEMBERS(ReadableStreamBYOBRequest,
                                                      (mGlobal, mController),
                                                      (mView))

NS_IMPL_CYCLE_COLLECTING_ADDREF(ReadableStreamBYOBRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ReadableStreamBYOBRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ReadableStreamBYOBRequest)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* ReadableStreamBYOBRequest::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return ReadableStreamBYOBRequest_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#rs-byob-request-view
void ReadableStreamBYOBRequest::GetView(
    JSContext* cx, JS::MutableHandle<JSObject*> aRetVal) const {
  // Step 1.
  aRetVal.set(mView);
}

// https://streams.spec.whatwg.org/#rs-byob-request-respond
void ReadableStreamBYOBRequest::Respond(JSContext* aCx, uint64_t bytesWritten,
                                        ErrorResult& aRv) {
  // Step 1.
  if (!mController) {
    aRv.ThrowTypeError("Undefined Controller");
    return;
  }

  // Step 2.
  bool isSharedMemory;
  JS::Rooted<JSObject*> view(aCx, mView);
  JS::Rooted<JSObject*> arrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, view, &isSharedMemory));
  if (!arrayBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  if (JS::IsDetachedArrayBufferObject(arrayBuffer)) {
    aRv.ThrowTypeError("View of Detached buffer");
    return;
  }

  // Step 3.
  MOZ_ASSERT(JS_GetArrayBufferViewByteLength(view) > 0);

  // Step 4.
  MOZ_ASSERT(JS::GetArrayBufferByteLength(arrayBuffer) > 0);

  // Step 5.
  RefPtr<ReadableByteStreamController> controller(mController);
  ReadableByteStreamControllerRespond(aCx, controller, bytesWritten, aRv);
}

// https://streams.spec.whatwg.org/#rs-byob-request-respond-with-new-view
void ReadableStreamBYOBRequest::RespondWithNewView(JSContext* aCx,
                                                   const ArrayBufferView& view,
                                                   ErrorResult& aRv) {
  // Step 1.
  if (!mController) {
    aRv.ThrowTypeError("Undefined Controller");
    return;
  }

  // Step 2.
  bool isSharedMemory;
  JS::Rooted<JSObject*> rootedViewObj(aCx, view.Obj());
  JS::Rooted<JSObject*> viewedArrayBuffer(
      aCx, JS_GetArrayBufferViewBuffer(aCx, rootedViewObj, &isSharedMemory));
  if (!viewedArrayBuffer) {
    aRv.StealExceptionFromJSContext(aCx);
    return;
  }

  if (JS::IsDetachedArrayBufferObject(viewedArrayBuffer)) {
    aRv.ThrowTypeError("View of Detached Array Buffer");
    return;
  }

  // Step 3.
  RefPtr<ReadableByteStreamController> controller(mController);
  ReadableByteStreamControllerRespondWithNewView(aCx, controller, rootedViewObj,
                                                 aRv);
}

void ReadableStreamBYOBRequest::SetController(
    ReadableByteStreamController* aController) {
  mController = aController;
}

}  // namespace mozilla::dom
