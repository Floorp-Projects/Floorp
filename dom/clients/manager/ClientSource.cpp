/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientSource.h"

#include "ClientManager.h"
#include "ClientManagerChild.h"
#include "ClientPrincipalUtils.h"
#include "ClientSourceChild.h"
#include "ClientState.h"
#include "ClientValidation.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "nsContentUtils.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

using mozilla::dom::ipc::StructuredCloneData;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

void
ClientSource::Shutdown()
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

void
ClientSource::ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs)
{
  // Fast fail if we don't understand this particular principal/URL combination.
  // This can happen since we use MozURL for validation which does not handle
  // some of the more obscure internal principal/url combinations.  Normal
  // content pages will pass this check.
  if (NS_WARN_IF(!ClientIsValidCreationURL(mClientInfo.PrincipalInfo(),
                                           aArgs.url()))) {
    Shutdown();
    return;
  }

  mClientInfo.SetURL(aArgs.url());
  mClientInfo.SetFrameType(aArgs.frameType());
  MaybeExecute([aArgs](PClientSourceChild* aActor) {
    aActor->SendExecutionReady(aArgs);
  });
}

nsresult
ClientSource::SnapshotWindowState(ClientState* aStateOut)
{
  MOZ_ASSERT(NS_IsMainThread());

  nsPIDOMWindowInner* window = GetInnerWindow();
  if (!window || !window->IsCurrentInnerWindow() ||
      !window->HasActiveDocument()) {
    *aStateOut = ClientState(ClientWindowState(VisibilityState::Hidden,
                                               TimeStamp(),
                                               nsContentUtils::StorageAccess::eDeny,
                                               false));
    return NS_OK;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (NS_WARN_IF(!doc)) {
    return NS_ERROR_UNEXPECTED;
  }

  ErrorResult rv;
  bool focused = doc->HasFocus(rv);
  if (NS_WARN_IF(rv.Failed())) {
    rv.SuppressException();
    return rv.StealNSResult();
  }

  nsContentUtils::StorageAccess storage =
    nsContentUtils::StorageAllowedForDocument(doc);

  *aStateOut = ClientState(ClientWindowState(doc->VisibilityState(),
                                             doc->LastFocusTime(), storage,
                                             focused));

  return NS_OK;
}

WorkerPrivate*
ClientSource::GetWorkerPrivate() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<WorkerPrivate*>()) {
    return nullptr;
  }
  return mOwner.as<WorkerPrivate*>();
}

nsIDocShell*
ClientSource::GetDocShell() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    return nullptr;
  }
  return mOwner.as<nsCOMPtr<nsIDocShell>>();
}

nsIGlobalObject*
ClientSource::GetGlobal() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  nsPIDOMWindowInner* win = GetInnerWindow();
  if (win) {
    return win->AsGlobal();
  }

  WorkerPrivate* wp = GetWorkerPrivate();
  if (wp) {
    return wp->GlobalScope();
  }

  // Note, ClientSource objects attached to docshell for conceptual
  // initial about:blank will get nullptr here.  The caller should
  // use MaybeCreateIntitialDocument() to create the window before
  // GetGlobal() if it wants this before.

  return nullptr;
}

void
ClientSource::MaybeCreateInitialDocument()
{
  nsIDocShell* docshell = GetDocShell();
  if (docshell) {
    // Force the create of the initial document if it does not exist yet.
    Unused << docshell->GetDocument();

    MOZ_DIAGNOSTIC_ASSERT(GetInnerWindow());
  }
}

ClientSource::ClientSource(ClientManager* aManager,
                           nsISerialEventTarget* aEventTarget,
                           const ClientSourceConstructorArgs& aArgs)
  : mManager(aManager)
  , mEventTarget(aEventTarget)
  , mOwner(AsVariant(Nothing()))
  , mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(), aArgs.creationTime())
{
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mEventTarget);
}

void
ClientSource::Activate(PClientManagerChild* aActor)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_ASSERT(!GetActor());

  if (IsShutdown()) {
    return;
  }

  // Fast fail if we don't understand this particular kind of PrincipalInfo.
  // This can happen since we use MozURL for validation which does not handle
  // some of the more obscure internal principal/url combinations.  Normal
  // content pages will pass this check.
  if (NS_WARN_IF(!ClientIsValidPrincipalInfo(mClientInfo.PrincipalInfo()))) {
    Shutdown();
    return;
  }

  ClientSourceConstructorArgs args(mClientInfo.Id(), mClientInfo.Type(),
                                   mClientInfo.PrincipalInfo(),
                                   mClientInfo.CreationTime());
  PClientSourceChild* actor = aActor->SendPClientSourceConstructor(args);
  if (!actor) {
    Shutdown();
    return;
  }

  ActivateThing(static_cast<ClientSourceChild*>(actor));
}

