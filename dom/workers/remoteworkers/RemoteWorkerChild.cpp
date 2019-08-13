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
#include "mozilla/net/CookieSettings.h"
#include "nsIConsoleReportCollector.h"
#include "nsIPrincipal.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"

namespace mozilla {

using namespace ipc;

namespace dom {

using workerinternals::ChannelFromScriptURLMainThread;

namespace {

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
    : mIPCActive(true), mSharedData("RemoteWorkerChild::mSharedData") {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
}

RemoteWorkerChild::SharedData::SharedData() : mWorkerState(ePending) {}

RemoteWorkerChild::~RemoteWorkerChild() {
  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);

  const auto lock = mSharedData.Lock();
  NS_ProxyRelease("RemoteWorkerChild::mWorkerPrivate", target,
                  lock->mWorkerPrivate.forget());
}

void RemoteWorkerChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, data);
  mIPCActive = false;
  data->mPendingOps.Clear();
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

  nsCOMPtr<nsIPrincipal> loadingPrincipal =
      PrincipalInfoToPrincipal(aData.loadingPrincipalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIPrincipal> storagePrincipal =
      PrincipalInfoToPrincipal(aData.storagePrincipalInfo(), &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  WorkerLoadInfo info;
  info.mBaseURI = DeserializeURI(aData.baseScriptURL());
  info.mResolvedScriptURI = DeserializeURI(aData.resolvedScriptURL());

  info.mPrincipalInfo = new PrincipalInfo(aData.principalInfo());
  info.mStoragePrincipalInfo = new PrincipalInfo(aData.storagePrincipalInfo());

  info.mReferrerInfo = aData.referrerInfo();
  info.mDomain = aData.domain();
  info.mPrincipal = principal;
  info.mStoragePrincipal = storagePrincipal;
  info.mLoadingPrincipal = loadingPrincipal;
  info.mStorageAccess = aData.storageAccess();
  info.mOriginAttributes =
      BasePrincipal::Cast(principal)->OriginAttributesRef();
  info.mCookieSettings = net::CookieSettings::Create();

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

  Maybe<ClientInfo> clientInfo;
  if (aData.clientInfo().isSome()) {
    clientInfo.emplace(ClientInfo(aData.clientInfo().ref()));
  }

  if (clientInfo.isSome()) {
    Maybe<mozilla::ipc::CSPInfo> cspInfo = clientInfo.ref().GetCspInfo();
    if (cspInfo.isSome()) {
      info.mCSP = CSPInfoToCSP(cspInfo.ref(), nullptr);
      info.mCSPInfo = new CSPInfo();
      rv = CSPToCSPInfo(info.mCSP, info.mCSPInfo);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
  }

  rv = info.SetPrincipalsAndCSPOnMainThread(
      info.mPrincipal, info.mStoragePrincipal, info.mLoadGroup, info.mCSP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Top level workers' main script use the document charset for the script
  // uri encoding.
  rv = ChannelFromScriptURLMainThread(
      info.mLoadingPrincipal, nullptr /* parent document */, info.mLoadGroup,
      info.mResolvedScriptURI, clientInfo,
      aData.isSharedWorker() ? nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER
                             : nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER,
      info.mCookieSettings, info.mReferrerInfo, getter_AddRefs(info.mChannel));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  const auto lock = mSharedData.Lock();
  lock->mWorkerPrivate = WorkerPrivate::Constructor(
      jsapi.cx(), aData.originalScriptURL(), false,
      aData.isSharedWorker() ? WorkerTypeShared : WorkerTypeService,
      aData.name(), VoidCString(), &info, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.StealNSResult();
  }

  RefPtr<InitializeWorkerRunnable> runnable =
      new InitializeWorkerRunnable(lock->mWorkerPrivate, this);
  if (NS_WARN_IF(!runnable->Dispatch())) {
    return NS_ERROR_FAILURE;
  }

  lock->mWorkerPrivate->SetRemoteWorkerController(this);
  return NS_OK;
}

void RemoteWorkerChild::InitializeOnWorker(WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  RefPtr<RemoteWorkerChild> self = this;
  {
    const auto lock = mSharedData.Lock();
    mWorkerRef = WeakWorkerRef::Create(lock->mWorkerPrivate,
                                       [self]() { self->ShutdownOnWorker(); });
  }

  if (NS_WARN_IF(!mWorkerRef)) {
    CreationFailedOnAnyThread();
    ShutdownOnWorker();
    return;
  }

  CreationSucceededOnAnyThread();
}

void RemoteWorkerChild::ShutdownOnWorker() {
  const auto lock = mSharedData.Lock();
  MOZ_ASSERT(lock->mWorkerPrivate);
  lock->mWorkerPrivate->AssertIsOnWorkerThread();

  // This will release the worker.
  mWorkerRef = nullptr;

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);

  NS_ProxyRelease("RemoteWorkerChild::mWorkerPrivate", target,
                  lock->mWorkerPrivate.forget());

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction("RemoteWorkerChild::ShutdownOnWorker",
                             [self]() { self->WorkerTerminated(); });

  RemoteWorkerService::Thread()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::WorkerTerminated() {
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, data);

  {
    const auto lock = mSharedData.Lock();
    lock->mWorkerState = eTerminated;
  }
  data->mPendingOps.Clear();

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
  const auto lock = mSharedData.Lock();

  if (lock->mWorkerState == ePending) {
    lock->mWorkerState = ePendingTerminated;
    // Already released.
    return;
  }

  // The holder will be notified by this.
  if (lock->mWorkerState == eRunning) {
    // FIXME: mWorkerState transition and each state's associated data should
    // be improved/fixed in bug 1231213. `!lock->mWorkerPrivate` implies that
    // the worker state is effectively `eTerminated.`
    if (!lock->mWorkerPrivate) {
      return;
    }

    lock->mWorkerPrivate->Cancel();
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
  const auto lock = mSharedData.Lock();
  return ExecuteOperation(aOp, lock);
}

template <typename T>
IPCResult RemoteWorkerChild::ExecuteOperation(const RemoteWorkerOp& aOp,
                                              const T& aLock) {
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, data);

  if (!mIPCActive) {
    return IPC_OK();
  }

  // The worker is not ready yet.
  if (aLock->mWorkerState == ePending) {
    data->mPendingOps.AppendElement(aOp);
    return IPC_OK();
  }

  if (aLock->mWorkerState == eTerminated ||
      aLock->mWorkerState == ePendingTerminated) {
    // No op.
    return IPC_OK();
  }

  MOZ_ASSERT(aLock->mWorkerState == eRunning);

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
        new MessagePortIdentifierRunnable(aLock->mWorkerPrivate, this,
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

  {
    const auto lock = mSharedData.Lock();

    if (aOp.type() == RemoteWorkerOp::TRemoteWorkerSuspendOp) {
      if (lock->mWorkerPrivate) {
        lock->mWorkerPrivate->ParentWindowPaused();
      }
      return;
    }

    if (aOp.type() == RemoteWorkerOp::TRemoteWorkerResumeOp) {
      if (lock->mWorkerPrivate) {
        lock->mWorkerPrivate->ParentWindowResumed();
      }
      return;
    }

    if (aOp.type() == RemoteWorkerOp::TRemoteWorkerFreezeOp) {
      if (lock->mWorkerPrivate) {
        lock->mWorkerPrivate->Freeze(nullptr);
      }
      return;
    }

    if (aOp.type() == RemoteWorkerOp::TRemoteWorkerThawOp) {
      if (lock->mWorkerPrivate) {
        lock->mWorkerPrivate->Thaw(nullptr);
      }
      return;
    }
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
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, data);

  // The worker is created but we need to terminate it already.
  const auto lock = mSharedData.Lock();
  if (lock->mWorkerState == ePendingTerminated) {
    RefPtr<RemoteWorkerChild> self = this;
    nsCOMPtr<nsIRunnable> r =
        NS_NewRunnableFunction("RemoteWorkerChild::CreationSucceeded",
                               [self]() { self->CloseWorkerOnMainThread(); });

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);
    target->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
    return;
  }

  lock->mWorkerState = eRunning;

  if (!mIPCActive) {
    return;
  }

  for (const RemoteWorkerOp& op : data->mPendingOps) {
    ExecuteOperation(op, lock);
  }

  data->mPendingOps.Clear();

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
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, data);

  const auto lock = mSharedData.Lock();
  lock->mWorkerState = eTerminated;
  data->mPendingOps.Clear();

  if (!mIPCActive) {
    return;
  }

  Unused << SendCreated(false);
}

}  // namespace dom
}  // namespace mozilla
