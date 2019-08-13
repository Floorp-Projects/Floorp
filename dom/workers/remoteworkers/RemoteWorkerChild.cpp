/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerChild.h"

#include <utility>

#include "MainThreadUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIConsoleReportCollector.h"
#include "nsIInterfaceRequestor.h"
#include "nsIPrincipal.h"
#include "nsIPermissionManager.h"
#include "nsNetUtil.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"
#include "nsXULAppAPI.h"

#include "RemoteWorkerService.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Services.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/FetchEventOpProxyChild.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/MessagePort.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/ServiceWorkerDescriptor.h"
#include "mozilla/dom/ServiceWorkerInterceptController.h"
#include "mozilla/dom/ServiceWorkerOp.h"
#include "mozilla/dom/ServiceWorkerRegistrationDescriptor.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/dom/workerinternals/ScriptLoader.h"
#include "mozilla/dom/WorkerError.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRef.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/net/CookieSettings.h"

namespace mozilla {

using namespace ipc;

namespace dom {

using workerinternals::ChannelFromScriptURLMainThread;

class SelfHolder {
 public:
  MOZ_IMPLICIT SelfHolder(RemoteWorkerChild* aSelf) : mSelf(aSelf) {
    MOZ_ASSERT(mSelf);
  }

  SelfHolder(const SelfHolder&) = default;

  SelfHolder& operator=(const SelfHolder&) = default;

  SelfHolder(SelfHolder&&) = default;

  SelfHolder& operator=(SelfHolder&&) = default;

  ~SelfHolder() {
    if (!mSelf) {
      return;
    }

    nsCOMPtr<nsISerialEventTarget> target = mSelf->GetOwningEventTarget();
    NS_ProxyRelease("SelfHolder::mSelf", target, mSelf.forget());
  }

  RemoteWorkerChild* get() const {
    MOZ_ASSERT(mSelf);

    return mSelf.get();
  }

  RemoteWorkerChild* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    return get();
  }

  bool operator!() { return !mSelf.get(); }

 private:
  RefPtr<RemoteWorkerChild> mSelf;
};

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
                                SelfHolder aActor,
                                const MessagePortIdentifier& aPortIdentifier)
      : WorkerRunnable(aWorkerPrivate),
        mActor(std::move(aActor)),
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

  SelfHolder mActor;
  MessagePortIdentifier mPortIdentifier;
};

class ReleaseWorkerRunnable final : public WorkerRunnable {
 public:
  ReleaseWorkerRunnable(RefPtr<WorkerPrivate> aWorkerPrivate,
                        already_AddRefed<WeakWorkerRef> aWeakRef)
      : WorkerRunnable(aWorkerPrivate),
        mWorkerPrivate(std::move(aWorkerPrivate)),
        mWeakRef(aWeakRef) {}

 private:
  bool WorkerRun(JSContext*, WorkerPrivate*) override {
    mWeakRef = nullptr;
    mWorkerPrivate = nullptr;

    return true;
  }

  nsresult Cancel() override {
    mWeakRef = nullptr;
    mWorkerPrivate = nullptr;

    return NS_OK;
  }

  RefPtr<WorkerPrivate> mWorkerPrivate;
  RefPtr<WeakWorkerRef> mWeakRef;
};

}  // anonymous namespace

class RemoteWorkerChild::InitializeWorkerRunnable final
    : public WorkerRunnable {
 public:
  InitializeWorkerRunnable(RefPtr<WorkerPrivate> aWorkerPrivate,
                           SelfHolder aActor)
      : WorkerRunnable(aWorkerPrivate),
        mWorkerPrivate(std::move(aWorkerPrivate)),
        mActor(std::move(aActor)) {
    MOZ_ASSERT(mWorkerPrivate);
    MOZ_ASSERT(mActor);
  }

 private:
  ~InitializeWorkerRunnable() { MaybeAbort(); }

  bool WorkerRun(JSContext*, WorkerPrivate*) override {
    mActor->InitializeOnWorker(mWorkerPrivate.forget());
    return true;
  }

  nsresult Cancel() override {
    MaybeAbort();

    return WorkerRunnable::Cancel();
  }

  // Slowly running out of synonyms for cancel, abort, terminate, etc...
  void MaybeAbort() {
    if (!mWorkerPrivate) {
      return;
    }

    nsCOMPtr<nsIEventTarget> target =
        SystemGroup::EventTargetFor(TaskCategory::Other);
    NS_ProxyRelease("InitializeWorkerRunnable::mWorkerPrivate", target,
                    mWorkerPrivate.forget());

    mActor->TransitionStateToTerminated();
    mActor->CreationFailedOnAnyThread();
    mActor->ShutdownOnWorker();
  }

  RefPtr<WorkerPrivate> mWorkerPrivate;
  SelfHolder mActor;
};

