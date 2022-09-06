/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageManager.h"
#include "fs/FileSystemRequestHandler.h"

#include <cstdint>
#include <cstdlib>
#include <utility>
#include "ErrorList.h"
#include "fs/FileSystemRequestHandler.h"
#include "MainThreadUtils.h"
#include "js/CallArgs.h"
#include "js/TypeDecls.h"
#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Telemetry.h"
#include "mozilla/TelemetryScalarEnums.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FileSystemManager.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/StorageManagerBinding.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerStatus.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIGlobalObject.h"
#include "nsIPrincipal.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaManagerService.h"
#include "nsIQuotaRequests.h"
#include "nsIQuotaResults.h"
#include "nsIVariant.h"
#include "nsLiteralString.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"
#include "nsStringFlags.h"
#include "nsTLiteralString.h"
#include "nscore.h"

class JSObject;
struct JSContext;
struct nsID;

namespace mozilla {
class Runnable;
}

using namespace mozilla::dom::quota;

namespace mozilla::dom {

namespace {

// This class is used to get quota usage, request persist and check persisted
// status callbacks.
class RequestResolver final : public nsIQuotaCallback {
 public:
  enum Type { Estimate, Persist, Persisted };

 private:
  class FinishWorkerRunnable;

  // If this resolver was created for a window then mPromise must be non-null.
  // Otherwise mProxy must be non-null.
  RefPtr<Promise> mPromise;
  RefPtr<PromiseWorkerProxy> mProxy;

  nsresult mResultCode;
  StorageEstimate mStorageEstimate;
  const Type mType;
  bool mPersisted;

 public:
  RequestResolver(Type aType, Promise* aPromise)
      : mPromise(aPromise),
        mResultCode(NS_OK),
        mType(aType),
        mPersisted(false) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aPromise);
  }

  RequestResolver(Type aType, PromiseWorkerProxy* aProxy)
      : mProxy(aProxy), mResultCode(NS_OK), mType(aType), mPersisted(false) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aProxy);
  }

  void ResolveOrReject();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIQUOTACALLBACK

 private:
  ~RequestResolver() = default;

  nsresult GetStorageEstimate(nsIVariant* aResult);

  nsresult GetPersisted(nsIVariant* aResult);

  nsresult OnCompleteInternal(nsIQuotaRequest* aRequest);

  nsresult Finish();
};

// This class is used to return promise on worker thread.
class RequestResolver::FinishWorkerRunnable final : public WorkerRunnable {
  RefPtr<RequestResolver> mResolver;

 public:
  explicit FinishWorkerRunnable(RequestResolver* aResolver)
      : WorkerRunnable(aResolver->mProxy->GetWorkerPrivate()),
        mResolver(aResolver) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aResolver);
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;
};

class EstimateWorkerMainThreadRunnable final : public WorkerMainThreadRunnable {
  RefPtr<PromiseWorkerProxy> mProxy;

 public:
  EstimateWorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                   PromiseWorkerProxy* aProxy)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "StorageManager :: Estimate"_ns),
        mProxy(aProxy) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aProxy);
  }

  bool MainThreadRun() override;
};

class PersistedWorkerMainThreadRunnable final
    : public WorkerMainThreadRunnable {
  RefPtr<PromiseWorkerProxy> mProxy;

 public:
  PersistedWorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                    PromiseWorkerProxy* aProxy)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "StorageManager :: Persisted"_ns),
        mProxy(aProxy) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aProxy);
  }

  bool MainThreadRun() override;
};

/*******************************************************************************
 * PersistentStoragePermissionRequest
 ******************************************************************************/

class PersistentStoragePermissionRequest final
    : public ContentPermissionRequestBase {
  RefPtr<Promise> mPromise;

 public:
  PersistentStoragePermissionRequest(nsIPrincipal* aPrincipal,
                                     nsPIDOMWindowInner* aWindow,
                                     Promise* aPromise)
      : ContentPermissionRequestBase(aPrincipal, aWindow,
                                     "dom.storageManager"_ns,
                                     "persistent-storage"_ns),
        mPromise(aPromise) {
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aPromise);
  }

  nsresult Start();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PersistentStoragePermissionRequest,
                                           ContentPermissionRequestBase)

  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::Handle<JS::Value> choices) override;

 private:
  ~PersistentStoragePermissionRequest() = default;
};

