/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebPublishedServerIPC_h
#define mozilla_dom_FlyWebPublishedServerIPC_h

#include "HttpServer.h"
#include "mozilla/dom/FlyWebPublishedServer.h"
#include "mozilla/dom/PFlyWebPublishedServerParent.h"
#include "mozilla/dom/PFlyWebPublishedServerChild.h"
#include "mozilla/MozPromise.h"
#include "nsISupportsImpl.h"
#include "nsICancelable.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class FlyWebPublishedServerParent;

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
  virtual already_AddRefed<WebSocket>
    OnWebSocketAccept(InternalRequest* aConnectRequest,
                      const Optional<nsAString>& aProtocol,
                      ErrorResult& aRv) override;
  virtual void OnWebSocketResponse(InternalRequest* aConnectRequest,
                                   InternalResponse* aResponse) override;

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

  virtual void OnFetchResponse(InternalRequest* aRequest,
                               InternalResponse* aResponse) override;
  virtual already_AddRefed<WebSocket>
    OnWebSocketAccept(InternalRequest* aConnectRequest,
                      const Optional<nsAString>& aProtocol,
                      ErrorResult& aRv) override;
  virtual void OnWebSocketResponse(InternalRequest* aConnectRequest,
                                   InternalResponse* aResponse) override;

  virtual void Close() override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~FlyWebPublishedServerChild() {}

  bool mActorDestroyed;
};

class FlyWebPublishedServerParent final : public PFlyWebPublishedServerParent
{
public:
  FlyWebPublishedServerParent(const nsAString& aName,
                              const FlyWebPublishOptions& aOptions);

  NS_INLINE_DECL_REFCOUNTING(FlyWebPublishedServerParent)

private:
  virtual void
  ActorDestroy(ActorDestroyReason aWhy) override;

  virtual bool
  Recv__delete__() override;

  ~FlyWebPublishedServerParent() {}

  bool mActorDestroyed;
  RefPtr<FlyWebPublishedServerImpl> mPublishedServer;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebPublishedServerIPC_h
