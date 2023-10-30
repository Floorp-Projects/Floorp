/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_
#define DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_

#include "ErrorList.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/PWebTransportParent.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsISupports.h"
#include "nsIPrincipal.h"
#include "nsIWebTransport.h"
#include "nsIWebTransportStream.h"
#include "nsTHashMap.h"

namespace mozilla::dom {

enum class WebTransportReliabilityMode : uint8_t;

class WebTransportParent : public PWebTransportParent,
                           public WebTransportSessionEventListener {
  using IPCResult = mozilla::ipc::IPCResult;

 public:
  WebTransportParent() = default;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_WEBTRANSPORTSESSIONEVENTLISTENER

  void Create(const nsAString& aURL, nsIPrincipal* aPrincipal,
              const mozilla::Maybe<IPCClientInfo>& aClientInfo,
              const bool& aDedicated, const bool& aRequireUnreliable,
              const uint32_t& aCongestionControl,
              // Sequence<WebTransportHash>* aServerCertHashes,
              Endpoint<PWebTransportParent>&& aParentEndpoint,
              std::function<void(std::tuple<const nsresult&, const uint8_t&>)>&&
                  aResolver);

  IPCResult RecvClose(const uint32_t& aCode, const nsACString& aReason);

  IPCResult RecvSetSendOrder(uint64_t aStreamId, Maybe<int64_t> aSendOrder);

  IPCResult RecvCreateUnidirectionalStream(
      Maybe<int64_t> aSendOrder,
      CreateUnidirectionalStreamResolver&& aResolver);
  IPCResult RecvCreateBidirectionalStream(
      Maybe<int64_t> aSendOrder, CreateBidirectionalStreamResolver&& aResolver);

  ::mozilla::ipc::IPCResult RecvOutgoingDatagram(
      nsTArray<uint8_t>&& aData, const TimeStamp& aExpirationTime,
      OutgoingDatagramResolver&& aResolver);

  ::mozilla::ipc::IPCResult RecvGetMaxDatagramSize(
      GetMaxDatagramSizeResolver&& aResolver);

  void ActorDestroy(ActorDestroyReason aWhy) override;

  class OnResetOrStopSendingCallback final {
   public:
    explicit OnResetOrStopSendingCallback(
        std::function<void(nsresult)>&& aCallback)
        : mCallback(std::move(aCallback)) {}
    ~OnResetOrStopSendingCallback() = default;

    void OnResetOrStopSending(nsresult aError) { mCallback(aError); }

   private:
    std::function<void(nsresult)> mCallback;
  };

 protected:
  virtual ~WebTransportParent();

 private:
  void NotifyRemoteClosed(bool aCleanly, uint32_t aErrorCode,
                          const nsACString& aReason);

  using ResolveType = std::tuple<const nsresult&, const uint8_t&>;
  nsCOMPtr<nsISerialEventTarget> mSocketThread;
  Atomic<bool> mSessionReady{false};

  mozilla::Mutex mMutex{"WebTransportParent::mMutex"};
  std::function<void(ResolveType)> mResolver MOZ_GUARDED_BY(mMutex);
  // This is needed because mResolver is resolved on the background thread and
  // OnSessionClosed is called on the socket thread.
  std::function<void()> mExecuteAfterResolverCallback MOZ_GUARDED_BY(mMutex);
  OutgoingDatagramResolver mOutgoingDatagramResolver;
  GetMaxDatagramSizeResolver mMaxDatagramSizeResolver;
  FlippedOnce<false> mClosed MOZ_GUARDED_BY(mMutex);

  nsCOMPtr<nsIWebTransport> mWebTransport;
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;

  // What we need to be able to lookup by streamId
  template <typename T>
  struct StreamHash {
    OnResetOrStopSendingCallback mCallback;
    nsCOMPtr<T> mStream;
  };
  nsTHashMap<nsUint64HashKey, StreamHash<nsIWebTransportBidirectionalStream>>
      mBidiStreamCallbackMap;
  nsTHashMap<nsUint64HashKey, StreamHash<nsIWebTransportSendStream>>
      mUniStreamCallbackMap;
};

}  // namespace mozilla::dom

#endif  // DOM_WEBTRANSPORT_PARENT_WEBTRANSPORTPARENT_H_