nsresult Estimate(nsIPrincipal* aPrincipal, nsIQuotaCallback* aCallback,
                  nsIQuotaRequest** aRequest) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aRequest);

  // Firefox and Quota Manager have always used the schemeless origin group
  // (https://storage.spec.whatwg.org/#schemeless-origin-group) for quota limit
  // purposes. This has been to prevent a site/eTLD+1 from claiming more than
  // its fair share of storage through the use of sub-domains. Because the limit
  // is at the group level and the usage needs to make sense in the context of
  // that limit, we also expose the group usage. Bug 1374970 reflects this
  // reality and bug 1305665 tracks our plan to eliminate our use of groups for
  // this.

  nsCOMPtr<nsIQuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  if (NS_WARN_IF(!qms)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIQuotaRequest> request;
  nsresult rv = qms->Estimate(aPrincipal, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ALWAYS_SUCCEEDS(request->SetCallback(aCallback));

  request.forget(aRequest);
  return NS_OK;
};

nsresult Persisted(nsIPrincipal* aPrincipal, nsIQuotaCallback* aCallback,
                   nsIQuotaRequest** aRequest) {
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aRequest);

  nsCOMPtr<nsIQuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  if (NS_WARN_IF(!qms)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIQuotaRequest> request;
  nsresult rv = qms->Persisted(aPrincipal, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // All the methods in nsIQuotaManagerService shouldn't synchronously fire
  // any callbacks when they are being executed. Even when a result is ready,
  // a new runnable should be dispatched to current thread to fire the callback
  // asynchronously. It's safe to set the callback after we call Persisted().
  MOZ_ALWAYS_SUCCEEDS(request->SetCallback(aCallback));

  request.forget(aRequest);

  return NS_OK;
};

already_AddRefed<Promise> ExecuteOpOnMainOrWorkerThread(
    nsIGlobalObject* aGlobal, RequestResolver::Type aType, ErrorResult& aRv) {
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT_IF(aType == RequestResolver::Type::Persist, NS_IsMainThread());

  RefPtr<Promise> promise = Promise::Create(aGlobal, aRv);
  if (NS_WARN_IF(!promise)) {
    return nullptr;
  }

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (NS_WARN_IF(!window)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<Document> doc = window->GetExtantDoc();
    if (NS_WARN_IF(!doc)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
    MOZ_ASSERT(principal);

    // Storage Standard 7. API
    // If origin is an opaque origin, then reject promise with a TypeError.
    if (principal->GetIsNullPrincipal()) {
      switch (aType) {
        case RequestResolver::Type::Persisted:
          promise->MaybeRejectWithTypeError(
              "persisted() called for opaque origin");
          break;
        case RequestResolver::Type::Persist:
          promise->MaybeRejectWithTypeError(
              "persist() called for opaque origin");
          break;
        case RequestResolver::Type::Estimate:
          promise->MaybeRejectWithTypeError(
              "estimate() called for opaque origin");
          break;
      }

      return promise.forget();
    }

    switch (aType) {
      case RequestResolver::Type::Persisted: {
        RefPtr<RequestResolver> resolver =
            new RequestResolver(RequestResolver::Type::Persisted, promise);

        RefPtr<nsIQuotaRequest> request;
        aRv = Persisted(principal, resolver, getter_AddRefs(request));

        break;
      }

      case RequestResolver::Type::Persist: {
        RefPtr<PersistentStoragePermissionRequest> request =
            new PersistentStoragePermissionRequest(principal, window, promise);

        // In private browsing mode, no permission prompt.
        if (nsContentUtils::IsInPrivateBrowsing(doc)) {
          aRv = request->Cancel();
        } else if (!request->CheckPermissionDelegate()) {
          aRv = request->Cancel();
        } else {
          aRv = request->Start();
        }

        break;
      }

      case RequestResolver::Type::Estimate: {
        RefPtr<RequestResolver> resolver =
            new RequestResolver(RequestResolver::Type::Estimate, promise);

        RefPtr<nsIQuotaRequest> request;
        aRv = Estimate(principal, resolver, getter_AddRefs(request));

        break;
      }

      default:
        MOZ_CRASH("Invalid aRequest type!");
    }

    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }

    return promise.forget();
  }

  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(workerPrivate);

  RefPtr<PromiseWorkerProxy> promiseProxy =
      PromiseWorkerProxy::Create(workerPrivate, promise);
  if (NS_WARN_IF(!promiseProxy)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  switch (aType) {
    case RequestResolver::Type::Estimate: {
      RefPtr<EstimateWorkerMainThreadRunnable> runnnable =
          new EstimateWorkerMainThreadRunnable(promiseProxy->GetWorkerPrivate(),
                                               promiseProxy);
      runnnable->Dispatch(Canceling, aRv);

      break;
    }

    case RequestResolver::Type::Persisted: {
      RefPtr<PersistedWorkerMainThreadRunnable> runnnable =
          new PersistedWorkerMainThreadRunnable(
              promiseProxy->GetWorkerPrivate(), promiseProxy);
      runnnable->Dispatch(Canceling, aRv);

      break;
    }

    default:
      MOZ_CRASH("Invalid aRequest type");
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return promise.forget();
};

}  // namespace

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void RequestResolver::ResolveOrReject() {
  class MOZ_STACK_CLASS AutoCleanup final {
    RefPtr<PromiseWorkerProxy> mProxy;

   public:
    explicit AutoCleanup(PromiseWorkerProxy* aProxy) : mProxy(aProxy) {
      MOZ_ASSERT(aProxy);
    }

    ~AutoCleanup() {
      MOZ_ASSERT(mProxy);

      mProxy->CleanUp();
    }
  };

  RefPtr<Promise> promise;
  Maybe<AutoCleanup> autoCleanup;

  if (mPromise) {
    promise = mPromise;
  } else {
    MOZ_ASSERT(mProxy);

    promise = mProxy->WorkerPromise();

    // Only clean up for worker case.
    autoCleanup.emplace(mProxy);
  }

  MOZ_ASSERT(promise);

  if (mType == Type::Estimate) {
    if (NS_SUCCEEDED(mResultCode)) {
      promise->MaybeResolve(mStorageEstimate);
    } else {
      promise->MaybeRejectWithTypeError(
          "Internal error while estimating storage usage");
    }

    return;
  }

  MOZ_ASSERT(mType == Type::Persist || mType == Type::Persisted);

  if (NS_SUCCEEDED(mResultCode)) {
    promise->MaybeResolve(mPersisted);
  } else {
    promise->MaybeResolve(false);
  }
}

NS_IMPL_ISUPPORTS(RequestResolver, nsIQuotaCallback)

nsresult RequestResolver::GetStorageEstimate(nsIVariant* aResult) {
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mType == Type::Estimate);

  MOZ_ASSERT(aResult->GetDataType() == nsIDataType::VTYPE_INTERFACE_IS);

  nsID* iid;
  nsCOMPtr<nsISupports> supports;
  nsresult rv = aResult->GetAsInterface(&iid, getter_AddRefs(supports));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  free(iid);

  nsCOMPtr<nsIQuotaEstimateResult> estimateResult = do_QueryInterface(supports);
  MOZ_ASSERT(estimateResult);

  MOZ_ALWAYS_SUCCEEDS(
      estimateResult->GetUsage(&mStorageEstimate.mUsage.Construct()));

  MOZ_ALWAYS_SUCCEEDS(
      estimateResult->GetLimit(&mStorageEstimate.mQuota.Construct()));

  return NS_OK;
}

nsresult RequestResolver::GetPersisted(nsIVariant* aResult) {
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mType == Type::Persist || mType == Type::Persisted);

#ifdef DEBUG
  uint16_t dataType = aResult->GetDataType();
#endif

  if (mType == Type::Persist) {
    MOZ_ASSERT(dataType == nsIDataType::VTYPE_VOID);

    mPersisted = true;
    return NS_OK;
  }

  MOZ_ASSERT(dataType == nsIDataType::VTYPE_BOOL);

  bool persisted;
  nsresult rv = aResult->GetAsBool(&persisted);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mPersisted = persisted;
  return NS_OK;
}

nsresult RequestResolver::OnCompleteInternal(nsIQuotaRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  nsresult resultCode;
  nsresult rv = aRequest->GetResultCode(&resultCode);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_FAILED(resultCode)) {
    return resultCode;
  }

  nsCOMPtr<nsIVariant> result;
  rv = aRequest->GetResult(getter_AddRefs(result));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (mType == Type::Estimate) {
    rv = GetStorageEstimate(result);
  } else {
    MOZ_ASSERT(mType == Type::Persist || mType == Type::Persisted);

    rv = GetPersisted(result);
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult RequestResolver::Finish() {
  // In a main thread request.
  if (!mProxy) {
    MOZ_ASSERT(mPromise);

    ResolveOrReject();
    return NS_OK;
  }

  {
    // In a worker thread request.
    MutexAutoLock lock(mProxy->Lock());

    if (NS_WARN_IF(mProxy->CleanedUp())) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<FinishWorkerRunnable> runnable = new FinishWorkerRunnable(this);
    if (NS_WARN_IF(!runnable->Dispatch())) {
      return NS_ERROR_FAILURE;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
RequestResolver::OnComplete(nsIQuotaRequest* aRequest) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  mResultCode = OnCompleteInternal(aRequest);

  nsresult rv = Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool RequestResolver::FinishWorkerRunnable::WorkerRun(
    JSContext* aCx, WorkerPrivate* aWorkerPrivate) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mResolver);
  mResolver->ResolveOrReject();

  return true;
}

bool EstimateWorkerMainThreadRunnable::MainThreadRun() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal;

  {
    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return true;
    }
    principal = mProxy->GetWorkerPrivate()->GetPrincipal();
  }

  MOZ_ASSERT(principal);

  RefPtr<RequestResolver> resolver =
      new RequestResolver(RequestResolver::Type::Estimate, mProxy);

  RefPtr<nsIQuotaRequest> request;
  nsresult rv = Estimate(principal, resolver, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

bool PersistedWorkerMainThreadRunnable::MainThreadRun() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIPrincipal> principal;

  {
    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return true;
    }
    principal = mProxy->GetWorkerPrivate()->GetPrincipal();
  }

  MOZ_ASSERT(principal);

  RefPtr<RequestResolver> resolver =
      new RequestResolver(RequestResolver::Type::Persisted, mProxy);

  RefPtr<nsIQuotaRequest> request;
  nsresult rv = Persisted(principal, resolver, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

nsresult PersistentStoragePermissionRequest::Start() {
  MOZ_ASSERT(NS_IsMainThread());

  PromptResult pr;
#ifdef MOZ_WIDGET_ANDROID
  // on Android calling `ShowPrompt` here calls
  // `nsContentPermissionUtils::AskPermission` once, and a response of
  // `PromptResult::Pending` calls it again. This results in multiple requests
  // for storage access, so we check the prompt prefs only to ensure we only
  // request it once.
  pr = CheckPromptPrefs();
#else
  nsresult rv = ShowPrompt(pr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
#endif
  if (pr == PromptResult::Granted) {
    return Allow(JS::UndefinedHandleValue);
  }
  if (pr == PromptResult::Denied) {
    return Cancel();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(
    PersistentStoragePermissionRequest, ContentPermissionRequestBase)

NS_IMPL_CYCLE_COLLECTION_INHERITED(PersistentStoragePermissionRequest,
                                   ContentPermissionRequestBase, mPromise)

NS_IMETHODIMP
PersistentStoragePermissionRequest::Cancel() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPromise);

  RefPtr<RequestResolver> resolver =
      new RequestResolver(RequestResolver::Type::Persisted, mPromise);

  RefPtr<nsIQuotaRequest> request;

  return Persisted(mPrincipal, resolver, getter_AddRefs(request));
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::Allow(JS::Handle<JS::Value> aChoices) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<RequestResolver> resolver =
      new RequestResolver(RequestResolver::Type::Persist, mPromise);

  nsCOMPtr<nsIQuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  if (NS_WARN_IF(!qms)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<nsIQuotaRequest> request;

  nsresult rv = qms->Persist(mPrincipal, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ALWAYS_SUCCEEDS(request->SetCallback(resolver));

  return NS_OK;
}

/*******************************************************************************
 * StorageManager
 ******************************************************************************/

StorageManager::StorageManager(nsIGlobalObject* aGlobal) : mOwner(aGlobal) {
  MOZ_ASSERT(aGlobal);
}

StorageManager::~StorageManager() = default;

already_AddRefed<Promise> StorageManager::Persisted(ErrorResult& aRv) {
  MOZ_ASSERT(mOwner);

  return ExecuteOpOnMainOrWorkerThread(mOwner, RequestResolver::Type::Persisted,
                                       aRv);
}

already_AddRefed<Promise> StorageManager::Persist(ErrorResult& aRv) {
  MOZ_ASSERT(mOwner);

  Telemetry::ScalarAdd(Telemetry::ScalarID::NAVIGATOR_STORAGE_PERSIST_COUNT, 1);
  return ExecuteOpOnMainOrWorkerThread(mOwner, RequestResolver::Type::Persist,
                                       aRv);
}

already_AddRefed<Promise> StorageManager::Estimate(ErrorResult& aRv) {
  MOZ_ASSERT(mOwner);

  Telemetry::ScalarAdd(Telemetry::ScalarID::NAVIGATOR_STORAGE_ESTIMATE_COUNT,
                       1);
  return ExecuteOpOnMainOrWorkerThread(mOwner, RequestResolver::Type::Estimate,
                                       aRv);
}

already_AddRefed<Promise> StorageManager::GetDirectory(ErrorResult& aRv) {
  if (!mFileSystemManager) {
    MOZ_ASSERT(mOwner);

    mFileSystemManager = MakeRefPtr<FileSystemManager>(mOwner, this);
  }

  return mFileSystemManager->GetDirectory(aRv);
}

void StorageManager::Shutdown() {
  if (mFileSystemManager) {
    mFileSystemManager->Shutdown();
    mFileSystemManager = nullptr;
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(StorageManager, mOwner,
                                      mFileSystemManager)

NS_IMPL_CYCLE_COLLECTING_ADDREF(StorageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StorageManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StorageManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* StorageManager::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return StorageManager_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