ClientSource::~ClientSource()
{
  Shutdown();
}

nsPIDOMWindowInner*
ClientSource::GetInnerWindow() const
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    return nullptr;
  }
  return mOwner.as<RefPtr<nsPIDOMWindowInner>>();
}

void
ClientSource::WorkerExecutionReady(WorkerPrivate* aWorkerPrivate)
{
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  if (IsShutdown()) {
    return;
  }

  // A client without access to storage should never be controlled by
  // a service worker.  Check this here in case we were controlled before
  // execution ready.  We can't reliably determine what our storage policy
  // is before execution ready, unfortunately.
  if (mController.isSome()) {
    MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate->IsStorageAllowed() ||
                          StringBeginsWith(aWorkerPrivate->ScriptURL(),
                                           NS_LITERAL_STRING("blob:")));
  }

  // Its safe to store the WorkerPrivate* here because the ClientSource
  // is explicitly destroyed by WorkerPrivate before exiting its run loop.
  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>());
  mOwner = AsVariant(aWorkerPrivate);

  ClientSourceExecutionReadyArgs args(
    aWorkerPrivate->GetLocationInfo().mHref,
    FrameType::None);

  ExecutionReady(args);
}

nsresult
ClientSource::WindowExecutionReady(nsPIDOMWindowInner* aInnerWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow);
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->HasActiveDocument());

  if (IsShutdown()) {
    return NS_OK;
  }

  nsIDocument* doc = aInnerWindow->GetExtantDoc();
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  nsIURI* uri = doc->GetOriginalURI();
  NS_ENSURE_TRUE(uri, NS_ERROR_UNEXPECTED);

  // Don't use nsAutoCString here since IPC requires a full nsCString anyway.
  nsCString spec;
  nsresult rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  // A client without access to storage should never be controlled by
  // a service worker.  Check this here in case we were controlled before
  // execution ready.  We can't reliably determine what our storage policy
  // is before execution ready, unfortunately.
  //
  // Note, explicitly avoid checking storage policy for windows that inherit
  // service workers from their parent.  If a user opens a controlled window
  // and then blocks storage, that window will continue to be controlled by
  // the SW until the window is closed.  Any about:blank or blob URL should
  // continue to inherit the SW as well.  We need to avoid triggering the
  // assertion in this corner case.
  if (mController.isSome()) {
    MOZ_DIAGNOSTIC_ASSERT(spec.LowerCaseEqualsLiteral("about:blank") ||
                          StringBeginsWith(spec, NS_LITERAL_CSTRING("blob:")) ||
                          nsContentUtils::StorageAllowedForWindow(aInnerWindow) ==
                          nsContentUtils::StorageAccess::eAllow);
  }

  nsPIDOMWindowOuter* outer = aInnerWindow->GetOuterWindow();
  NS_ENSURE_TRUE(outer, NS_ERROR_UNEXPECTED);

  FrameType frameType = FrameType::Top_level;
  if (!outer->IsTopLevelWindow()) {
    frameType = FrameType::Nested;
  } else if(outer->HadOriginalOpener()) {
    frameType = FrameType::Auxiliary;
  }

  // We should either be setting a window execution ready for the
  // first time or setting the same window execution ready again.
  // The secondary calls are due to initial about:blank replacement.
  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>() ||
                        mOwner.is<nsCOMPtr<nsIDocShell>>() ||
                        GetInnerWindow() == aInnerWindow);

  // This creates a cycle with the window.  It is broken when
  // nsGlobalWindow::FreeInnerObjects() deletes the ClientSource.
  mOwner = AsVariant(RefPtr<nsPIDOMWindowInner>(aInnerWindow));

  ClientSourceExecutionReadyArgs args(spec, frameType);
  ExecutionReady(args);

  return NS_OK;
}