RemoteWorkerChild::RemoteWorkerChild(const RemoteWorkerData& aData)
    : mState(VariantType<Pending>(), "RemoteWorkerChild::mState"),
      mIsServiceWorker(aData.serviceWorkerData().type() ==
                       OptionalServiceWorkerData::TServiceWorkerData),
      mOwningEventTarget(GetCurrentThreadSerialEventTarget()) {
  MOZ_ASSERT(RemoteWorkerService::Thread()->IsOnCurrentThread());
  MOZ_ASSERT(mOwningEventTarget);
}

RemoteWorkerChild::~RemoteWorkerChild() {
#ifdef DEBUG
  MOZ_ASSERT(mTerminationPromise.IsEmpty());
  auto lock = mState.Lock();
  MOZ_ASSERT(lock->is<Terminated>());
#endif
}

nsISerialEventTarget* RemoteWorkerChild::GetOwningEventTarget() const {
  return mOwningEventTarget;
}

void RemoteWorkerChild::ActorDestroy(ActorDestroyReason) {
  Unused << NS_WARN_IF(!mTerminationPromise.IsEmpty());
  mTerminationPromise.RejectIfExists(NS_ERROR_DOM_ABORT_ERR, __func__);

  MOZ_ACCESS_THREAD_BOUND(mLauncherData, launcherData);
  launcherData->mIPCActive = false;

  auto lock = mState.Lock();

  Unused << NS_WARN_IF(!lock->is<Terminated>());

  if (NS_WARN_IF(lock->is<Running>())) {
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        __func__,
        [workerPrivate = std::move(lock->as<Running>().mWorkerPrivate),
         workerRef = std::move(lock->as<Running>().mWorkerRef)]() mutable {
          RefPtr<ReleaseWorkerRunnable> r =
              new ReleaseWorkerRunnable(workerPrivate, workerRef.forget());

          Unused << NS_WARN_IF(!r->Dispatch());

          workerPrivate->Cancel();
        });

    MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
  }

  *lock = VariantType<Terminated>();
}

void RemoteWorkerChild::ExecWorker(const RemoteWorkerData& aData) {
#ifdef DEBUG
  MOZ_ASSERT(GetOwningEventTarget()->IsOnCurrentThread());
  MOZ_ACCESS_THREAD_BOUND(mLauncherData, launcherData);
  MOZ_ASSERT(launcherData->mIPCActive);
#endif

  SelfHolder self = this;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [self = std::move(self), data = aData]() mutable {
        nsresult rv = self->ExecWorkerOnMainThread(std::move(data));

        if (NS_WARN_IF(NS_FAILED(rv))) {
          self->CreationFailedOnAnyThread();
        }
      });

  MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));
}

