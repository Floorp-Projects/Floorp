/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FlyWebPublishedServerIPC.h"
#include "mozilla/dom/FlyWebPublishBinding.h"
#include "mozilla/dom/FlyWebService.h"
#include "mozilla/dom/Request.h"
#include "mozilla/dom/FlyWebServerEvents.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/InternalResponse.h"
#include "mozilla/ipc/IPCStreamUtils.h"
#include "mozilla/net/NeckoParent.h"
#include "mozilla/net/IPCTransportProvider.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "nsCharSeparatedTokenizer.h"
#include "nsGlobalWindow.h"
#include "WebSocketChannel.h"

namespace mozilla {
namespace dom {

static LazyLogModule gFlyWebPublishedServerLog("FlyWebPublishedServer");
#undef LOG_I
#define LOG_I(...) MOZ_LOG(mozilla::dom::gFlyWebPublishedServerLog, mozilla::LogLevel::Debug, (__VA_ARGS__))
#undef LOG_E
#define LOG_E(...) MOZ_LOG(mozilla::dom::gFlyWebPublishedServerLog, mozilla::LogLevel::Error, (__VA_ARGS__))

/******** FlyWebPublishedServer ********/

FlyWebPublishedServer::FlyWebPublishedServer(nsPIDOMWindowInner* aOwner,
                                             const nsAString& aName,
                                             const FlyWebPublishOptions& aOptions)
  : mozilla::DOMEventTargetHelper(aOwner)
  , mOwnerWindowID(aOwner ? aOwner->WindowID() : 0)
  , mName(aName)
  , mUiUrl(aOptions.mUiUrl)
  , mIsRegistered(true) // Registered by the FlyWebService
{
}

void
FlyWebPublishedServer::LastRelease()
{
  // Make sure to unregister to avoid dangling pointers. Use the LastRelease
  // hook rather than dtor since calling virtual functions during dtor
  // wouldn't do what we want. Also, LastRelease is called earlier than dtor
  // for CC objects.
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
  LOG_I("FlyWebPublishedServer::Close(%p)", this);

  // Unregister from server.
  if (mIsRegistered) {
    MOZ_ASSERT(FlyWebService::GetExisting());
    FlyWebService::GetExisting()->UnregisterServer(this);
    mIsRegistered = false;

    DispatchTrustedEvent(NS_LITERAL_STRING("close"));
  }
}

void
FlyWebPublishedServer::FireFetchEvent(InternalRequest* aRequest)
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
FlyWebPublishedServer::FireWebsocketEvent(InternalRequest* aConnectRequest)
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
FlyWebPublishedServer::PublishedServerStarted(nsresult aStatus)
{
  LOG_I("FlyWebPublishedServer::PublishedServerStarted(%p)", this);

  RefPtr<FlyWebPublishPromise> promise = mPublishPromise.Ensure(__func__);
  if (NS_SUCCEEDED(aStatus)) {
    mPublishPromise.Resolve(this, __func__);
  } else {
    Close();
    mPublishPromise.Reject(aStatus, __func__);
  }
}

already_AddRefed<WebSocket>
FlyWebPublishedServer::OnWebSocketAccept(InternalRequest* aConnectRequest,
                                         const Optional<nsAString>& aProtocol,
                                         ErrorResult& aRv)
{
  MOZ_ASSERT(aConnectRequest);

  LOG_I("FlyWebPublishedServer::OnWebSocketAccept(%p)", this);

  nsCOMPtr<nsITransportProvider> provider =
    OnWebSocketAcceptInternal(aConnectRequest,
                              aProtocol,
                              aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  MOZ_ASSERT(provider);

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetOwner());
  AutoJSContext cx;
  GlobalObject global(cx, nsGlobalWindow::Cast(window)->FastGetGlobalJSObject());

  nsAutoCString extensions, negotiatedExtensions;
  aConnectRequest->Headers()->
    Get(NS_LITERAL_CSTRING("Sec-WebSocket-Extensions"), extensions, aRv);
  mozilla::net::ProcessServerWebSocketExtensions(extensions,
                                                 negotiatedExtensions);

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

/******** FlyWebPublishedServerImpl ********/

NS_IMPL_ISUPPORTS_INHERITED0(FlyWebPublishedServerImpl, mozilla::DOMEventTargetHelper)

FlyWebPublishedServerImpl::FlyWebPublishedServerImpl(nsPIDOMWindowInner* aOwner,
                                                     const nsAString& aName,
                                                     const FlyWebPublishOptions& aOptions)
  : FlyWebPublishedServer(aOwner, aName, aOptions)
  , mHttpServer(new HttpServer())
{
  LOG_I("FlyWebPublishedServerImpl::FlyWebPublishedServerImpl(%p)", this);
}

void
FlyWebPublishedServerImpl::PermissionGranted(bool aGranted)
{
  LOG_I("FlyWebPublishedServerImpl::PermissionGranted(%b)", aGranted);
  if (!aGranted) {
    PublishedServerStarted(NS_ERROR_FAILURE);
    return;
  }

  mHttpServer->Init(-1, Preferences::GetBool("flyweb.use-tls", false), this);
}

void
FlyWebPublishedServerImpl::Close()
{
  FlyWebPublishedServer::Close();

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
FlyWebPublishedServerImpl::OnServerStarted(nsresult aStatus)
{
  if (NS_SUCCEEDED(aStatus)) {
    FlyWebService::GetOrCreate()->StartDiscoveryOf(this);
  } else {
    PublishedServerStarted(aStatus);
  }
}

void
FlyWebPublishedServerImpl::OnFetchResponse(InternalRequest* aRequest,
                                           InternalResponse* aResponse)
{
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aResponse);

  LOG_I("FlyWebPublishedServerImpl::OnFetchResponse(%p)", this);

  if (mHttpServer) {
    mHttpServer->SendResponse(aRequest, aResponse);
  }
}

void
FlyWebPublishedServerImpl::OnWebSocketResponse(InternalRequest* aConnectRequest,
                                               InternalResponse* aResponse)
{
  MOZ_ASSERT(aConnectRequest);
  MOZ_ASSERT(aResponse);

  LOG_I("FlyWebPublishedMDNSServer::OnWebSocketResponse(%p)", this);

  if (mHttpServer) {
    mHttpServer->SendWebSocketResponse(aConnectRequest, aResponse);
  }
}

already_AddRefed<nsITransportProvider>
FlyWebPublishedServerImpl::OnWebSocketAcceptInternal(InternalRequest* aConnectRequest,
                                                     const Optional<nsAString>& aProtocol,
                                                     ErrorResult& aRv)
{
  LOG_I("FlyWebPublishedServerImpl::OnWebSocketAcceptInternal(%p)", this);

  if (!mHttpServer) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return mHttpServer->AcceptWebSocket(aConnectRequest,
                                      aProtocol,
                                      aRv);
}

/******** FlyWebPublishedServerChild ********/

FlyWebPublishedServerChild::FlyWebPublishedServerChild(nsPIDOMWindowInner* aOwner,
                                                       const nsAString& aName,
                                                       const FlyWebPublishOptions& aOptions)
  : FlyWebPublishedServer(aOwner, aName, aOptions)
  , mActorExists(false)
{
  LOG_I("FlyWebPublishedServerChild::FlyWebPublishedServerChild(%p)", this);

  // The matching release happens when the actor is destroyed, in
  // ContentChild::DeallocPFlyWebPublishedServerChild
  NS_ADDREF_THIS();
}

void
FlyWebPublishedServerChild::PermissionGranted(bool aGranted)
{
  if (!aGranted) {
    PublishedServerStarted(NS_ERROR_FAILURE);
    return;
  }

  mActorExists = true;
  FlyWebPublishOptions options;
  options.mUiUrl = mUiUrl;

  // Proceed with initialization.
  ContentChild::GetSingleton()->
    SendPFlyWebPublishedServerConstructor(this, mName, options);
}

bool
FlyWebPublishedServerChild::RecvServerReady(const nsresult& aStatus)
{
  LOG_I("FlyWebPublishedServerChild::RecvServerReady(%p)", this);
  MOZ_ASSERT(mActorExists);

  PublishedServerStarted(aStatus);
  return true;
}

bool
FlyWebPublishedServerChild::RecvServerClose()
{
  LOG_I("FlyWebPublishedServerChild::RecvServerClose(%p)", this);
  MOZ_ASSERT(mActorExists);

  Close();

  return true;
}

bool
FlyWebPublishedServerChild::RecvFetchRequest(const IPCInternalRequest& aRequest,
                                             const uint64_t& aRequestId)
{
  LOG_I("FlyWebPublishedServerChild::RecvFetchRequest(%p)", this);
  MOZ_ASSERT(mActorExists);

  RefPtr<InternalRequest> request = new InternalRequest(aRequest);
  mPendingRequests.Put(request, aRequestId);
  FireFetchEvent(request);

  return true;
}

bool
FlyWebPublishedServerChild::RecvWebSocketRequest(const IPCInternalRequest& aRequest,
                                                 const uint64_t& aRequestId,
                                                 PTransportProviderChild* aProvider)
{
  LOG_I("FlyWebPublishedServerChild::RecvWebSocketRequest(%p)", this);
  MOZ_ASSERT(mActorExists);

  RefPtr<InternalRequest> request = new InternalRequest(aRequest);
  mPendingRequests.Put(request, aRequestId);

  // Not addreffing here. The addref was already done when the
  // PTransportProvider child constructor original ran.
  mPendingTransportProviders.Put(aRequestId,
    dont_AddRef(static_cast<TransportProviderChild*>(aProvider)));

  FireWebsocketEvent(request);

  return true;
}

void
FlyWebPublishedServerChild::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG_I("FlyWebPublishedServerChild::ActorDestroy(%p)", this);

  mActorExists = false;
}

void
FlyWebPublishedServerChild::OnFetchResponse(InternalRequest* aRequest,
                                            InternalResponse* aResponse)
{
  LOG_I("FlyWebPublishedServerChild::OnFetchResponse(%p)", this);

  if (!mActorExists) {
    LOG_I("FlyWebPublishedServerChild::OnFetchResponse(%p) - No actor!", this);
    return;
  }

  uint64_t id = mPendingRequests.Get(aRequest);
  MOZ_ASSERT(id);
  mPendingRequests.Remove(aRequest);

  IPCInternalResponse ipcResp;
  UniquePtr<mozilla::ipc::AutoIPCStream> autoStream;
  aResponse->ToIPC(&ipcResp, Manager(), autoStream);
  Unused << SendFetchResponse(ipcResp, id);
  if (autoStream) {
    autoStream->TakeOptionalValue();
  }
}

already_AddRefed<nsITransportProvider>
FlyWebPublishedServerChild::OnWebSocketAcceptInternal(InternalRequest* aRequest,
                                                      const Optional<nsAString>& aProtocol,
                                                      ErrorResult& aRv)
{
  LOG_I("FlyWebPublishedServerChild::OnWebSocketAcceptInternal(%p)", this);

  if (!mActorExists) {
    LOG_I("FlyWebPublishedServerChild::OnWebSocketAcceptInternal(%p) - No actor!", this);
    return nullptr;
  }

  uint64_t id = mPendingRequests.Get(aRequest);
  MOZ_ASSERT(id);
  mPendingRequests.Remove(aRequest);

  RefPtr<TransportProviderChild> provider;
  mPendingTransportProviders.Remove(id, getter_AddRefs(provider));

  nsString protocol;
  if (aProtocol.WasPassed()) {
    protocol = aProtocol.Value();

    nsAutoCString reqProtocols;
    aRequest->Headers()->
      Get(NS_LITERAL_CSTRING("Sec-WebSocket-Protocol"), reqProtocols, aRv);
    if (!ContainsToken(reqProtocols, NS_ConvertUTF16toUTF8(protocol))) {
      // Should throw a better error here
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }
  } else {
    protocol.SetIsVoid(true);
  }

  Unused << SendWebSocketAccept(protocol, id);

  return provider.forget();
}

void
FlyWebPublishedServerChild::OnWebSocketResponse(InternalRequest* aRequest,
                                                InternalResponse* aResponse)
{
  LOG_I("FlyWebPublishedServerChild::OnFetchResponse(%p)", this);

  if (!mActorExists) {
    LOG_I("FlyWebPublishedServerChild::OnFetchResponse(%p) - No actor!", this);
    return;
  }

  uint64_t id = mPendingRequests.Get(aRequest);
  MOZ_ASSERT(id);
  mPendingRequests.Remove(aRequest);

  mPendingTransportProviders.Remove(id);

  IPCInternalResponse ipcResp;
  UniquePtr<mozilla::ipc::AutoIPCStream> autoStream;
  aResponse->ToIPC(&ipcResp, Manager(), autoStream);

  Unused << SendWebSocketResponse(ipcResp, id);
  if (autoStream) {
    autoStream->TakeOptionalValue();
  }
}

void
FlyWebPublishedServerChild::Close()
{
  LOG_I("FlyWebPublishedServerChild::Close(%p)", this);

  FlyWebPublishedServer::Close();

  if (mActorExists) {
    LOG_I("FlyWebPublishedServerChild::Close - sending __delete__ (%p)", this);

    Send__delete__(this);
  }
}

/******** FlyWebPublishedServerParent ********/

NS_IMPL_ISUPPORTS(FlyWebPublishedServerParent, nsIDOMEventListener)

FlyWebPublishedServerParent::FlyWebPublishedServerParent(const nsAString& aName,
                                                         const FlyWebPublishOptions& aOptions)
  : mActorDestroyed(false)
  , mNextRequestId(1)
{
  LOG_I("FlyWebPublishedServerParent::FlyWebPublishedServerParent(%p)", this);

  RefPtr<FlyWebService> service = FlyWebService::GetOrCreate();
  if (!service) {
    Unused << SendServerReady(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<FlyWebPublishPromise> mozPromise =
    service->PublishServer(aName, aOptions, nullptr);
  if (!mozPromise) {
    Unused << SendServerReady(NS_ERROR_FAILURE);
    return;
  }

  RefPtr<FlyWebPublishedServerParent> self = this;

  mozPromise->Then(
    AbstractThread::MainThread(),
    __func__,
    [this, self] (FlyWebPublishedServer* aServer) {
      mPublishedServer = static_cast<FlyWebPublishedServerImpl*>(aServer);
      if (mActorDestroyed) {
        mPublishedServer->Close();
        return;
      }

      mPublishedServer->AddEventListener(NS_LITERAL_STRING("fetch"),
                                         this, false, false, 2);
      mPublishedServer->AddEventListener(NS_LITERAL_STRING("websocket"),
                                         this, false, false, 2);
      mPublishedServer->AddEventListener(NS_LITERAL_STRING("close"),
                                         this, false, false, 2);
      Unused << SendServerReady(NS_OK);
    },
    [this, self] (nsresult aStatus) {
      MOZ_ASSERT(NS_FAILED(aStatus));
      if (!mActorDestroyed) {
        Unused << SendServerReady(aStatus);
      }
    });
}

NS_IMETHODIMP
FlyWebPublishedServerParent::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mActorDestroyed) {
    return NS_OK;
  }

  nsAutoString type;
  aEvent->GetType(type);
  if (type.EqualsLiteral("close")) {
    Unused << SendServerClose();
    return NS_OK;
  }

  if (type.EqualsLiteral("fetch")) {
    RefPtr<InternalRequest> request =
      static_cast<FlyWebFetchEvent*>(aEvent)->Request()->GetInternalRequest();
    uint64_t id = mNextRequestId++;
    mPendingRequests.Put(id, request);

    IPCInternalRequest ipcReq;
    request->ToIPC(&ipcReq);
    Unused << SendFetchRequest(ipcReq, id);
    return NS_OK;
  }

  if (type.EqualsLiteral("websocket")) {
    RefPtr<InternalRequest> request =
      static_cast<FlyWebWebSocketEvent*>(aEvent)->Request()->GetInternalRequest();
    uint64_t id = mNextRequestId++;
    mPendingRequests.Put(id, request);

    RefPtr<TransportProviderParent> provider =
      static_cast<TransportProviderParent*>(
        mozilla::net::gNeckoParent->SendPTransportProviderConstructor());

    IPCInternalRequest ipcReq;
    request->ToIPC(&ipcReq);
    Unused << SendWebSocketRequest(ipcReq, id, provider);

    mPendingTransportProviders.Put(id, provider.forget());
    return NS_OK;
  }

  MOZ_CRASH("Unknown event type");

  return NS_OK;
}

bool
FlyWebPublishedServerParent::RecvFetchResponse(const IPCInternalResponse& aResponse,
                                               const uint64_t& aRequestId)
{
  MOZ_ASSERT(!mActorDestroyed);

  RefPtr<InternalRequest> request;
  mPendingRequests.Remove(aRequestId, getter_AddRefs(request));
  if (!request) {
     static_cast<ContentParent*>(Manager())->KillHard("unknown request id");
     return false;
  }

  RefPtr<InternalResponse> response = InternalResponse::FromIPC(aResponse);

  mPublishedServer->OnFetchResponse(request, response);

  return true;
}

bool
FlyWebPublishedServerParent::RecvWebSocketResponse(const IPCInternalResponse& aResponse,
                                                   const uint64_t& aRequestId)
{
  MOZ_ASSERT(!mActorDestroyed);

  mPendingTransportProviders.Remove(aRequestId);

  RefPtr<InternalRequest> request;
  mPendingRequests.Remove(aRequestId, getter_AddRefs(request));
  if (!request) {
     static_cast<ContentParent*>(Manager())->KillHard("unknown websocket request id");
     return false;
  }

  RefPtr<InternalResponse> response = InternalResponse::FromIPC(aResponse);

  mPublishedServer->OnWebSocketResponse(request, response);

  return true;
}

bool
FlyWebPublishedServerParent::RecvWebSocketAccept(const nsString& aProtocol,
                                                 const uint64_t& aRequestId)
{
  MOZ_ASSERT(!mActorDestroyed);

  RefPtr<TransportProviderParent> providerIPC;
  mPendingTransportProviders.Remove(aRequestId, getter_AddRefs(providerIPC));

  RefPtr<InternalRequest> request;
  mPendingRequests.Remove(aRequestId, getter_AddRefs(request));

  if (!request || !providerIPC) {
     static_cast<ContentParent*>(Manager())->KillHard("unknown websocket request id");
     return false;
  }

  Optional<nsAString> protocol;
  if (!aProtocol.IsVoid()) {
    protocol = &aProtocol;
  }

  ErrorResult result;
  nsCOMPtr<nsITransportProvider> providerServer =
    mPublishedServer->OnWebSocketAcceptInternal(request, protocol, result);
  if (result.Failed()) {
    return false;
  }

  providerServer->SetListener(providerIPC);

  return true;
}

void
FlyWebPublishedServerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  LOG_I("FlyWebPublishedServerParent::ActorDestroy(%p)", this);

  mActorDestroyed = true;
}

bool
FlyWebPublishedServerParent::Recv__delete__()
{
  LOG_I("FlyWebPublishedServerParent::Recv__delete__(%p)", this);
  MOZ_ASSERT(!mActorDestroyed);

  if (mPublishedServer) {
    mPublishedServer->RemoveEventListener(NS_LITERAL_STRING("fetch"),
                                          this, false);
    mPublishedServer->RemoveEventListener(NS_LITERAL_STRING("websocket"),
                                          this, false);
    mPublishedServer->RemoveEventListener(NS_LITERAL_STRING("close"),
                                          this, false);
    mPublishedServer->Close();
    mPublishedServer = nullptr;
  }
  return true;
}

} // namespace dom
} // namespace mozilla


