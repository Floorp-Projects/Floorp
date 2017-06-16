/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HttpServer_h
#define mozilla_dom_HttpServer_h

#include "nsISupportsImpl.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsITLSServerSocket.h"
#include "nsIAsyncInputStream.h"
#include "nsIAsyncOutputStream.h"
#include "mozilla/Variant.h"
#include "nsIRequestObserver.h"
#include "mozilla/MozPromise.h"
#include "nsITransportProvider.h"
#include "nsILocalCertService.h"

class nsIX509Cert;

class nsISerialEventTarget;

namespace mozilla {
namespace dom {

extern bool
ContainsToken(const nsCString& aList, const nsCString& aToken);

class InternalRequest;
class InternalResponse;

class HttpServerListener
{
public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void OnServerStarted(nsresult aStatus) = 0;
  virtual void OnRequest(InternalRequest* aRequest) = 0;
  virtual void OnWebSocket(InternalRequest* aConnectRequest) = 0;
  virtual void OnServerClose() = 0;
};

class HttpServer final : public nsIServerSocketListener,
                         public nsILocalCertGetCallback
{
public:
  explicit HttpServer(nsISerialEventTarget* aEventTarget);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISERVERSOCKETLISTENER
  NS_DECL_NSILOCALCERTGETCALLBACK

  void Init(int32_t aPort, bool aHttps, HttpServerListener* aListener);

  void SendResponse(InternalRequest* aRequest, InternalResponse* aResponse);
  already_AddRefed<nsITransportProvider>
    AcceptWebSocket(InternalRequest* aConnectRequest,
                    const Optional<nsAString>& aProtocol,
                    ErrorResult& aRv);
  void SendWebSocketResponse(InternalRequest* aConnectRequest,
                             InternalResponse* aResponse);

  void Close();

  void GetCertKey(nsACString& aKey);

  int32_t GetPort()
  {
    return mPort;
  }

private:
  ~HttpServer();

  nsresult StartServerSocket(nsIX509Cert* aCert);
  void NotifyStarted(nsresult aStatus);

  class TransportProvider final : public nsITransportProvider
  {
  public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITRANSPORTPROVIDER

    void SetTransport(nsISocketTransport* aTransport,
                      nsIAsyncInputStream* aInput,
                      nsIAsyncOutputStream* aOutput);

  private:
    virtual ~TransportProvider();
    void MaybeNotify();

    nsCOMPtr<nsIHttpUpgradeListener> mListener;
    nsCOMPtr<nsISocketTransport> mTransport;
    nsCOMPtr<nsIAsyncInputStream> mInput;
    nsCOMPtr<nsIAsyncOutputStream> mOutput;
  };

  class Connection final : public nsIInputStreamCallback
                         , public nsIOutputStreamCallback
                         , public nsITLSServerSecurityObserver
  {
  public:
    Connection(nsISocketTransport* aTransport,
               HttpServer* aServer,
               nsresult& rv);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAMCALLBACK
    NS_DECL_NSIOUTPUTSTREAMCALLBACK
    NS_DECL_NSITLSSERVERSECURITYOBSERVER

    bool TryHandleResponse(InternalRequest* aRequest,
                           InternalResponse* aResponse);
    already_AddRefed<nsITransportProvider>
      HandleAcceptWebSocket(const Optional<nsAString>& aProtocol,
                            ErrorResult& aRv);
    void HandleWebSocketResponse(InternalResponse* aResponse);
    bool HasPendingWebSocketRequest(InternalRequest* aRequest)
    {
      return aRequest == mPendingWebSocketRequest;
    }

    void Close();

    private:
    ~Connection();

    void SetSecurityObserver(bool aListen);

    static nsresult ReadSegmentsFunc(nsIInputStream* aIn,
                                     void* aClosure,
                                     const char* aBuffer,
                                     uint32_t aToOffset,
                                     uint32_t aCount,
                                     uint32_t* aWriteCount);
    nsresult ConsumeInput(const char*& aBuffer,
                          const char* aEnd);
    nsresult ConsumeLine(const char* aBuffer,
                         size_t aLength);
    void MaybeAddPendingHeader();

    void QueueResponse(InternalResponse* aResponse);

    RefPtr<HttpServer> mServer;
    nsCOMPtr<nsISocketTransport> mTransport;
    nsCOMPtr<nsIAsyncInputStream> mInput;
    nsCOMPtr<nsIAsyncOutputStream> mOutput;

    enum { eRequestLine, eHeaders, eBody, ePause } mState;
    RefPtr<InternalRequest> mPendingReq;
    uint32_t mPendingReqVersion;
    nsCString mInputBuffer;
    nsCString mPendingHeaderName;
    nsCString mPendingHeaderValue;
    uint32_t mRemainingBodySize;
    nsCOMPtr<nsIAsyncOutputStream> mCurrentRequestBody;
    bool mCloseAfterRequest;

    typedef Pair<RefPtr<InternalRequest>,
                 RefPtr<InternalResponse>> PendingRequest;
    nsTArray<PendingRequest> mPendingRequests;
    RefPtr<MozPromise<nsresult, bool, false>> mOutputCopy;

    RefPtr<InternalRequest> mPendingWebSocketRequest;
    RefPtr<TransportProvider> mWebSocketTransportProvider;

    struct OutputBuffer {
      nsCString mString;
      nsCOMPtr<nsIInputStream> mStream;
      bool mChunked;
    };

    nsTArray<OutputBuffer> mOutputBuffers;
  };

  friend class Connection;

  RefPtr<HttpServerListener> mListener;
  nsCOMPtr<nsIServerSocket> mServerSocket;
  nsCOMPtr<nsIX509Cert> mCert;

  nsTArray<RefPtr<Connection>> mConnections;

  int32_t mPort;
  bool mHttps;

  const nsCOMPtr<nsISerialEventTarget> mEventTarget;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_HttpServer_h
