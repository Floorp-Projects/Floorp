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

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class Promise;
class InternalResponse;
class InternalRequest;
struct FlyWebPublishOptions;
class FlyWebPublishedServer;

class FlyWebPublishedServer final : public mozilla::DOMEventTargetHelper
                                  , public HttpServerListener
{
public:
  FlyWebPublishedServer(nsPIDOMWindowInner* aOwner,
                        const nsAString& aName,
                        const FlyWebPublishOptions& aOptions,
                        Promise* aPublishPromise);

  virtual JSObject* WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

  NS_DECL_ISUPPORTS_INHERITED

  uint64_t OwnerWindowID() const {
    return mOwnerWindowID;
  }

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

  void GetName(nsAString& aName)
  {
    aName = mName;
  }
  nsAString& Name()
  {
    return mName;
  }

  void GetCategory(nsAString& aCategory)
  {
    aCategory = mCategory;
  }

  bool Http()
  {
    return mHttp;
  }

  bool Message()
  {
    return mMessage;
  }

  void GetUiUrl(nsAString& aUiUrl)
  {
    aUiUrl = mUiUrl;
  }

  void OnFetchResponse(InternalRequest* aRequest,
                       InternalResponse* aResponse);
  already_AddRefed<WebSocket>
    OnWebSocketAccept(InternalRequest* aConnectRequest,
                      const Optional<nsAString>& aProtocol,
                      ErrorResult& aRv);
  void OnWebSocketResponse(InternalRequest* aConnectRequest,
                           InternalResponse* aResponse);

  void SetCancelRegister(nsICancelable* aCancelRegister)
  {
    mMDNSCancelRegister = aCancelRegister;
  }

  void Close();

  // HttpServerListener
  virtual void OnServerStarted(nsresult aStatus) override;
  virtual void OnRequest(InternalRequest* aRequest) override;
  virtual void OnWebSocket(InternalRequest* aConnectRequest) override;
  virtual void OnServerClose() override;

  IMPL_EVENT_HANDLER(fetch)
  IMPL_EVENT_HANDLER(websocket)
  IMPL_EVENT_HANDLER(close)

  void DiscoveryStarted(nsresult aStatus);

private:
  ~FlyWebPublishedServer();

  uint64_t mOwnerWindowID;
  RefPtr<HttpServer> mHttpServer;
  RefPtr<Promise> mPublishPromise;
  nsCOMPtr<nsICancelable> mMDNSCancelRegister;

  nsString mName;
  nsString mCategory;
  bool mHttp;
  bool mMessage;
  nsString mUiUrl;

  bool mIsRegistered;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebPublishedServer_h
