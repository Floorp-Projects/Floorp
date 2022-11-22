/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransport.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTransport, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTransport)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTransport)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransport)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// WebIDL Boilerplate

nsIGlobalObject* WebTransport::GetParentObject() const { return mGlobal; }

JSObject* WebTransport::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return WebTransport_Binding::Wrap(aCx, this, aGivenProto);
}

// WebIDL Interface

/* static */
already_AddRefed<WebTransport> WebTransport::Constructor(
    const GlobalObject& aGlobal, const nsAString& aUrl,
    const WebTransportOptions& aOptions) {
  return nullptr;
}

already_AddRefed<Promise> WebTransport::GetStats(ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<Promise> WebTransport::Ready() {
  ErrorResult error;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), error);
  if (error.Failed()) {
    return nullptr;
  }
  promise->MaybeRejectWithUndefined();
  return promise.forget();
}

WebTransportReliabilityMode WebTransport::Reliability() {
  // XXX not implemented
  return WebTransportReliabilityMode::Pending;
}

WebTransportCongestionControl WebTransport::CongestionControl() {
  // XXX not implemented
  return WebTransportCongestionControl::Default;
}

already_AddRefed<Promise> WebTransport::Closed() {
  ErrorResult error;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), error);
  if (error.Failed()) {
    return nullptr;
  }
  promise->MaybeRejectWithUndefined();
  return promise.forget();
}

void WebTransport::Close(const WebTransportCloseInfo& aOptions) {}

already_AddRefed<WebTransportDatagramDuplexStream> WebTransport::Datagrams() {
  // XXX not implemented
  return nullptr;
}

already_AddRefed<Promise> WebTransport::CreateBidirectionalStream(
    ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<ReadableStream> WebTransport::IncomingBidirectionalStreams() {
  // XXX not implemented
  return nullptr;
}

already_AddRefed<Promise> WebTransport::CreateUnidirectionalStream(
    ErrorResult& aError) {
  aError.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

already_AddRefed<ReadableStream> WebTransport::IncomingUnidirectionalStreams() {
  // XXX not implemented
  return nullptr;
}

}  // namespace mozilla::dom
