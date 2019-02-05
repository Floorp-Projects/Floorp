/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerChild.h"
#include "RemoteWorkerService.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/ServiceWorkerInterceptController.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "mozilla/dom/WorkerError.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIConsoleReportCollector.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"

namespace mozilla {

using namespace ipc;

namespace dom {

using workerinternals::ChannelFromScriptURLMainThread;

namespace {

nsresult PopulateContentSecurityPolicy(
    nsIContentSecurityPolicy* aCSP,
    const nsTArray<ContentSecurityPolicy>& aPolicies) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCSP);
  MOZ_ASSERT(!aPolicies.IsEmpty());

  for (const ContentSecurityPolicy& policy : aPolicies) {
    nsresult rv = aCSP->AppendPolicy(policy.policy(), policy.reportOnlyFlag(),
                                     policy.deliveredViaMetaTagFlag());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult PopulatePrincipalContentSecurityPolicy(
    nsIPrincipal* aPrincipal, const nsTArray<ContentSecurityPolicy>& aPolicies,
    const nsTArray<ContentSecurityPolicy>& aPreloadPolicies) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  if (!aPolicies.IsEmpty()) {
    nsCOMPtr<nsIContentSecurityPolicy> csp;
    aPrincipal->EnsureCSP(nullptr, getter_AddRefs(csp));
    nsresult rv = PopulateContentSecurityPolicy(csp, aPolicies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!aPreloadPolicies.IsEmpty()) {
    nsCOMPtr<nsIContentSecurityPolicy> preloadCsp;
    aPrincipal->EnsurePreloadCSP(nullptr, getter_AddRefs(preloadCsp));
    nsresult rv = PopulateContentSecurityPolicy(preloadCsp, aPreloadPolicies);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

class SharedWorkerInterfaceRequestor final : public nsIInterfaceRequestor {
 public:
  NS_DECL_ISUPPORTS

  SharedWorkerInterfaceRequestor() {
    // This check must match the code nsDocShell::Create.
    if (!ServiceWorkerParentInterceptEnabled() || XRE_IsParentProcess()) {
      mSWController = new ServiceWorkerInterceptController();
    }
  }

  NS_IMETHOD
  GetInterface(const nsIID& aIID, void** aSink) override {
    MOZ_ASSERT(NS_IsMainThread());

    if (mSWController &&
        aIID.Equals(NS_GET_IID(nsINetworkInterceptController))) {
      // If asked for the network intercept controller, ask the outer requestor,
      // which could be the docshell.
      RefPtr<ServiceWorkerInterceptController> swController = mSWController;
      swController.forget(aSink);
      return NS_OK;
    }

    return NS_NOINTERFACE;
  }

 private:
  ~SharedWorkerInterfaceRequestor() = default;

  RefPtr<ServiceWorkerInterceptController> mSWController;
};

NS_IMPL_ADDREF(SharedWorkerInterfaceRequestor)
NS_IMPL_RELEASE(SharedWorkerInterfaceRequestor)
NS_IMPL_QUERY_INTERFACE(SharedWorkerInterfaceRequestor, nsIInterfaceRequestor)

// Normal runnable because AddPortIdentifier() is going to exec JS code.
class MessagePortIdentifierRunnable final : public WorkerRunnable {
 public:
  MessagePortIdentifierRunnable(WorkerPrivate* aWorkerPrivate,
                                RemoteWorkerChild* aActor,
                                const MessagePortIdentifier& aPortIdentifier)
      : WorkerRunnable(aWorkerPrivate),
        mActor(aActor),
        mPortIdentifier(aPortIdentifier) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    mActor->AddPortIdentifier(aCx, aWorkerPrivate, mPortIdentifier);
    return true;
  }

  nsresult Cancel() override {
    MessagePort::ForceClose(mPortIdentifier);
    return WorkerRunnable::Cancel();
  }

  virtual bool PreDispatch(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  virtual void PostDispatch(WorkerPrivate* aWorkerPrivate,
                            bool aDispatchResult) override {
    // Silence bad assertions.
  }

  bool PreRun(WorkerPrivate* aWorkerPrivate) override {
    // Silence bad assertions.
    return true;
  }

  void PostRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate,
               bool aRunResult) override {
    // Silence bad assertions.
    return;
  }

  RefPtr<RemoteWorkerChild> mActor;
  MessagePortIdentifier mPortIdentifier;
};

}  // namespace

class RemoteWorkerChild::InitializeWorkerRunnable final
    : public WorkerRunnable {
 public:
  InitializeWorkerRunnable(WorkerPrivate* aWorkerPrivate,
                           RemoteWorkerChild* aActor)
      : WorkerRunnable(aWorkerPrivate), mActor(aActor) {}

 private:
  virtual bool WorkerRun(JSContext* aCx,
                         WorkerPrivate* aWorkerPrivate) override {
    mActor->InitializeOnWorker(aWorkerPrivate);
    return true;
  }

  nsresult Cancel() override {
    mActor->CreationFailedOnAnyThread();
    mActor->ShutdownOnWorker();
    return WorkerRunnable::Cancel();
  }

  RefPtr<RemoteWorkerChild> mActor;
};

RemoteWorkerChild::RemoteWorkerChild()
    : mIPCActive(true), mWorkerState(ePending) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
}

RemoteWorkerChild::~RemoteWorkerChild() {
  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("RemoteWorkerChild::mWorkerPrivate", target,
                  mWorkerPrivate.forget());
}

void RemoteWorkerChild::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCActive = false;
  mPendingOps.Clear();
}

void RemoteWorkerChild::ExecWorker(const RemoteWorkerData& aData) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
  MOZ_ASSERT(mIPCActive);

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerChild::ExecWorker", [self, aData]() {
        nsresult rv = self->ExecWorkerOnMainThread(aData);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          self->CreationFailedOnAnyThread();
        }
      });

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

