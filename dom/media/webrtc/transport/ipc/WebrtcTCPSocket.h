/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef webrtc_tcp_socket_h__
#define webrtc_tcp_socket_h__

#include <list>

#include "nsCOMPtr.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "nsIAuthPromptProvider.h"
#include "nsIHttpChannelInternal.h"
#include "nsIInterfaceRequestor.h"
#include "nsIStreamListener.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include "nsIProtocolProxyCallback.h"
#include "mozilla/net/WebrtcProxyConfig.h"

class nsISocketTransport;

namespace mozilla {
namespace net {

class WebrtcTCPSocketCallback;
class WebrtcTCPData;

class WebrtcTCPSocket : public nsIHttpUpgradeListener,
                        public nsIStreamListener,
                        public nsIInputStreamCallback,
                        public nsIOutputStreamCallback,
                        public nsIInterfaceRequestor,
                        public nsIAuthPromptProvider,
                        public nsIProtocolProxyCallback {
 public:
  NS_DECL_NSIHTTPUPGRADELISTENER
  NS_DECL_NSIINPUTSTREAMCALLBACK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_FORWARD_SAFE_NSIAUTHPROMPTPROVIDER(mAuthProvider)
  NS_DECL_NSIPROTOCOLPROXYCALLBACK

  explicit WebrtcTCPSocket(WebrtcTCPSocketCallback* aCallbacks);

  void SetTabId(dom::TabId aTabId);
  nsresult Open(const nsCString& aHost, const int& aPort,
                const nsCString& aLocalAddress, const int& aLocalPort,
                bool aUseTls,
                const Maybe<net::WebrtcProxyConfig>& aProxyConfig);
  nsresult Write(nsTArray<uint8_t>&& aBytes);
  nsresult Close();

  size_t CountUnwrittenBytes() const;

  nsresult OnWebSocketConnectionAvailable(
      WebSocketConnectionBase* aConnection) override {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

 protected:
  virtual ~WebrtcTCPSocket();

  // protected for gtests
  virtual void InvokeOnClose(nsresult aReason);
  virtual void InvokeOnConnected();
  virtual void InvokeOnRead(nsTArray<uint8_t>&& aReadData);

  RefPtr<WebrtcTCPSocketCallback> mProxyCallbacks;

 private:
  bool mClosed;
  bool mOpened;
  nsCOMPtr<nsIURI> mURI;
  bool mTls = false;
  Maybe<WebrtcProxyConfig> mProxyConfig;
  nsCString mLocalAddress;
  uint16_t mLocalPort = 0;
  nsCString mProxyType;

  nsresult DoProxyConfigLookup();
  nsresult OpenWithHttpProxy();
  void OpenWithoutHttpProxy(nsIProxyInfo* aSocksProxyInfo);
  void FinishOpen();
  void EnqueueWrite_s(nsTArray<uint8_t>&& aWriteData);

  void CloseWithReason(nsresult aReason);

  size_t mWriteOffset;
  std::list<WebrtcTCPData> mWriteQueue;
  nsCOMPtr<nsIAuthPromptProvider> mAuthProvider;

  // Indicates that the channel is CONNECTed
  nsCOMPtr<nsISocketTransport> mTransport;
  nsCOMPtr<nsIAsyncInputStream> mSocketIn;
  nsCOMPtr<nsIAsyncOutputStream> mSocketOut;
  nsCOMPtr<nsIEventTarget> mMainThread;
  nsCOMPtr<nsIEventTarget> mSocketThread;
  nsCOMPtr<nsICancelable> mProxyRequest;
};

}  // namespace net
}  // namespace mozilla

#endif  // webrtc_tcp_socket_h__
