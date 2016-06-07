/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FlyWebPublishedServer.h"
#include "mozilla/dom/FlyWebPublishBinding.h"
#include "mozilla/dom/FlyWebService.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/FlyWebServerEvents.h"
#include "mozilla/Preferences.h"
#include "nsGlobalWindow.h"

namespace mozilla {
namespace dom {

static LazyLogModule gFlyWebPublishedServerLog("FlyWebPublishedServer");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(mozilla::dom::gFlyWebPublishedServerLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(mozilla::dom::gFlyWebPublishedServerLog, mozilla::LogLevel::Error, (__VA_ARGS__))

NS_IMPL_ISUPPORTS_INHERITED0(FlyWebPublishedServer, mozilla::DOMEventTargetHelper)

FlyWebPublishedServer::FlyWebPublishedServer(nsPIDOMWindowInner* aOwner,
                                             const nsAString& aName,
                                             const FlyWebPublishOptions& aOptions,
                                             Promise* aPublishPromise)
  : mozilla::DOMEventTargetHelper(aOwner)
  , mOwnerWindowID(aOwner ? aOwner->WindowID() : 0)
  , mPublishPromise(aPublishPromise)
  , mName(aName)
  , mUiUrl(aOptions.mUiUrl)
  , mIsRegistered(true) // Registered by the FlyWebService
{
  mHttpServer = new HttpServer();
  mHttpServer->Init(-1, Preferences::GetBool("flyweb.use-tls", false), this);
}

FlyWebPublishedServer::~FlyWebPublishedServer()
{
  // Make sure to unregister to avoid dangling pointers
  Close();
}

JSObject*
FlyWebPublishedServer::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return FlyWebPublishedServerBinding::Wrap(aCx, this, aGivenProto);
}

void
FlyWebPublishedServer::Close()
{
  // Unregister from server.
  if (mIsRegistered) {
    FlyWebService::GetOrCreate()->UnregisterServer(this);
    mIsRegistered = false;
  }

  if (mMDNSCancelRegister) {
    mMDNSCancelRegister->Cancel(NS_BINDING_ABORTED);
    mMDNSCancelRegister = nullptr;
  }

  if (mHttpServer) {
    RefPtr<HttpServer> server = mHttpServer.forget();
    server->Close();
  }
}

void
FlyWebPublishedServer::OnServerStarted(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus)) {
    FlyWebService::GetOrCreate()->StartDiscoveryOf(this);
  } else {
    DiscoveryStarted(aStatus);
  }
}

void
FlyWebPublishedServer::OnServerClose()
{
  mHttpServer = nullptr;
  Close();

  DispatchTrustedEvent(NS_LITERAL_STRING("close"));
}

void
FlyWebPublishedServer::OnRequest(InternalRequest* aRequest)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  RefPtr<FlyWebFetchEvent> e = new FlyWebFetchEvent(this,
                                                    new Request(global, aRequest),
                                                    aRequest);
  e->Init(this);
  e->InitEvent(NS_LITERAL_STRING("fetch"), false, false);

  DispatchTrustedEvent(e);
}

void
FlyWebPublishedServer::OnWebSocket(InternalRequest* aConnectRequest)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  RefPtr<FlyWebFetchEvent> e = new FlyWebWebSocketEvent(this,
                                                        new Request(global, aConnectRequest),
                                                        aConnectRequest);
  e->Init(this);
  e->InitEvent(NS_LITERAL_STRING("websocket"), false, false);

  DispatchTrustedEvent(e);
}

void
FlyWebPublishedServer::OnFetchResponse(InternalRequest* aRequest,
                                       InternalResponse* aResponse)
{
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aResponse);

  LOG_I("FlyWebPublishedMDNSServer::OnFetchResponse(%p)", this);

  if (mHttpServer) {
    mHttpServer->SendResponse(aRequest, aResponse);
  }
}

already_AddRefed<WebSocket>
FlyWebPublishedServer::OnWebSocketAccept(InternalRequest* aConnectRequest,
                                         const Optional<nsAString>& aProtocol,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(aConnectRequest);

  LOG_I("FlyWebPublishedMDNSServer::OnWebSocketAccept(%p)", this);

  if (!mHttpServer) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsAutoCString negotiatedExtensions;
  nsCOMPtr<nsITransportProvider> provider =
    mHttpServer->AcceptWebSocket(aConnectRequest,
                                 aProtocol,
                                 negotiatedExtensions,
                                 aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(provider);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetOwner());
  AutoJSContext cx;
  GlobalObject global(cx, nsGlobalWindow::Cast(window)->FastGetGlobalJSObject());

  nsCString url;
  aConnectRequest->GetURL(url);
  Sequence<nsString> protocols;
  if (aProtocol.WasPassed() &&
      !protocols.AppendElement(aProtocol.Value(), fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  return WebSocket::ConstructorCommon(global,
                                      NS_ConvertUTF8toUTF16(url),
                                      protocols,
                                      provider,
                                      negotiatedExtensions,
                                      aRv);
}

void
FlyWebPublishedServer::OnWebSocketResponse(InternalRequest* aConnectRequest,
                                           InternalResponse* aResponse)
{
  MOZ_ASSERT(aConnectRequest);
  MOZ_ASSERT(aResponse);

  LOG_I("FlyWebPublishedMDNSServer::OnWebSocketResponse(%p)", this);

  if (mHttpServer) {
    mHttpServer->SendWebSocketResponse(aConnectRequest, aResponse);
  }
}

void FlyWebPublishedServer::DiscoveryStarted(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus)) {
    mPublishPromise->MaybeResolve(this);
  } else {
    Close();
    mPublishPromise->MaybeReject(aStatus);
  }
}

} // namespace dom
} // namespace mozilla