nsresult RemoteWorkerChild::ExecWorkerOnMainThread(RemoteWorkerData&& aData) {
  MOZ_ASSERT(NS_IsMainThread());

  // Ensure that the IndexedDatabaseManager is initialized
  Unused << NS_WARN_IF(!IndexedDatabaseManager::GetOrCreate());

  nsresult rv = NS_OK;

  auto scopeExit = MakeScopeExit([&] { TransitionStateToTerminated(); });

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

  if (mIsServiceWorker) {
    ServiceWorkerData& data = aData.serviceWorkerData().get_ServiceWorkerData();

    nsCOMPtr<nsIPermissionManager> permissionManager =
        services::GetPermissionManager();

    for (auto& keyAndPermissions : data.permissionsByKey()) {
      permissionManager->SetPermissionsWithKey(keyAndPermissions.key(),
                                               keyAndPermissions.permissions());
    }

    info.mServiceWorkerCacheName = data.cacheName();
    info.mServiceWorkerDescriptor.emplace(data.descriptor());
    info.mServiceWorkerRegistrationDescriptor.emplace(
        data.registrationDescriptor());
    info.mLoadFlags = static_cast<nsLoadFlags>(data.loadFlags());
  } else {
    // Top level workers' main script use the document charset for the script
    // uri encoding.
    rv = ChannelFromScriptURLMainThread(
        info.mLoadingPrincipal, nullptr /* parent document */, info.mLoadGroup,
        info.mResolvedScriptURI, clientInfo,
        nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER, info.mCookieSettings,
        info.mReferrerInfo, getter_AddRefs(info.mChannel));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  AutoJSAPI jsapi;
  jsapi.Init();

  ErrorResult error;
  RefPtr<WorkerPrivate> workerPrivate = WorkerPrivate::Constructor(
      jsapi.cx(), aData.originalScriptURL(), false,
      mIsServiceWorker ? WorkerTypeService : WorkerTypeShared, aData.name(),
      VoidCString(), &info, error);

  if (NS_WARN_IF(error.Failed())) {
    MOZ_ASSERT(!workerPrivate);

    rv = error.StealNSResult();
    return rv;
  }

  if (mIsServiceWorker) {
    RefPtr<RemoteWorkerChild> self = this;
    workerPrivate->SetRemoteWorkerControllerWeakRef(
        ThreadSafeWeakPtr<RemoteWorkerChild>(self));
  } else {
    workerPrivate->SetRemoteWorkerController(this);
  }

  RefPtr<InitializeWorkerRunnable> runnable =
      new InitializeWorkerRunnable(std::move(workerPrivate), SelfHolder(this));

  if (NS_WARN_IF(!runnable->Dispatch())) {
    rv = NS_ERROR_FAILURE;
    return rv;
  }

  scopeExit.release();

  return NS_OK;
}

void RemoteWorkerChild::InitializeOnWorker(
    already_AddRefed<WorkerPrivate> aWorkerPrivate) {
  {
    auto lock = mState.Lock();

    if (lock->is<PendingTerminated>()) {
      nsCOMPtr<nsIEventTarget> target =
          SystemGroup::EventTargetFor(TaskCategory::Other);
      NS_ProxyRelease(__func__, target, std::move(aWorkerPrivate));

      TransitionStateToTerminated(lock.ref());
      ShutdownOnWorker();
      return;
    }
  }

  RefPtr<WorkerPrivate> workerPrivate = aWorkerPrivate;
  MOZ_ASSERT(workerPrivate);
  workerPrivate->AssertIsOnWorkerThread();

  RefPtr<RemoteWorkerChild> self = this;
  ThreadSafeWeakPtr<RemoteWorkerChild> selfWeakRef(self);

  auto scopeExit = MakeScopeExit([&] {
    MOZ_ASSERT(self);

    NS_ProxyRelease(__func__, mOwningEventTarget, self.forget());
  });

  RefPtr<WeakWorkerRef> workerRef = WeakWorkerRef::Create(
      workerPrivate, [selfWeakRef = std::move(selfWeakRef)]() mutable {
        RefPtr<RemoteWorkerChild> self(selfWeakRef);

        if (NS_WARN_IF(!self)) {
          return;
        }

        self->TransitionStateToTerminated();
        self->ShutdownOnWorker();

        nsCOMPtr<nsISerialEventTarget> target = self->GetOwningEventTarget();
        NS_ProxyRelease(__func__, target, self.forget());
      });

  if (NS_WARN_IF(!workerRef)) {
    TransitionStateToTerminated();
    CreationFailedOnAnyThread();
    ShutdownOnWorker();

    return;
  }

  TransitionStateToRunning(workerPrivate.forget(), workerRef.forget());
  CreationSucceededOnAnyThread();
}

void RemoteWorkerChild::ShutdownOnWorker() {
  RefPtr<RemoteWorkerChild> self = this;

  nsCOMPtr<nsIRunnable> r =
      NS_NewRunnableFunction(__func__, [self = std::move(self)] {
        MOZ_ACCESS_THREAD_BOUND(self->mLauncherData, launcherData);

        if (!launcherData->mIPCActive) {
          return;
        }

        launcherData->mIPCActive = false;
        Unused << self->SendClose();
      });

  GetOwningEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

RefPtr<GenericNonExclusivePromise> RemoteWorkerChild::GetTerminationPromise() {
  MOZ_ASSERT(GetOwningEventTarget()->IsOnCurrentThread());

  return mTerminationPromise.Ensure(__func__);
}

void RemoteWorkerChild::CreationSucceededOnAnyThread() {
  CreationSucceededOrFailedOnAnyThread(true);
}

void RemoteWorkerChild::CreationFailedOnAnyThread() {
  CreationSucceededOrFailedOnAnyThread(false);
}

void RemoteWorkerChild::CreationSucceededOrFailedOnAnyThread(
    bool aDidCreationSucceed) {
#ifdef DEBUG
  auto lock = mState.Lock();
  MOZ_ASSERT_IF(aDidCreationSucceed, lock->is<Running>());
  MOZ_ASSERT_IF(!aDidCreationSucceed, lock->is<Terminated>());
#endif

  RefPtr<RemoteWorkerChild> self = this;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__,
      [self = std::move(self), didCreationSucceed = aDidCreationSucceed] {
        MOZ_ACCESS_THREAD_BOUND(self->mLauncherData, launcherData);

        if (!launcherData->mIPCActive) {
          return;
        }

        Unused << self->SendCreated(didCreationSucceed);
      });

  Unused << GetOwningEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::CloseWorkerOnMainThread(State& aState) {
  AssertIsOnMainThread();
  MOZ_ASSERT(!aState.is<PendingTerminated>());

  if (aState.is<Pending>()) {
    TransitionStateToPendingTerminated(aState);
    return;
  }

  // The holder will be notified by this.
  if (aState.is<Running>()) {
    aState.as<Running>().mWorkerPrivate->Cancel();
  }
}

/**
 * Error reporting method
 */
void RemoteWorkerChild::ErrorPropagation(const ErrorValue& aValue) {
  MOZ_ASSERT(GetOwningEventTarget()->IsOnCurrentThread());

  MOZ_ACCESS_THREAD_BOUND(mLauncherData, launcherData);

  if (!launcherData->mIPCActive) {
    return;
  }

  Unused << SendError(aValue);
}

void RemoteWorkerChild::ErrorPropagationDispatch(nsresult aError) {
  MOZ_ASSERT(NS_FAILED(aError));

  RefPtr<RemoteWorkerChild> self = this;
  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      "RemoteWorkerChild::ErrorPropagationDispatch",
      [self = std::move(self), aError]() { self->ErrorPropagation(aError); });

  GetOwningEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::ErrorPropagationOnMainThread(
    const WorkerErrorReport* aReport, bool aIsErrorEvent) {
  AssertIsOnMainThread();

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
      [self = std::move(self), value]() { self->ErrorPropagation(value); });

  GetOwningEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);
}

