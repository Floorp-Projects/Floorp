/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebTransportBidirectionalStream.h"
#include "mozilla/dom/Promise.h"

namespace mozilla::dom {

using namespace mozilla::ipc;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(WebTransportBidirectionalStream, mGlobal,
                                      mReadable, mWritable)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WebTransportBidirectionalStream)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WebTransportBidirectionalStream)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WebTransportBidirectionalStream)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// WebIDL Boilerplate

nsIGlobalObject* WebTransportBidirectionalStream::GetParentObject() const {
  return mGlobal;
}

JSObject* WebTransportBidirectionalStream::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return WebTransportBidirectionalStream_Binding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<WebTransportBidirectionalStream>
WebTransportBidirectionalStream::Create(
    WebTransport* aWebTransport, nsIGlobalObject* aGlobal, uint64_t aStreamId,
    DataPipeReceiver* receiver, DataPipeSender* aSender,
    Maybe<int64_t> aSendOrder, ErrorResult& aRv) {
  // https://w3c.github.io/webtransport/#pullbidirectionalstream (and
  // createBidirectionalStream)

  // Step 7.1: Let stream be the result of creating a
  // WebTransportBidirectionalStream with internalStream and transport
  RefPtr<WebTransportReceiveStream> readableStream =
      WebTransportReceiveStream::Create(aWebTransport, aGlobal, aStreamId,
                                        receiver, aRv);
  if (!readableStream) {
    return nullptr;
  }
  RefPtr<WebTransportSendStream> writableStream =
      WebTransportSendStream::Create(aWebTransport, aGlobal, aStreamId, aSender,
                                     aSendOrder, aRv);
  if (!writableStream) {
    return nullptr;
    ;
  }

  auto stream = MakeRefPtr<WebTransportBidirectionalStream>(
      aGlobal, readableStream, writableStream);
  return stream.forget();
}

// WebIDL Interface

}  // namespace mozilla::dom