nsresult
ClientSource::DocShellExecutionReady(nsIDocShell* aDocShell)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aDocShell);

  if (IsShutdown()) {
    return NS_OK;
  }

  nsPIDOMWindowOuter* outer = aDocShell->GetWindow();
  if (NS_WARN_IF(!outer)) {
    return NS_ERROR_UNEXPECTED;
  }

  // Note: We don't assert storage access for a controlled client.  If
  // the about:blank actually gets used then WindowExecutionReady() will
  // get called which asserts storage access.

  // TODO: dedupe this with WindowExecutionReady
  FrameType frameType = FrameType::Top_level;
  if (!outer->IsTopLevelWindow()) {
    frameType = FrameType::Nested;
  } else if(outer->HadOriginalOpener()) {
    frameType = FrameType::Auxiliary;
  }

  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>());

  // This creates a cycle with the docshell.  It is broken when
  // nsDocShell::Destroy() deletes the ClientSource.
  mOwner = AsVariant(nsCOMPtr<nsIDocShell>(aDocShell));

  ClientSourceExecutionReadyArgs args(NS_LITERAL_CSTRING("about:blank"),
                                      frameType);
  ExecutionReady(args);

  return NS_OK;
}

void
ClientSource::Freeze()
{
  MaybeExecute([](PClientSourceChild* aActor) {
    aActor->SendFreeze();
  });
}

void
ClientSource::Thaw()
{
  MaybeExecute([](PClientSourceChild* aActor) {
    aActor->SendThaw();
  });
}

const ClientInfo&
ClientSource::Info() const
{
  return mClientInfo;
}

void
ClientSource::WorkerSyncPing(WorkerPrivate* aWorkerPrivate)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);

  if (IsShutdown()) {
    return;
  }

  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate == mManager->GetWorkerPrivate());
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_DIAGNOSTIC_ASSERT(GetActor());

  GetActor()->SendWorkerSyncPing();
}

void
ClientSource::SetController(const ServiceWorkerDescriptor& aServiceWorker)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // We should never have a cross-origin controller.  Since this would be
  // same-origin policy violation we do a full release assertion here.
  MOZ_RELEASE_ASSERT(ClientMatchPrincipalInfo(mClientInfo.PrincipalInfo(),
                                              aServiceWorker.PrincipalInfo()));

  // A client in private browsing mode should never be controlled by
  // a service worker.  The principal origin attributes should guarantee
  // this invariant.
  MOZ_DIAGNOSTIC_ASSERT(!mClientInfo.IsPrivateBrowsing());

  // A client without access to storage should never be controlled a
  // a service worker.  If we are already execution ready with a real
  // window or worker, then verify assert the storage policy is correct.
  //
  // Note, explicitly avoid checking storage policy for clients that inherit
  // service workers from their parent.  This basically means blob: URLs
  // and about:blank windows.
  if (GetInnerWindow()) {
    MOZ_DIAGNOSTIC_ASSERT(Info().URL().LowerCaseEqualsLiteral("about:blank") ||
                          StringBeginsWith(Info().URL(), NS_LITERAL_CSTRING("blob:")) ||
                          nsContentUtils::StorageAllowedForWindow(GetInnerWindow()) ==
                          nsContentUtils::StorageAccess::eAllow);
  } else if (GetWorkerPrivate()) {
    MOZ_DIAGNOSTIC_ASSERT(GetWorkerPrivate()->IsStorageAllowed() ||
                          StringBeginsWith(GetWorkerPrivate()->ScriptURL(),
                                           NS_LITERAL_STRING("blob:")));
  }

  if (mController.isSome() && mController.ref() == aServiceWorker) {
    return;
  }

  mController.reset();
  mController.emplace(aServiceWorker);

  RefPtr<ServiceWorkerContainer> swc;
  nsPIDOMWindowInner* window = GetInnerWindow();
  if (window) {
    swc = window->Navigator()->ServiceWorker();
  }

  // TODO: Also self.navigator.serviceWorker on workers when its exposed there

  if (swc && nsContentUtils::IsSafeToRunScript()) {
    swc->ControllerChanged(IgnoreErrors());
  }
}