void RemoteWorkerChild::FlushReportsOnMainThread(
    nsIConsoleReportCollector* aReporter) {
  AssertIsOnMainThread();

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

/**
 * Worker state transition methods
 */
RemoteWorkerChild::Running::~Running() {
  if (!mWorkerPrivate) {
    return;
  }

  nsCOMPtr<nsIEventTarget> target =
      SystemGroup::EventTargetFor(TaskCategory::Other);
  NS_ProxyRelease("RemoteWorkerChild::Running::mWorkerPrivate", target,
                  mWorkerPrivate.forget());
}

void RemoteWorkerChild::TransitionStateToPendingTerminated(State& aState) {
  MOZ_ASSERT(aState.is<Pending>());

  CancelAllPendingOps(aState);

  aState = VariantType<PendingTerminated>();
}

void RemoteWorkerChild::TransitionStateToRunning(
    already_AddRefed<WorkerPrivate> aWorkerPrivate,
    already_AddRefed<WeakWorkerRef> aWorkerRef) {
  RefPtr<WorkerPrivate> workerPrivate = aWorkerPrivate;
  MOZ_ASSERT(workerPrivate);

  RefPtr<WeakWorkerRef> workerRef = aWorkerRef;
  MOZ_ASSERT(workerRef);

  auto lock = mState.Lock();

  MOZ_ASSERT(lock->is<Pending>());

  auto pendingOps = std::move(lock->as<Pending>().mPendingOps);

  /**
   * I'd initialize the WorkerPrivate and WeakWorkerRef in the constructor,
   * but mozilla::Variant attempts to call the thread-unsafe `AddRef()` on
   * WorkerPrivate.
   */
  *lock = VariantType<Running>();
  lock->as<Running>().mWorkerPrivate = std::move(workerPrivate);
  lock->as<Running>().mWorkerRef = std::move(workerRef);

  SelfHolder self = this;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
      __func__, [pendingOps = std::move(pendingOps), self = std::move(self)]() {
        for (auto& op : pendingOps) {
          auto lock = self->mState.Lock();

          DebugOnly<bool> started = op->MaybeStart(self.get(), lock.ref());
          MOZ_ASSERT(started);
        }
      });

  MOZ_ALWAYS_SUCCEEDS(
      mOwningEventTarget->Dispatch(r.forget(), NS_DISPATCH_NORMAL));
}

