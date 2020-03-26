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
#include "mozilla/dom/BlobURLProtocolHandler.h"
#include "mozilla/dom/ClientIPCTypes.h"
#include "mozilla/dom/DOMMozPromiseRequestHolder.h"
#include "mozilla/dom/ipc/StructuredCloneData.h"
#include "mozilla/dom/JSExecutionManager.h"
#include "mozilla/dom/MessageEvent.h"
#include "mozilla/dom/MessageEventBinding.h"
#include "mozilla/dom/Navigator.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/ServiceWorkerContainer.h"
#include "mozilla/dom/ServiceWorkerManager.h"
#include "mozilla/StorageAccess.h"
#include "nsIContentSecurityPolicy.h"
#include "nsContentUtils.h"
#include "nsFocusManager.h"
#include "nsIDocShell.h"
#include "nsPIDOMWindow.h"

#include "mozilla/ipc/BackgroundUtils.h"

namespace mozilla {
namespace dom {

using mozilla::dom::ipc::StructuredCloneData;
using mozilla::ipc::CSPInfo;
using mozilla::ipc::CSPToCSPInfo;
using mozilla::ipc::PrincipalInfo;
using mozilla::ipc::PrincipalInfoToPrincipal;

void ClientSource::Shutdown() {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (IsShutdown()) {
    return;
  }

  ShutdownThing();

  mManager = nullptr;
}

void ClientSource::ExecutionReady(const ClientSourceExecutionReadyArgs& aArgs) {
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

Result<ClientState, ErrorResult> ClientSource::SnapshotWindowState() {
  MOZ_ASSERT(NS_IsMainThread());

  nsPIDOMWindowInner* window = GetInnerWindow();
  if (!window || !window->IsCurrentInnerWindow() ||
      !window->HasActiveDocument()) {
    return ClientState(ClientWindowState(VisibilityState::Hidden, TimeStamp(),
                                         StorageAccess::eDeny, false));
  }

  Document* doc = window->GetExtantDoc();
  ErrorResult rv;
  if (NS_WARN_IF(!doc)) {
    rv.ThrowInvalidStateError("Document not active");
    return Err(std::move(rv));
  }

  bool focused = doc->HasFocus(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return Err(std::move(rv));
  }

  StorageAccess storage = StorageAllowedForDocument(doc);

  return ClientState(ClientWindowState(doc->VisibilityState(),
                                       doc->LastFocusTime(), storage, focused));
}

WorkerPrivate* ClientSource::GetWorkerPrivate() const {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<WorkerPrivate*>()) {
    return nullptr;
  }
  return mOwner.as<WorkerPrivate*>();
}

nsIDocShell* ClientSource::GetDocShell() const {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    return nullptr;
  }
  return mOwner.as<nsCOMPtr<nsIDocShell>>();
}

nsIGlobalObject* ClientSource::GetGlobal() const {
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

void ClientSource::MaybeCreateInitialDocument() {
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
    : mManager(aManager),
      mEventTarget(aEventTarget),
      mOwner(AsVariant(Nothing())),
      mClientInfo(aArgs.id(), aArgs.type(), aArgs.principalInfo(),
                  aArgs.creationTime()) {
  MOZ_ASSERT(mManager);
  MOZ_ASSERT(mEventTarget);
}

void ClientSource::Activate(PClientManagerChild* aActor) {
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

ClientSource::~ClientSource() { Shutdown(); }

nsPIDOMWindowInner* ClientSource::GetInnerWindow() const {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    return nullptr;
  }
  return mOwner.as<RefPtr<nsPIDOMWindowInner>>();
}

void ClientSource::WorkerExecutionReady(WorkerPrivate* aWorkerPrivate) {
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
    MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate->StorageAccess() >
                              StorageAccess::ePrivateBrowsing ||
                          StringBeginsWith(aWorkerPrivate->ScriptURL(),
                                           NS_LITERAL_STRING("blob:")));
  }

  // Its safe to store the WorkerPrivate* here because the ClientSource
  // is explicitly destroyed by WorkerPrivate before exiting its run loop.
  MOZ_DIAGNOSTIC_ASSERT(mOwner.is<Nothing>());
  mOwner = AsVariant(aWorkerPrivate);

  ClientSourceExecutionReadyArgs args(aWorkerPrivate->GetLocationInfo().mHref,
                                      FrameType::None);

  ExecutionReady(args);
}

nsresult ClientSource::WindowExecutionReady(nsPIDOMWindowInner* aInnerWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow);
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->IsCurrentInnerWindow());
  MOZ_DIAGNOSTIC_ASSERT(aInnerWindow->HasActiveDocument());

  if (IsShutdown()) {
    return NS_OK;
  }

  Document* doc = aInnerWindow->GetExtantDoc();
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
                          StorageAllowedForWindow(aInnerWindow) ==
                              StorageAccess::eAllow);
  }

  nsPIDOMWindowOuter* outer = aInnerWindow->GetOuterWindow();
  NS_ENSURE_TRUE(outer, NS_ERROR_UNEXPECTED);

  FrameType frameType = FrameType::Top_level;
  if (!outer->IsTopLevelWindow()) {
    frameType = FrameType::Nested;
  } else if (outer->HadOriginalOpener()) {
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

nsresult ClientSource::DocShellExecutionReady(nsIDocShell* aDocShell) {
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
  } else if (outer->HadOriginalOpener()) {
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

void ClientSource::Freeze() {
  MaybeExecute([](PClientSourceChild* aActor) { aActor->SendFreeze(); });
}

void ClientSource::Thaw() {
  MaybeExecute([](PClientSourceChild* aActor) { aActor->SendThaw(); });
}

const ClientInfo& ClientSource::Info() const { return mClientInfo; }

void ClientSource::WorkerSyncPing(WorkerPrivate* aWorkerPrivate) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate);

  if (IsShutdown()) {
    return;
  }

  // We need to make sure the mainthread is unblocked.
  AutoYieldJSThreadExecution yield;

  MOZ_DIAGNOSTIC_ASSERT(aWorkerPrivate == mManager->GetWorkerPrivate());
  aWorkerPrivate->AssertIsOnWorkerThread();
  MOZ_DIAGNOSTIC_ASSERT(GetActor());

  GetActor()->SendWorkerSyncPing();
}

