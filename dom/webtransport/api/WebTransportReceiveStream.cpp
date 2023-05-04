/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebTransportReceiveStream.h"

#include "mozilla/dom/ReadableByteStreamController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WebTransportSendReceiveStreamBinding.h"
#include "mozilla/ipc/DataPipe.h"

using namespace mozilla::ipc;

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(WebTransportReceiveStream, ReadableStream,
                                   mTransport)
NS_IMPL_ADDREF_INHERITED(WebTransportReceiveStream, ReadableStream)
NS_IMPL_RELEASE_INHERITED(WebTransportReceiveStream, ReadableStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransportReceiveStream)
NS_INTERFACE_MAP_END_INHERITING(ReadableStream)

WebTransportReceiveStream::WebTransportReceiveStream(nsIGlobalObject* aGlobal,
                                                     WebTransport* aTransport)
    : ReadableStream(aGlobal,
                     ReadableStream::HoldDropJSObjectsCaller::Explicit),
      mTransport(aTransport) {
  mozilla::HoldJSObjects(this);
}

// WebIDL Boilerplate

JSObject* WebTransportReceiveStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WebTransportReceiveStream_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<WebTransportReceiveStream> WebTransportReceiveStream::Create(
    WebTransport* aWebTransport, nsIGlobalObject* aGlobal, uint64_t aStreamId,
    DataPipeReceiver* receiver, ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#webtransportreceivestream-create
  AutoJSAPI jsapi;
  if (!jsapi.Init(aGlobal)) {
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  auto stream = MakeRefPtr<WebTransportReceiveStream>(aGlobal, aWebTransport);

  nsCOMPtr<nsIAsyncInputStream> inputStream = receiver;
  auto algorithms = MakeRefPtr<InputToReadableStreamAlgorithms>(
      cx, inputStream, (ReadableStream*)stream);

  stream->SetUpByteNative(cx, *algorithms, Some(0.0), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  // Add to ReceiveStreams
  aWebTransport->mReceiveStreams.InsertOrUpdate(aStreamId, stream);
  return stream.forget();
}

already_AddRefed<Promise> WebTransportReceiveStream::GetStats() {
  RefPtr<Promise> promise = Promise::CreateInfallible(ReadableStream::mGlobal);
  promise->MaybeRejectWithNotSupportedError("GetStats isn't supported yet");
  return promise.forget();
}

}  // namespace mozilla::dom
