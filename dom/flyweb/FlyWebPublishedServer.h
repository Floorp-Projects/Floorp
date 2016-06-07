/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebPublishedServer_h
#define mozilla_dom_FlyWebPublishedServer_h

#include "nsISupportsImpl.h"
#include "nsICancelable.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "HttpServer.h"
#include "mozilla/dom/WebSocket.h"
#include "mozilla/dom/PFlyWebPublishedServerParent.h"
#include "mozilla/dom/PFlyWebPublishedServerChild.h"
#include "mozilla/MozPromise.h"
#include "nsIDOMEventListener.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Promise;
class InternalResponse;
class InternalRequest;
struct FlyWebPublishOptions;
class FlyWebPublishedServer;
class FlyWebPublishedServerImpl;
class FlyWebPublishedServerParent;

typedef MozPromise<RefPtr<FlyWebPublishedServer>, nsresult, false>
  FlyWebPublishPromise;

class FlyWebPublishedServer : public mozilla::DOMEventTargetHelper
{
public:
  FlyWebPublishedServer(nsPIDOMWindowInner* aOwner,
                        const nsAString& aName,
                        const FlyWebPublishOptions& aOptions);

  virtual void LastRelease() override;

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  uint64_t OwnerWindowID() const {
    return mOwnerWindowID;
  }

  void GetName(nsAString& aName)
  {
    aName = mName;
  }
  nsAString& Name()
  {
    return mName;
  }

  void GetUiUrl(nsAString& aUiUrl)
  {
    aUiUrl = mUiUrl;
  }

  virtual void OnFetchResponse(InternalRequest* aRequest,
                               InternalResponse* aResponse) = 0;
  already_AddRefed<WebSocket>
    OnWebSocketAccept(InternalRequest* aConnectRequest,
                      const Optional<nsAString>& aProtocol,
                      ErrorResult& aRv);
  virtual void OnWebSocketResponse(InternalRequest* aConnectRequest,
                                   InternalResponse* aResponse) = 0;
  virtual already_AddRefed<nsITransportProvider>
    OnWebSocketAcceptInternal(InternalRequest* aConnectRequest,
                              const Optional<nsAString>& aProtocol,
                              ErrorResult& aRv) = 0;

  virtual void Close();

  void FireFetchEvent(InternalRequest* aRequest);
  void FireWebsocketEvent(InternalRequest* aConnectRequest);
  void PublishedServerStarted(nsresult aStatus);

  IMPL_EVENT_HANDLER(fetch)
  IMPL_EVENT_HANDLER(websocket)
  IMPL_EVENT_HANDLER(close)

  already_AddRefed<FlyWebPublishPromise>
  GetPublishPromise()
  {
    return mPublishPromise.Ensure(__func__);
  }

protected:
  virtual ~FlyWebPublishedServer()
  {
    MOZ_ASSERT(!mIsRegistered, "Subclass dtor forgot to call Close()");
  }

  uint64_t mOwnerWindowID;
  MozPromiseHolder<FlyWebPublishPromise> mPublishPromise;

  nsString mName;
  nsString mUiUrl;

  bool mIsRegistered;
};

class FlyWebPublishedServerImpl final : public FlyWebPublishedServer
                                      , public HttpServerListener
{
public:
  FlyWebPublishedServerImpl(nsPIDOMWindowInner* aOwner,
                            const nsAString& aName,
                            const FlyWebPublishOptions& aOptions);

  NS_DECL_ISUPPORTS_INHERITED

  int32_t Port()
  {
    return mHttpServer ? mHttpServer->GetPort() : 0;
  }
  void GetCertKey(nsACString& aKey) {
    if (mHttpServer) {
      mHttpServer->GetCertKey(aKey);
    } else {
      aKey.Truncate();
    }
  }

  virtual void OnFetchResponse(InternalRequest* aRequest,
                               InternalResponse* aResponse) override;
  virtual void OnWebSocketResponse(InternalRequest* aConnectRequest,
                                   InternalResponse* aResponse) override;
  virtual already_AddRefed<nsITransportProvider>
    OnWebSocketAcceptInternal(InternalRequest* aConnectRequest,
                              const Optional<nsAString>& aProtocol,
                              ErrorResult& aRv) override;

  void SetCancelRegister(nsICancelable* aCancelRegister)
  {
    mMDNSCancelRegister = aCancelRegister;
  }

  virtual void Close() override;

  // HttpServerListener
  virtual void OnServerStarted(nsresult aStatus) override;
  virtual void OnRequest(InternalRequest* aRequest) override
  {
    FireFetchEvent(aRequest);
  }
  virtual void OnWebSocket(InternalRequest* aConnectRequest) override
  {
    FireWebsocketEvent(aConnectRequest);
  }
  virtual void OnServerClose() override
  {
    mHttpServer = nullptr;
    Close();
  }

private:
  ~FlyWebPublishedServerImpl() {}

  RefPtr<HttpServer> mHttpServer;
  nsCOMPtr<nsICancelable> mMDNSCancelRegister;
  RefPtr<FlyWebPublishedServerParent> mServerParent;
};

class FlyWebPublishedServerChild final : public FlyWebPublishedServer
                                       , public PFlyWebPublishedServerChild
{
public:
  FlyWebPublishedServerChild(nsPIDOMWindowInner* aOwner,
                             const nsAString& aName,
                             const FlyWebPublishOptions& aOptions);

  virtual bool RecvServerReady(const nsresult& aStatus) override;
  virtual bool RecvServerClose() override;
  virtual bool RecvFetchRequest(const IPCInternalRequest& aRequest,
                                const uint64_t& aRequestId) override;

  virtual void OnFetchResponse(InternalRequest* aRequest,
                               InternalResponse* aResponse) override;
  virtual void OnWebSocketResponse(InternalRequest* aConnectRequest,
                                   InternalResponse* aResponse) override;
  virtual already_AddRefed<nsITransportProvider>
    OnWebSocketAcceptInternal(InternalRequest* aConnectRequest,
                              const Optional<nsAString>& aProtocol,
                              ErrorResult& aRv) override;

  virtual void Close() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~FlyWebPublishedServerChild() {}

  nsDataHashtable<nsRefPtrHashKey<InternalRequest>, uint64_t> mPendingRequests;
  bool mActorDestroyed;
};

class FlyWebPublishedServerParent final : public PFlyWebPublishedServerParent
                                        , public nsIDOMEventListener
{
public:
  FlyWebPublishedServerParent(const nsAString& aName,
                              const FlyWebPublishOptions& aOptions);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

private:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__() override;

  virtual bool
  RecvFetchResponse(const IPCInternalResponse& aResponse,
                    const uint64_t& aRequestId) override;

  ~FlyWebPublishedServerParent() {}

  bool mActorDestroyed;
  uint64_t mNextRequestId;
  nsRefPtrHashtable<nsUint64HashKey, InternalRequest> mPendingRequests;
  RefPtr<FlyWebPublishedServerImpl> mPublishedServer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebPublishedServer_h
