/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TransformStream.h"

#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/TransformStreamBinding.h"
#include "mozilla/dom/TransformerBinding.h"
#include "mozilla/dom/StreamUtils.h"
#include "nsWrapperCache.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TransformStream, mGlobal)
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
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<TransformStream> transformStream = new TransformStream(global);
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