void ClientSource::SetController(
    const ServiceWorkerDescriptor& aServiceWorker) {
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
    MOZ_DIAGNOSTIC_ASSERT(
        Info().URL().LowerCaseEqualsLiteral("about:blank") ||
        StringBeginsWith(Info().URL(), NS_LITERAL_CSTRING("blob:")) ||
        StorageAllowedForWindow(GetInnerWindow()) == StorageAccess::eAllow);
  } else if (GetWorkerPrivate()) {
    MOZ_DIAGNOSTIC_ASSERT(GetWorkerPrivate()->StorageAccess() >
                              StorageAccess::ePrivateBrowsing ||
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

RefPtr<ClientOpPromise> ClientSource::Control(
    const ClientControlledArgs& aArgs) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // Determine if the client is allowed to be controlled.  Currently we
  // prevent service workers from controlling clients that cannot access
  // storage.  We exempt this restriction for local URL clients, like
  // about:blank and blob:, since access to service workers is dictated by their
  // parent.
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
    controlAllowed =
        Info().URL().LowerCaseEqualsLiteral("about:blank") ||
        StringBeginsWith(Info().URL(), NS_LITERAL_CSTRING("blob:")) ||
        StorageAllowedForWindow(GetInnerWindow()) == StorageAccess::eAllow;
  } else if (GetWorkerPrivate()) {
    // Local URL workers and workers with access to storage cna be controlled.
    controlAllowed =
        GetWorkerPrivate()->StorageAccess() > StorageAccess::ePrivateBrowsing ||
        StringBeginsWith(GetWorkerPrivate()->ScriptURL(),
                         NS_LITERAL_STRING("blob:"));
  }

  if (NS_WARN_IF(!controlAllowed)) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Client cannot be controlled");
    return ClientOpPromise::CreateAndReject(rv, __func__);
  }

  SetController(ServiceWorkerDescriptor(aArgs.serviceWorker()));

  return ClientOpPromise::CreateAndResolve(CopyableErrorResult(), __func__);
}

