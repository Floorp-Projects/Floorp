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
#include "mozilla/dom/WritableStream.h"
#include "mozilla/dom/WebTransportSendReceiveStreamBinding.h"

// #include "mozilla/dom/WebTransportReceiveStream.h"
// #include "mozilla/dom/WebTransportSendStream.h"

namespace mozilla::dom {
class WebTransportBidirectionalStream final : public nsISupports,
                                              public nsWrapperCache {
 public:
  explicit WebTransportBidirectionalStream(nsIGlobalObject* aGlobal)
      : mGlobal(aGlobal) {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransportBidirectionalStream)

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  // XXX spec says these should be WebTransportReceiveStream and
  // WebTransportSendStream
  // XXX Not implemented
  already_AddRefed<ReadableStream> Readable() { return nullptr; }
  already_AddRefed<WritableStream> Writable() { return nullptr; }

 private:
  ~WebTransportBidirectionalStream() = default;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace mozilla::dom

#endif