void RemoteWorkerChild::TransitionStateToTerminated() {
  auto lock = mState.Lock();

  TransitionStateToTerminated(lock.ref());
}

void RemoteWorkerChild::TransitionStateToTerminated(State& aState) {
  if (aState.is<Pending>()) {
    CancelAllPendingOps(aState);
  }

  mTerminationPromise.ResolveIfExists(true, __func__);

  aState = VariantType<Terminated>();
}

/**
 * Operation execution classes/methods
 */
class RemoteWorkerChild::SharedWorkerOp : public RemoteWorkerChild::Op {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerOp, override)

  explicit SharedWorkerOp(RemoteWorkerOp&& aOp) : mOp(std::move(aOp)) {}

  bool MaybeStart(RemoteWorkerChild* aOwner,
                  RemoteWorkerChild::State& aState) override {
    MOZ_ASSERT(!mStarted);
    MOZ_ASSERT(aOwner);

    MOZ_ACCESS_THREAD_BOUND(aOwner->mLauncherData, launcherData);

    if (NS_WARN_IF(!launcherData->mIPCActive)) {
      Unused << NS_WARN_IF(!aState.is<Terminated>());

#ifdef DEBUG
      mStarted = true;
#endif

      return true;
    }

    if (aState.is<Pending>() && !IsTerminationOp()) {
      return false;
    }

    if (aState.is<PendingTerminated>() || aState.is<Terminated>()) {
#ifdef DEBUG
      mStarted = true;
#endif

      return true;
    }

    MOZ_ASSERT(aState.is<Running>() || IsTerminationOp());

    RefPtr<SharedWorkerOp> self = this;
    SelfHolder owner = aOwner;

    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(
        __func__, [self = std::move(self), owner = std::move(owner)]() mutable {
          self->Exec(owner);
        });

    MOZ_ALWAYS_SUCCEEDS(SystemGroup::Dispatch(TaskCategory::Other, r.forget()));

#ifdef DEBUG
    mStarted = true;
#endif

    return true;
  }

  void Cancel() override {
    MOZ_ASSERT(!mStarted);

#ifdef DEBUG
    mStarted = true;
#endif
  }

 private:
  ~SharedWorkerOp() { MOZ_ASSERT(mStarted); }

  bool IsTerminationOp() const {
    return mOp.type() == RemoteWorkerOp::TRemoteWorkerTerminateOp;
  }

  void Exec(SelfHolder& aOwner) {
    using Running = RemoteWorkerChild::Running;

    AssertIsOnMainThread();

    auto lock = aOwner->mState.Lock();

    MOZ_ASSERT(lock->is<Running>() || IsTerminationOp());

    if (IsTerminationOp()) {
      aOwner->CloseWorkerOnMainThread(lock.ref());
      return;
    }

    RefPtr<WorkerPrivate> workerPrivate = lock->as<Running>().mWorkerPrivate;

    MOZ_ASSERT(workerPrivate);

    if (mOp.type() == RemoteWorkerOp::TRemoteWorkerSuspendOp) {
      workerPrivate->ParentWindowPaused();
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerResumeOp) {
      workerPrivate->ParentWindowResumed();
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerFreezeOp) {
      workerPrivate->Freeze(nullptr);
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerThawOp) {
      workerPrivate->Thaw(nullptr);
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerPortIdentifierOp) {
      RefPtr<MessagePortIdentifierRunnable> r =
          new MessagePortIdentifierRunnable(
              workerPrivate, aOwner,
              mOp.get_RemoteWorkerPortIdentifierOp().portIdentifier());

      if (NS_WARN_IF(!r->Dispatch())) {
        aOwner->ErrorPropagation(NS_ERROR_FAILURE);
      }
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerAddWindowIDOp) {
      aOwner->mWindowIDs.AppendElement(
          mOp.get_RemoteWorkerAddWindowIDOp().windowID());
    } else if (mOp.type() == RemoteWorkerOp::TRemoteWorkerRemoveWindowIDOp) {
      aOwner->mWindowIDs.RemoveElement(
          mOp.get_RemoteWorkerRemoveWindowIDOp().windowID());
    } else {
      MOZ_CRASH("Unknown RemoteWorkerOp type!");
    }
  }

  RemoteWorkerOp mOp;

