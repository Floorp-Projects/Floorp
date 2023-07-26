/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTRECEIVESTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTRECEIVESTREAM__H_

#include "mozilla/dom/ReadableStream.h"

namespace mozilla::ipc {
class DataPipeReceiver;
}

namespace mozilla::dom {

class WebTransport;

class WebTransportReceiveStream final : public ReadableStream {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(WebTransportReceiveStream,
                                           ReadableStream)

  WebTransportReceiveStream(nsIGlobalObject* aGlobal, WebTransport* aTransport);

  static already_AddRefed<WebTransportReceiveStream> Create(
      WebTransport* aWebTransport, nsIGlobalObject* aGlobal, uint64_t aStreamId,
      mozilla::ipc::DataPipeReceiver* receiver, ErrorResult& aRv);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> GetStats();

 private:
  ~WebTransportReceiveStream() override { mozilla::DropJSObjects(this); }

  // We must hold a reference to the WebTransport so it can't go away on
  // us.  This forms a cycle with WebTransport that will be broken when the
  // CC runs.   WebTransport::CleanUp() will destroy all the send and receive
  // streams, breaking the cycle.
  RefPtr<WebTransport> mTransport;
};
}  // namespace mozilla::dom

#endif