RefPtr<ClientOpPromise>
ClientSource::Control(const ClientControlledArgs& aArgs)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // Determine if the client is allowed to be controlled.  Currently we
  // prevent service workers from controlling clients that cannot access
  // storage.  We exempt this restriction for local URL clients, like about:blank
  // and blob:, since access to service workers is dictated by their parent.
  //
  // Note, we default to allowing the client to be controlled in the case
  // where we are not execution ready yet.  This can only happen if the
  // the non-subresource load is intercepted by a service worker.  Since
  // ServiceWorkerInterceptController() uses StorageAllowedForChannel()
  // it should be fine to accept these control messages.
  //
  // Its also fine to default to allowing ClientSource attached to a docshell
  // to be controlled.  These clients represent inital about:blank windows
  // that do not have an inner window created yet.  We explicitly allow initial
  // about:blank.
  bool controlAllowed = true;
  if (GetInnerWindow()) {

    // Local URL windows and windows with access to storage can be controlled.
    controlAllowed = Info().URL().LowerCaseEqualsLiteral("about:blank") ||
                     StringBeginsWith(Info().URL(), NS_LITERAL_CSTRING("blob:")) ||
                     nsContentUtils::StorageAllowedForWindow(GetInnerWindow()) ==
                      nsContentUtils::StorageAccess::eAllow;
  } else if (GetWorkerPrivate()) {
    // Local URL workers and workers with access to storage cna be controlled.
    controlAllowed = GetWorkerPrivate()->IsStorageAllowed() ||
                     StringBeginsWith(GetWorkerPrivate()->ScriptURL(),
                                      NS_LITERAL_STRING("blob:"));
  }

  RefPtr<ClientOpPromise> ref;

  if (NS_WARN_IF(!controlAllowed)) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
    return ref.forget();
  }

  SetController(ServiceWorkerDescriptor(aArgs.serviceWorker()));

  ref = ClientOpPromise::CreateAndResolve(NS_OK, __func__);
  return ref.forget();
}

void
ClientSource::InheritController(const ServiceWorkerDescriptor& aServiceWorker)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // If we are in legacy child-side intercept mode then we must tell the current
  // process SWM that this client inherited a controller.  This will only update
  // the local SWM data and not send any messages to the ClientManagerService.
  //
  // Note, we only do this when inheriting the controller for main thread
  // windows.  The legacy mode never proprly marked inherited blob URL workers
  // controlled in the SWM.
  if (!ServiceWorkerParentInterceptEnabled() && GetDocShell()) {
    AssertIsOnMainThread();
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->NoteInheritedController(mClientInfo, aServiceWorker);
    }
  }

  // Also tell the parent-side ClientManagerService that the controller was
  // inherited.  This is necessary for clients.matchAll() to work properly.
  // In parent-side intercept mode this will also note the inheritance in
  // the parent-side SWM.
  MaybeExecute([aServiceWorker](PClientSourceChild* aActor) {
    aActor->SendInheritController(ClientControlledArgs(aServiceWorker.ToIPC()));
  });

  // Finally, record the new controller in our local ClientSource for any
  // immediate synchronous access.
  SetController(aServiceWorker);
}

const Maybe<ServiceWorkerDescriptor>&
ClientSource::GetController() const
{
  return mController;
}

void
ClientSource::NoteDOMContentLoaded()
{
  if (mController.isSome() && !ServiceWorkerParentInterceptEnabled()) {
    AssertIsOnMainThread();
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->MaybeCheckNavigationUpdate(mClientInfo);
    }
  }

  MaybeExecute([] (PClientSourceChild* aActor) {
    aActor->SendNoteDOMContentLoaded();
  });
}

RefPtr<ClientOpPromise>
ClientSource::Focus(const ClientFocusArgs& aArgs)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  RefPtr<ClientOpPromise> ref;

  if (mClientInfo.Type() != ClientType::Window) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR,
                                           __func__);
    return ref.forget();
  }
  nsPIDOMWindowOuter* outer = nullptr;

  nsPIDOMWindowInner* inner = GetInnerWindow();
  if (inner) {
    outer = inner->GetOuterWindow();
  } else {
    nsIDocShell* docshell = GetDocShell();
    if (docshell) {
      outer = docshell->GetWindow();
    }
  }

  if (!outer) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
    return ref.forget();
  }

  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv = nsContentUtils::DispatchFocusChromeEvent(outer);
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  ClientState state;
  rv = SnapshotState(&state);
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  ref = ClientOpPromise::CreateAndResolve(state.ToIPC(), __func__);
  return ref.forget();
}

