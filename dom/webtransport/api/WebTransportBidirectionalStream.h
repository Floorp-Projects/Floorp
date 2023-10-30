/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORTBIDIRECTIONALSTREAM__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORTBIDIRECTIONALSTREAM__H_

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ReadableStream.h"
#include "mozilla/dom/WebTransport.h"
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WebTransportSendReceiveStreamBinding.h"
#include "mozilla/ipc/DataPipe.h"

// #include "mozilla/dom/WebTransportReceiveStream.h"
// #include "mozilla/dom/WebTransportSendStream.h"

namespace mozilla::dom {
class WebTransportBidirectionalStream final : public nsISupports,
                                              public nsWrapperCache {
 public:
  explicit WebTransportBidirectionalStream(nsIGlobalObject* aGlobal,
                                           WebTransportReceiveStream* aReadable,
                                           WebTransportSendStream* aWritable)
      : mGlobal(aGlobal), mReadable(aReadable), mWritable(aWritable) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransportBidirectionalStream)

  static already_AddRefed<WebTransportBidirectionalStream> Create(
      WebTransport* aWebTransport, nsIGlobalObject* aGlobal, uint64_t aStreamId,
      ::mozilla::ipc::DataPipeReceiver* receiver,
      ::mozilla::ipc::DataPipeSender* aSender, Maybe<int64_t> aSendOrder,
      ErrorResult& aRv);

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  already_AddRefed<WebTransportReceiveStream> Readable() const {
    return do_AddRef(mReadable);
  }
  already_AddRefed<WebTransportSendStream> Writable() const {
    return do_AddRef(mWritable);
  }

 private:
  ~WebTransportBidirectionalStream() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WebTransportReceiveStream> mReadable;
  RefPtr<WebTransportSendStream> mWritable;
};

}  // namespace mozilla::dom

#endif
