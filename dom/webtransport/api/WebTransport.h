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

class WebTransportError;
class WebTransportDatagramDuplexStream;
class WebTransportIncomingStreamsAlgorithms;
class ReadableStream;
class WritableStream;

class WebTransport final : public nsISupports, public nsWrapperCache {
  friend class WebTransportIncomingStreamsAlgorithms;

 public:
  explicit WebTransport(nsIGlobalObject* aGlobal);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebTransport)

  enum class WebTransportState { CONNECTING, CONNECTED, CLOSED, FAILED };

  // this calls CreateReadableStream(), which in this case doesn't actually run
  // script.   See also bug 1810942
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Init(const GlobalObject& aGlobal,
                                        const nsAString& aUrl,
                                        const WebTransportOptions& aOptions,
                                        ErrorResult& aError);
  void ResolveWaitingConnection(WebTransportReliabilityMode aReliability,
                                WebTransportChild* aChild);
  void RejectWaitingConnection(nsresult aRv);
  bool ParseURL(const nsAString& aURL) const;
  // this calls CloseNative(), which doesn't actually run script.   See bug
  // 1810942
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Cleanup(
      WebTransportError* aError, const WebTransportCloseInfo* aCloseInfo,
      ErrorResult& aRv);

  // WebIDL Boilerplate
  nsIGlobalObject* GetParentObject() const;

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Interface
  static already_AddRefed<WebTransport> Constructor(
      const GlobalObject& aGlobal, const nsAString& aUrl,
      const WebTransportOptions& aOptions, ErrorResult& aError);

  already_AddRefed<Promise> GetStats(ErrorResult& aError);

  already_AddRefed<Promise> Ready() { return do_AddRef(mReady); }
  WebTransportReliabilityMode Reliability();
  WebTransportCongestionControl CongestionControl();
  already_AddRefed<Promise> Closed();
  void Close(const WebTransportCloseInfo& aOptions);
  already_AddRefed<WebTransportDatagramDuplexStream> Datagrams();
  already_AddRefed<Promise> CreateBidirectionalStream(ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<ReadableStream>
  IncomingBidirectionalStreams();
  already_AddRefed<Promise> CreateUnidirectionalStream(ErrorResult& aError);
  MOZ_CAN_RUN_SCRIPT_BOUNDARY already_AddRefed<ReadableStream>
  IncomingUnidirectionalStreams();

  void Shutdown() {}

 private:
  ~WebTransport();

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WebTransportChild> mChild;

  // Spec in 5.8 says it can't be GC'd while CONNECTING or CONNECTED.  We won't
  // hold ref which we drop on CLOSED or FAILED because a reference is also held
  // by IPC.  We drop the IPC connection and by proxy the reference when it goes
  // to FAILED or CLOSED.

  // Spec-defined slots:
  // ordered sets, but we can't have duplicates, and this spec only appends.
  // Order is visible due to
  // https://w3c.github.io/webtransport/#webtransport-procedures step 10: "For
  // each sendStream in sendStreams, error sendStream with error."
  // XXX Use nsTArray.h for now, but switch to OrderHashSet/Table for release to
  // improve remove performance (if needed)
  nsTArray<RefPtr<WritableStream>> mSendStreams;
  nsTArray<RefPtr<ReadableStream>> mReceiveStreams;

  WebTransportState mState;
  RefPtr<Promise> mReady;
  RefPtr<Promise> mIncomingUnidirectionalPromise;
  RefPtr<Promise> mIncomingBidirectionalPromise;
  WebTransportReliabilityMode mReliability;
  // These are created in the constructor
  RefPtr<ReadableStream> mIncomingUnidirectionalStreams;
  RefPtr<ReadableStream> mIncomingBidirectionalStreams;
  RefPtr<WebTransportDatagramDuplexStream> mDatagrams;
  RefPtr<Promise> mClosed;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_API_WEBTRANSPORT__H_