RefPtr<ClientOpPromise>
ClientSource::PostMessage(const ClientPostMessageArgs& aArgs)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  RefPtr<ClientOpPromise> ref;

  ServiceWorkerDescriptor source(aArgs.serviceWorker());
  const PrincipalInfo& principalInfo = source.PrincipalInfo();

  StructuredCloneData clonedData;
  clonedData.BorrowFromClonedMessageDataForBackgroundChild(aArgs.clonedData());

  // Currently we only support firing these messages on window Clients.
  // Once we expose ServiceWorkerContainer and the ServiceWorker on Worker
  // threads then this will need to change.  See bug 1113522.
  if (mClientInfo.Type() != ClientType::Window) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
    return ref.forget();
  }

  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<ServiceWorkerContainer> target;
  nsCOMPtr<nsIGlobalObject> globalObject;

  // We don't need to force the creation of the about:blank document
  // here because there is no postMessage listener.  If a listener
  // was registered then the document will already be created.
  nsPIDOMWindowInner* window = GetInnerWindow();
  if (window) {
    globalObject = do_QueryInterface(window);
    target = window->Navigator()->ServiceWorker();
  }

  if (NS_WARN_IF(!target)) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
    return ref.forget();
  }

  // If AutoJSAPI::Init() fails then either global is nullptr or not
  // in a usable state.
  AutoJSAPI jsapi;
  if (!jsapi.Init(globalObject)) {
    ref = ClientOpPromise::CreateAndResolve(NS_OK, __func__);
    return ref.forget();
  }

  JSContext* cx = jsapi.cx();

  ErrorResult result;
  JS::Rooted<JS::Value> messageData(cx);
  clonedData.Read(cx, &messageData, result);
  if (result.MaybeSetPendingException(cx)) {
    // We reported the error in the current window context.  Resolve
    // promise instead of rejecting.
    ref = ClientOpPromise::CreateAndResolve(NS_OK, __func__);
    return ref.forget();
  }

  RootedDictionary<MessageEventInit> init(cx);

  init.mData = messageData;
  if (!clonedData.TakeTransferredPortsAsSequence(init.mPorts)) {
    // Report the error in the current window context and resolve the
    // promise instead of rejecting.
    xpc::Throw(cx, NS_ERROR_OUT_OF_MEMORY);
    ref = ClientOpPromise::CreateAndResolve(NS_OK, __func__);
    return ref.forget();
  }

  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrincipal> principal =
    PrincipalInfoToPrincipal(principalInfo, &rv);
  if (NS_FAILED(rv) || !principal) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    return ref.forget();
  }

  nsAutoCString origin;
  rv = principal->GetOriginNoSuffix(origin);
  if (NS_SUCCEEDED(rv)) {
    CopyUTF8toUTF16(origin, init.mOrigin);
  }

  RefPtr<ServiceWorker> instance;

  if (ServiceWorkerParentInterceptEnabled()) {
    instance = globalObject->GetOrCreateServiceWorker(source);
  } else {
    // If we are in legacy child-side intercept mode then we need to verify
    // this registration exists in the current process.
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (!swm) {
      // Shutting down. Just don't deliver this message.
      ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
      return ref.forget();
    }

    RefPtr<ServiceWorkerRegistrationInfo> reg =
      swm->GetRegistration(principal, source.Scope());
    if (reg) {
      instance = globalObject->GetOrCreateServiceWorker(source);
    }
  }

  if (instance) {
    init.mSource.SetValue().SetAsServiceWorker() = instance;
  }

  RefPtr<MessageEvent> event =
    MessageEvent::Constructor(target, NS_LITERAL_STRING("message"), init);
  event->SetTrusted(true);

  target->DispatchEvent(*event, result);
  if (result.Failed()) {
    result.SuppressException();
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
    return ref.forget();
  }

  ref = ClientOpPromise::CreateAndResolve(NS_OK, __func__);
  return ref.forget();
}