nsresult RemoteWorkerChild::ExecWorkerOnMainThread(
    const RemoteWorkerData& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that the IndexedDatabaseManager is initialized
  Unused << NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate());

  nsresult rv = NS_OK;

  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aData.principalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = PopulatePrincipalContentSecurityPolicy(principal, aData.principalCsp(),
                                              aData.principalPreloadCsp());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> loadingPrincipal =
      PrincipalInfoToPrincipal(aData.loadingPrincipalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = PopulatePrincipalContentSecurityPolicy(
      loadingPrincipal, aData.loadingPrincipalCsp(),
      aData.loadingPrincipalPreloadCsp());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  WorkerLoadInfo info;
  info.mBaseURI = DeserializeURI(aData.baseScriptURL());
  info.mResolvedScriptURI = DeserializeURI(aData.resolvedScriptURL());

  info.mPrincipalInfo = new PrincipalInfo(aData.principalInfo());

  info.mDomain = aData.domain();
  info.mPrincipal = principal;
  info.mLoadingPrincipal = loadingPrincipal;
  info.mStorageAllowed = aData.isStorageAccessAllowed();
  info.mOriginAttributes =
      BasePrincipal::Cast(principal)->OriginAttributesRef();

  // Default CSP permissions for now.  These will be overrided if necessary
  // based on the script CSP headers during load in ScriptLoader.
  info.mEvalAllowed = true;
  info.mReportCSPViolations = false;
  info.mSecureContext = aData.isSecureContext()
                            ? WorkerLoadInfo::eSecureContext
                            : WorkerLoadInfo::eInsecureContext;

  WorkerPrivate::OverrideLoadInfoLoadGroup(info, info.mLoadingPrincipal);

  RefPtr<SharedWorkerInterfaceRequestor> requestor =
      new SharedWorkerInterfaceRequestor();
  info.mInterfaceRequestor->SetOuterRequestor(requestor);

  rv = info.SetPrincipalOnMainThread(info.mPrincipal, info.mLoadGroup);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  Maybe<ClientInfo> clientInfo;
  if (aData.clientInfo().type() == OptionalIPCClientInfo::TIPCClientInfo) {
    clientInfo.emplace(ClientInfo(aData.clientInfo().get_IPCClientInfo()));
  }

  // Top level workers' main script use the document charset for the script
  // uri encoding.
  rv = ChannelFromScriptURLMainThread(
      info.mLoadingPrincipal, nullptr /* parent document */, info.mLoadGroup,
      info.mResolvedScriptURI, clientInfo,
      aData.isSharedWorker() ? nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER
                             : nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER,
      getter_AddRefs(info.mChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  mWorkerPrivate = WorkerPrivate::Constructor(
      jsapi.cx(), aData.originalScriptURL(), false,
      aData.isSharedWorker() ? WorkerTypeShared : WorkerTypeService,
      aData.name(), VoidCString(), &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  RefPtr<InitializeWorkerRunnable> runnable =
      new InitializeWorkerRunnable(mWorkerPrivate, this);
  if (NS_WARN_IF(!runnable->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  mWorkerPrivate->SetRemoteWorkerController(this);
  return NS_OK;
}

void RemoteWorkerChild::InitializeOnWorker(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<RemoteWorkerChild> self = this;
  mWorkerRef = WeakWorkerRef::Create(mWorkerPrivate,
                                     [self]() { self->ShutdownOnWorker(); });

  if (NS_WARN_IF(!mWorkerRef)) {
    CreationFailedOnAnyThread();
    ShutdownOnWorker();
    return;
  }

  CreationSucceededOnAnyThread();
}

void RemoteWorkerChild::ShutdownOnWorker() {
  MOZ_ASSERT(mWorkerPrivate);
  mWorkerPrivate->AssertIsOnWorkerThread();

  // This will release the worker.
  mWorkerRef = nullptr;

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("RemoteWorkerChild::mWorkerPrivate", target,
                  mWorkerPrivate.forget());

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerChild::ShutdownOnWorker",
                             [self]() { self->WorkerTerminated(); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::WorkerTerminated() {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  mWorkerState = eTerminated;
  mPendingOps.Clear();

  if (!mIPCActive) {
    return;
  }

  Unused << SendClose();
  mIPCActive = false;
}

void RemoteWorkerChild::ErrorPropagationDispatch(nsresult aError) {
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "RemoteWorkerChild::ErrorPropagationDispatch",
      [self, aError]() { self->ErrorPropagation(aError); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::ErrorPropagationOnMainThread(
    const WorkerErrorReport* aReport, bool aIsErrorEvent) {
  MOZ_ASSERT(NS_IsMainThread());

  ErrorValue value;
  if (aIsErrorEvent) {
    nsTArray<ErrorDataNote> notes;
    for (size_t i = 0, len = aReport->mNotes.Length(); i < len; i++) {
      const WorkerErrorNote& note = aReport->mNotes.ElementAt(i);
      notes.AppendElement(ErrorDataNote(note.mLineNumber, note.mColumnNumber,
                                        note.mMessage, note.mFilename));
    }

    ErrorData data(aReport->mLineNumber, aReport->mColumnNumber,
                   aReport->mFlags, aReport->mMessage, aReport->mFilename,
                   aReport->mLine, notes);
    value = data;
  } else {
    value = void_t();
  }

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "RemoteWorkerChild::ErrorPropagationOnMainThread",
      [self, value]() { self->ErrorPropagation(value); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::ErrorPropagation(const ErrorValue& aValue) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  if (!mIPCActive) {
    return;
  }

  Unused << SendError(aValue);
}

void RemoteWorkerChild::CloseWorkerOnMainThread() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mWorkerState == ePending) {
    mWorkerState = ePendingTerminated;
    // Already released.
    return;
  }

  // The holder will be notified by this.
  if (mWorkerState == eRunning) {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->Cancel();
  }
}

void RemoteWorkerChild::FlushReportsOnMainThread(
    nsIConsoleReportCollector* aReporter) {
  MOZ_ASSERT(NS_IsMainThread());

  bool reportErrorToBrowserConsole = true;

  // Flush the reports.
  for (uint32_t i = 0, len = mWindowIDs.Length(); i < len; ++i) {
    aReporter->FlushReportsToConsole(
        mWindowIDs[i], nsIConsoleReportCollector::ReportAction::Save);
    reportErrorToBrowserConsole = false;
  }

  // Finally report to browser console if there is no any window.
  if (reportErrorToBrowserConsole) {
    aReporter->FlushReportsToConsole(0);
    return;
  }

  aReporter->ClearConsoleReports();
}

IPCResult RemoteWorkerChild::RecvExecOp(const RemoteWorkerOp& aOp) {
  if (!mIPCActive) {
    return IPC_OK();
  }

  // The worker is not ready yet.
  if (mWorkerState == ePending) {
    mPendingOps.AppendElement(aOp);
    return IPC_OK();
  }

  if (mWorkerState == eTerminated || mWorkerState == ePendingTerminated) {
    // No op.
    return IPC_OK();
  }

  MOZ_ASSERT(mWorkerState == eRunning);

  // Main-thread operations
  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerSuspendOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerResumeOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerFreezeOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerThawOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerTerminateOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerAddWindowIDOp ||
      aOp.type() == RemoteWorkerOp::TRemoteWorkerRemoveWindowIDOp) {
    RefPtr<RemoteWorkerChild> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        "RemoteWorkerChild::RecvExecOp",
        [self, aOp]() { self->RecvExecOpOnMainThread(aOp); });

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);
    target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    return IPC_OK();
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerPortIdentifierOp) {
    const RemoteWorkerPortIdentifierOp& op =
        aOp.get_RemoteWorkerPortIdentifierOp();
    RefPtr<MessagePortIdentifierRunnable> runnable =
        new MessagePortIdentifierRunnable(mWorkerPrivate, this,
                                          op.portIdentifier());
    if (NS_WARN_IF(!runnable->Dispatch())) {
      ErrorPropagation(NS_ERROR_FAILURE);
    }
    return IPC_OK();
  }

  MOZ_CRASH("Unknown operation.");

  return IPC_OK();
}

void RemoteWorkerChild::RecvExecOpOnMainThread(const RemoteWorkerOp& aOp) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerSuspendOp) {
    if (mWorkerPrivate) {
      mWorkerPrivate->ParentWindowPaused();
    }
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerResumeOp) {
    if (mWorkerPrivate) {
      mWorkerPrivate->ParentWindowResumed();
    }
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerFreezeOp) {
    if (mWorkerPrivate) {
      mWorkerPrivate->Freeze(nullptr);
    }
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerThawOp) {
    if (mWorkerPrivate) {
      mWorkerPrivate->Thaw(nullptr);
    }
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerTerminateOp) {
    CloseWorkerOnMainThread();
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerAddWindowIDOp) {
    mWindowIDs.AppendElement(aOp.get_RemoteWorkerAddWindowIDOp().windowID());
    return;
  }

  if (aOp.type() == RemoteWorkerOp::TRemoteWorkerRemoveWindowIDOp) {
    mWindowIDs.RemoveElement(aOp.get_RemoteWorkerRemoveWindowIDOp().windowID());
    return;
  }

  MOZ_CRASH("No other operations should be scheduled on main-thread.");
}

void RemoteWorkerChild::AddPortIdentifier(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate,
    const MessagePortIdentifier& aPortIdentifier) {
  if (NS_WARN_IF(!aWorkerPrivate->ConnectMessagePort(aCx, aPortIdentifier))) {
    ErrorPropagationDispatch(NS_ERROR_FAILURE);
  }
}

void RemoteWorkerChild::CreationSucceededOnAnyThread() {
  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerChild::CreationSucceededOnAnyThread",
                             [self]() { self->CreationSucceeded(); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::CreationSucceeded() {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  // The worker is created but we need to terminate it already.
  if (mWorkerState == ePendingTerminated) {
    RefPtr<RemoteWorkerChild> self = this;
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("RemoteWorkerChild::CreationSucceeded",
                               [self]() { self->CloseWorkerOnMainThread(); });

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);
    target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    return;
  }

  mWorkerState = eRunning;

  if (!mIPCActive) {
    return;
  }

  for (const RemoteWorkerOp& op : mPendingOps) {
    RecvExecOp(op);
  }

  mPendingOps.Clear();

  Unused << SendCreated(true);
}

void RemoteWorkerChild::CreationFailedOnAnyThread() {
  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerChild::CreationFailedOnAnyThread",
                             [self]() { self->CreationFailed(); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::CreationFailed() {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());

  mWorkerState = eTerminated;
  mPendingOps.Clear();

  if (!mIPCActive) {
    return;
  }

  Unused << SendCreated(false);
}

}  // namespace dom
}  // namespace mozilla