#ifdef DEBUG
  bool mStarted = false;
#endif
};

void RemoteWorkerChild::AddPortIdentifier(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate,
    const MessagePortIdentifier& aPortIdentifier) {
  if (NS_WARN_IF(!aWorkerPrivate->ConnectMessagePort(aCx, aPortIdentifier))) {
    ErrorPropagationDispatch(NS_ERROR_FAILURE);
  }
}

void RemoteWorkerChild::CancelAllPendingOps(State& aState) {
  MOZ_ASSERT(aState.is<Pending>());

  auto pendingOps = std::move(aState.as<Pending>().mPendingOps);

  for (auto& op : pendingOps) {
    op->Cancel();
  }
}

void RemoteWorkerChild::MaybeStartOp(RefPtr<Op>&& aOp) {
  MOZ_ASSERT(aOp);

  auto lock = mState.Lock();

  if (!aOp->MaybeStart(this, lock.ref())) {
    lock->as<Pending>().mPendingOps.AppendElement(std::move(aOp));
  }
}

IPCResult RemoteWorkerChild::RecvExecOp(RemoteWorkerOp&& aOp) {
  MOZ_ASSERT(!mIsServiceWorker);

  MaybeStartOp(new SharedWorkerOp(std::move(aOp)));

  return IPC_OK();
}

IPCResult RemoteWorkerChild::RecvExecServiceWorkerOp(
    ServiceWorkerOpArgs&& aArgs, ExecServiceWorkerOpResolver&& aResolve) {
  MOZ_ASSERT(mIsServiceWorker);
  MOZ_ASSERT(
      aArgs.type() != ServiceWorkerOpArgs::TServiceWorkerFetchEventOpArgs,
      "FetchEvent operations should be sent via PFetchEventOp(Proxy) actors!");

  MaybeStartOp(ServiceWorkerOp::Create(aArgs, std::move(aResolve)));

  return IPC_OK();
}

RefPtr<GenericPromise>
RemoteWorkerChild::MaybeSendSetServiceWorkerSkipWaitingFlag() {
  RefPtr<GenericPromise::Private> promise =
      new GenericPromise::Private(__func__);

  RefPtr<RemoteWorkerChild> self = this;

  nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction(__func__, [self = std::move(
                                                                  self),
                                                              promise] {
    MOZ_ACCESS_THREAD_BOUND(self->mLauncherData, launcherData);

    if (!launcherData->mIPCActive) {
      promise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
      return;
    }

    self->SendSetServiceWorkerSkipWaitingFlag()->Then(
        GetCurrentThreadSerialEventTarget(), __func__,
        [promise](
            const SetServiceWorkerSkipWaitingFlagPromise::ResolveOrRejectValue&
                aResult) {
          if (NS_WARN_IF(aResult.IsReject())) {
            promise->Reject(NS_ERROR_DOM_ABORT_ERR, __func__);
            return;
          }

          promise->Resolve(aResult.ResolveValue(), __func__);
        });
  });

  GetOwningEventTarget()->Dispatch(r.forget(), NS_DISPATCH_NORMAL);

  return promise;
}

/**
 * PFetchEventOpProxy methods
 */
PFetchEventOpProxyChild* RemoteWorkerChild::AllocPFetchEventOpProxyChild(
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  RefPtr<FetchEventOpProxyChild> actor = new FetchEventOpProxyChild();

  return actor.forget().take();
}

IPCResult RemoteWorkerChild::RecvPFetchEventOpProxyConstructor(
    PFetchEventOpProxyChild* aActor,
    const ServiceWorkerFetchEventOpArgs& aArgs) {
  MOZ_ASSERT(aActor);

  (static_cast<FetchEventOpProxyChild*>(aActor))->Initialize(aArgs);

  return IPC_OK();
}

bool RemoteWorkerChild::DeallocPFetchEventOpProxyChild(
    PFetchEventOpProxyChild* aActor) {
  MOZ_ASSERT(aActor);

  RefPtr<FetchEventOpProxyChild> actor =
      dont_AddRef(static_cast<FetchEventOpProxyChild*>(aActor));

  return true;
}

}  // namespace dom
}  // namespace mozilla
