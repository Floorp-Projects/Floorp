/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FlyWebPublishedServer_h
#define mozilla_dom_FlyWebPublishedServer_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/MozPromise.h"

class nsPIDOMWindowInner;
class nsITransportProvider;

namespace mozilla {

class ErrorResult;

namespace dom {

class InternalResponse;
class InternalRequest;
class WebSocket;
struct FlyWebPublishOptions;
class FlyWebPublishedServer;

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

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_FlyWebPublishedServer_h
