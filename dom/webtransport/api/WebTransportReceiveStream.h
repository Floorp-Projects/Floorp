/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTRECEIVESTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTRECEIVESTREAM__H_

#include "mozilla/dom/ReadableStream.h"

namespace mozilla::dom {

class WebTransportReceiveStream final : public ReadableStream {
 public:
  NS_INLINE_DECL_REFCOUNTING_INHERITED(WebTransportReceiveStream,
                                       ReadableStream)

  explicit WebTransportReceiveStream(nsIGlobalObject* aGlobal);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY static already_AddRefed<WebTransportReceiveStream>
  Create(WebTransport* aWebTransport, nsIGlobalObject* aGlobal,
         mozilla::ipc::DataPipeReceiver* receiver, ErrorResult& aRv);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<Promise> GetStats();

 private:
  ~WebTransportReceiveStream() override = default;
};
}  // namespace mozilla::dom

#endif