void ClientSource::InheritController(
    const ServiceWorkerDescriptor& aServiceWorker) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // If we are in legacy child-side intercept mode then we must tell the current
  // process SWM that this client inherited a controller.  This will only update
  // the local SWM data and not send any messages to the ClientManagerService.
  if (!ServiceWorkerParentInterceptEnabled()) {
    if (GetDocShell()) {
      AssertIsOnMainThread();

      RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
      if (swm) {
        swm->NoteInheritedController(mClientInfo, aServiceWorker);
      }
    } else {
      WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
      MOZ_ASSERT(workerPrivate);

      RefPtr<StrongWorkerRef> strongWorkerRef = StrongWorkerRef::Create(
          workerPrivate,
          NS_ConvertUTF16toUTF8(workerPrivate->WorkerName()).get());
      auto threadSafeWorkerRef =
          MakeRefPtr<ThreadSafeWorkerRef>(strongWorkerRef);

      nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
          __func__, [workerRef = threadSafeWorkerRef, clientInfo = mClientInfo,
                     serviceWorker = aServiceWorker]() {
            MOZ_ASSERT(IsBlobURI(workerRef->Private()->GetBaseURI()));

            RefPtr<ServiceWorkerManager> swm =
                ServiceWorkerManager::GetInstance();

            if (swm) {
              swm->NoteInheritedController(clientInfo, serviceWorker);
            }
          });

      Unused << NS_WARN_IF(
          NS_FAILED(workerPrivate->DispatchToMainThread(r.forget())));
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

const Maybe<ServiceWorkerDescriptor>& ClientSource::GetController() const {
  return mController;
}

void ClientSource::NoteDOMContentLoaded() {
  if (mController.isSome() && !ServiceWorkerParentInterceptEnabled()) {
    AssertIsOnMainThread();
    RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
    if (swm) {
      swm->MaybeCheckNavigationUpdate(mClientInfo);
    }
  }

  MaybeExecute(
      [](PClientSourceChild* aActor) { aActor->SendNoteDOMContentLoaded(); });
}

RefPtr<ClientOpPromise> ClientSource::Focus(const ClientFocusArgs& aArgs) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  if (mClientInfo.Type() != ClientType::Window) {
    CopyableErrorResult rv;
    rv.ThrowNotSupportedError("Not a Window client");
    return ClientOpPromise::CreateAndReject(rv, __func__);
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
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Browsing context discarded");
    return ClientOpPromise::CreateAndReject(rv, __func__);
  }

  MOZ_ASSERT(NS_IsMainThread());
  nsFocusManager::FocusWindow(outer, aArgs.callerType());

  Result<ClientState, ErrorResult> state = SnapshotState();
  if (state.isErr()) {
    return ClientOpPromise::CreateAndReject(
        CopyableErrorResult(state.unwrapErr()), __func__);
  }

  return ClientOpPromise::CreateAndResolve(state.inspect().ToIPC(), __func__);
}

RefPtr<ClientOpPromise> ClientSource::PostMessage(
    const ClientPostMessageArgs& aArgs) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  // TODO: Currently this function only supports clients whose global
  // object is a Window; it should also support those whose global
  // object is a WorkerGlobalScope.
  if (nsPIDOMWindowInner* const window = GetInnerWindow()) {
    const RefPtr<ServiceWorkerContainer> container =
        window->Navigator()->ServiceWorker();
    container->ReceiveMessage(aArgs);
    return ClientOpPromise::CreateAndResolve(CopyableErrorResult(), __func__);
  }

  CopyableErrorResult rv;
  rv.ThrowNotSupportedError(
      "postMessage to non-Window clients is not supported yet");
  return ClientOpPromise::CreateAndReject(rv, __func__);
}

RefPtr<ClientOpPromise> ClientSource::Claim(const ClientClaimArgs& aArgs) {
  // The ClientSource::Claim method is only needed in the legacy
  // mode where the ServiceWorkerManager is run in each child-process.
  // In parent-process mode this method should not be called.
  MOZ_DIAGNOSTIC_ASSERT(!ServiceWorkerParentInterceptEnabled());

  nsIGlobalObject* global = GetGlobal();
  if (NS_WARN_IF(!global)) {
    CopyableErrorResult rv;
    rv.ThrowInvalidStateError("Browsing context torn down");
    return ClientOpPromise::CreateAndReject(rv, __func__);
  }

  // Note, we cannot just mark the ClientSource controlled.  We must go through
  // the SWM so that it can keep track of which clients are controlled by each
  // registration.  We must tell the child-process SWM in legacy child-process
  // mode.  In parent-process service worker mode the SWM is notified in the
  // parent-process in ClientManagerService::Claim().

  RefPtr<GenericErrorResultPromise::Private> innerPromise =
      new GenericErrorResultPromise::Private(__func__);
  ServiceWorkerDescriptor swd(aArgs.serviceWorker());

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "ClientSource::Claim",
      [innerPromise, clientInfo = mClientInfo, swd]() mutable {
        RefPtr<ServiceWorkerManager> swm = ServiceWorkerManager::GetInstance();
        if (NS_WARN_IF(!swm)) {
          CopyableErrorResult rv;
          rv.ThrowInvalidStateError("Browser shutting down");
          innerPromise->Reject(rv, __func__);
          return;
        }

        RefPtr<GenericErrorResultPromise> p =
            swm->MaybeClaimClient(clientInfo, swd);
        p->ChainTo(innerPromise.forget(), __func__);
      });

  if (NS_IsMainThread()) {
    r->Run();
  } else {
    MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
  }

  RefPtr<ClientOpPromise::Private> outerPromise =
      new ClientOpPromise::Private(__func__);

  auto holder =
      MakeRefPtr<DOMMozPromiseRequestHolder<GenericErrorResultPromise>>(global);

  innerPromise
      ->Then(
          mEventTarget, __func__,
          [outerPromise, holder](bool aResult) {
            holder->Complete();
            outerPromise->Resolve(CopyableErrorResult(), __func__);
          },
          [outerPromise, holder](const CopyableErrorResult& aResult) {
            holder->Complete();
            outerPromise->Reject(aResult, __func__);
          })
      ->Track(*holder);

  return outerPromise;
}