RefPtr<ClientOpPromise>
ClientSource::Claim(const ClientClaimArgs& aArgs)
{
  // The ClientSource::Claim method is only needed in the legacy
  // mode where the ServiceWorkerManager is run in each child-process.
  // In parent-process mode this method should not be called.
  MOZ_DIAGNOSTIC_ASSERT(!ServiceWorkerParentInterceptEnabled());

  RefPtr<ClientOpPromise> ref;

  nsIGlobalObject* global = GetGlobal();
  if (NS_WARN_IF(!global)) {
    ref = ClientOpPromise::CreateAndReject(NS_ERROR_DOM_INVALID_STATE_ERR,
                                           __func__);
    return ref.forget();
  }

  // Note, we cannot just mark the ClientSource controlled.  We must go through
  // the SWM so that it can keep track of which clients are controlled by each
  // registration.  We must tell the child-process SWM in legacy child-process
  // mode.  In parent-process service worker mode the SWM is notified in the
  // parent-process in ClientManagerService::Claim().

  RefPtr<GenericPromise::Private> innerPromise =
    new GenericPromise::Private(__func__);
  ServiceWorkerDescriptor swd(aArgs.serviceWorker());

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
    "ClientSource::Claim",
    [innerPromise, clientInfo = mClientInfo, swd] () mutable {
      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (NS_WARN_IF(!swm)) {
        innerPromise->Reject(NS_ERROR_DOM_INVALID_STATE_ERR, __func__);
        return;
      }

      RefPtr<GenericPromise> p = swm->MaybeClaimClient(clientInfo, swd);
      p->ChainTo(innerPromise.forget(), __func__);
    });

  if (NS_IsMainThread()) {
    r->Run();
  } else {
    MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
  }

  RefPtr<ClientOpPromise::Private> outerPromise =
    new ClientOpPromise::Private(__func__);

  auto holder = MakeRefPtr<DOMMozPromiseRequestHolder<GenericPromise>>(global);

  innerPromise->Then(mEventTarget, __func__,
    [outerPromise, holder] (bool aResult) {
      holder->Complete();
      outerPromise->Resolve(NS_OK, __func__);
    }, [outerPromise, holder] (nsresult aResult) {
      holder->Complete();
      outerPromise->Reject(aResult, __func__);
    })->Track(*holder);

  ref = outerPromise;
  return ref.forget();
}

RefPtr<ClientOpPromise>
ClientSource::GetInfoAndState(const ClientGetInfoAndStateArgs& aArgs)
{
  RefPtr<ClientOpPromise> ref;

  ClientState state;
  nsresult rv = SnapshotState(&state);
  if (NS_FAILED(rv)) {
    ref = ClientOpPromise::CreateAndReject(rv, __func__);
    return ref.forget();
  }

  ref = ClientOpPromise::CreateAndResolve(ClientInfoAndState(mClientInfo.ToIPC(),
                                                             state.ToIPC()), __func__);
  return ref.forget();
}

nsresult
ClientSource::SnapshotState(ClientState* aStateOut)
{
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_DIAGNOSTIC_ASSERT(aStateOut);

  if (mClientInfo.Type() == ClientType::Window) {
    MaybeCreateInitialDocument();
    nsresult rv = SnapshotWindowState(aStateOut);
    if (NS_FAILED(rv)) {
      return rv;
    }
    return NS_OK;
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivate();
  if (!workerPrivate) {
    return NS_ERROR_DOM_INVALID_STATE_ERR;
  }

  // Workers only keep a boolean for storage access at the moment.
  // Map this back to eAllow or eDeny for now.
  nsContentUtils::StorageAccess storage =
    workerPrivate->IsStorageAllowed() ? nsContentUtils::StorageAccess::eAllow
                                      : nsContentUtils::StorageAccess::eDeny;

  *aStateOut = ClientState(ClientWorkerState(storage));
  return NS_OK;
}

nsISerialEventTarget*
ClientSource::EventTarget() const
{
  return mEventTarget;
}

void
ClientSource::Traverse(nsCycleCollectionTraversalCallback& aCallback,
                       const char* aName,
                       uint32_t aFlags)
{
  if (mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    ImplCycleCollectionTraverse(aCallback,
                                mOwner.as<RefPtr<nsPIDOMWindowInner>>(),
                                aName, aFlags);
  } else if (mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    ImplCycleCollectionTraverse(aCallback,
                                mOwner.as<nsCOMPtr<nsIDocShell>>(),
                                aName, aFlags);
  }
}

void
ClientSource::NoteCalledRegisterForServiceWorkerScope(const nsACString& aScope)
{
  if (mRegisteringScopeList.Contains(aScope)) {
    return;
  }
  mRegisteringScopeList.AppendElement(aScope);
}

bool
ClientSource::CalledRegisterForServiceWorkerScope(const nsACString& aScope)
{
  return mRegisteringScopeList.Contains(aScope);
}

} // namespace dom
} // namespace mozilla
