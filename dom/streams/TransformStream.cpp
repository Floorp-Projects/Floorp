/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStream.h"

#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TransformStreamBinding.h"
#include "mozilla/dom/TransformerBinding.h"
#include "mozilla/dom/StreamUtils.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransformStream, mGlobal, mController)
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
  // TODO

  // Step 6. Let readableSizeAlgorithm be !
  // ExtractSizeAlgorithm(readableStrategy).
  // TODO

  // Step 7. Let writableHighWaterMark be ?
  // ExtractHighWaterMark(writableStrategy, 1).
  // TODO

  // Step 8. Let writableSizeAlgorithm be !
  // ExtractSizeAlgorithm(writableStrategy).
  // TODO

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
  // TODO: Init()
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
  // TODO

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