RefPtr<ClientOpPromise> ClientSource::GetInfoAndState(
    const ClientGetInfoAndStateArgs& aArgs) {
  Result<ClientState, ErrorResult> state = SnapshotState();
  if (state.isErr()) {
    return ClientOpPromise::CreateAndReject(
        CopyableErrorResult(state.unwrapErr()), __func__);
  }

  return ClientOpPromise::CreateAndResolve(
      ClientInfoAndState(mClientInfo.ToIPC(), state.inspect().ToIPC()),
      __func__);
}

Result<ClientState, ErrorResult> ClientSource::SnapshotState() {
  NS_ASSERT_OWNINGTHREAD(ClientSource);

  if (mClientInfo.Type() == ClientType::Window) {
    MaybeCreateInitialDocument();
    return SnapshotWindowState();
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivate();
  if (!workerPrivate) {
    ErrorResult rv;
    rv.ThrowInvalidStateError("Worker terminated");
    return Err(std::move(rv));
  }

  return ClientState(ClientWorkerState(workerPrivate->StorageAccess()));
}

nsISerialEventTarget* ClientSource::EventTarget() const { return mEventTarget; }

void ClientSource::SetCsp(nsIContentSecurityPolicy* aCsp) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!aCsp) {
    return;
  }

  CSPInfo cspInfo;
  nsresult rv = CSPToCSPInfo(aCsp, &cspInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mClientInfo.SetCspInfo(cspInfo);
}

void ClientSource::SetPreloadCsp(nsIContentSecurityPolicy* aPreloadCsp) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  if (!aPreloadCsp) {
    return;
  }

  CSPInfo cspPreloadInfo;
  nsresult rv = CSPToCSPInfo(aPreloadCsp, &cspPreloadInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }
  mClientInfo.SetPreloadCspInfo(cspPreloadInfo);
}

void ClientSource::SetCspInfo(const CSPInfo& aCSPInfo) {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  mClientInfo.SetCspInfo(aCSPInfo);
}

const Maybe<mozilla::ipc::CSPInfo>& ClientSource::GetCspInfo() {
  NS_ASSERT_OWNINGTHREAD(ClientSource);
  return mClientInfo.GetCspInfo();
}

void ClientSource::Traverse(nsCycleCollectionTraversalCallback& aCallback,
                            const char* aName, uint32_t aFlags) {
  if (mOwner.is<RefPtr<nsPIDOMWindowInner>>()) {
    ImplCycleCollectionTraverse(
        aCallback, mOwner.as<RefPtr<nsPIDOMWindowInner>>(), aName, aFlags);
  } else if (mOwner.is<nsCOMPtr<nsIDocShell>>()) {
    ImplCycleCollectionTraverse(aCallback, mOwner.as<nsCOMPtr<nsIDocShell>>(),
                                aName, aFlags);
  }
}

void ClientSource::NoteCalledRegisterForServiceWorkerScope(
    const nsACString& aScope) {
  if (mRegisteringScopeList.Contains(aScope)) {
    return;
  }
  mRegisteringScopeList.AppendElement(aScope);
}

bool ClientSource::CalledRegisterForServiceWorkerScope(
    const nsACString& aScope) {
  return mRegisteringScopeList.Contains(aScope);
}

}  // namespace dom
}  // namespace mozilla
