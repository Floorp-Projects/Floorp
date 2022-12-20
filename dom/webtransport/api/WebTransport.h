/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_
#define DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/WebTransportBinding.h"
#include "mozilla/dom/WebTransportChild.h"

namespace mozilla::dom {

class WebTransportDatagramDuplexStream;
class ReadableStream;
class WritableStream;

class WebTransport final : public nsISupports, public nsWrapperCache {
 public:
  explicit WebTransport(nsIGlobalObject* aGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransport)

  enum class WebTransportState { CONNECTING, CONNECTED, CLOSED, FAILED };

  bool Init(const GlobalObject& aGlobal, const nsAString& aUrl,
            const WebTransportOptions& aOptions, ErrorResult& aError);
  void ResolveWaitingConnection(WebTransportChild* aChild);
  void RejectWaitingConnection(nsresult aRv);
  bool ParseURL(const nsAString& aURL) const;

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  static already_AddRefed<WebTransport> Constructor(
      const GlobalObject& aGlobal, const nsAString& aUrl,
      const WebTransportOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<Promise> GetStats(ErrorResult& aError);

  already_AddRefed<Promise> Ready();
  WebTransportReliabilityMode Reliability();
  WebTransportCongestionControl CongestionControl();
  already_AddRefed<Promise> Closed();
  void Close(const WebTransportCloseInfo& aOptions);
  already_AddRefed<WebTransportDatagramDuplexStream> Datagrams();
  already_AddRefed<Promise> CreateBidirectionalStream(ErrorResult& aError);
  already_AddRefed<ReadableStream> IncomingBidirectionalStreams();
  already_AddRefed<Promise> CreateUnidirectionalStream(ErrorResult& aError);
  already_AddRefed<ReadableStream> IncomingUnidirectionalStreams();

  void Shutdown() {}

 private:
  ~WebTransport() {
    // If this WebTransport was destroyed without being closed properly, make
    // sure to clean up the channel.
    if (mChild) {
      mChild->Shutdown();
    }
  }

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WebTransportChild> mChild;

  // These are created in the constructor
  RefPtr<ReadableStream> mIncomingUnidirectionalStreams;
  RefPtr<ReadableStream> mIncomingBidirectionalStreams;
  RefPtr<Promise> mReady;
  WebTransportState mState;
};

}  // namespace mozilla::dom

#endif
