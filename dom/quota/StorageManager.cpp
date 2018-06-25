/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageManager.h"

#include "mozilla/dom/DOMPrefs.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/quota/QuotaManagerService.h"
#include "mozilla/dom/StorageManagerBinding.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/Telemetry.h"
#include "nsContentPermissionHelper.h"
#include "nsIQuotaCallbacks.h"
#include "nsIQuotaRequests.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

namespace {

// This class is used to get quota usage, request persist and check persisted
// status callbacks.
class RequestResolver final
  : public nsIQuotaCallback
  , public nsIQuotaUsageCallback
{
public:
  enum Type
  {
    Estimate,
    Persist,
    Persisted
  };

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
    : mPromise(aPromise)
    , mResultCode(NS_OK)
    , mType(aType)
    , mPersisted(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aPromise);
  }

  RequestResolver(Type aType, PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
    , mResultCode(NS_OK)
    , mType(aType)
    , mPersisted(false)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aProxy);
  }

  void
  ResolveOrReject();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIQUOTACALLBACK
  NS_DECL_NSIQUOTAUSAGECALLBACK

private:
  ~RequestResolver()
  { }

  nsresult
  GetStorageEstimate(nsIVariant* aResult);

  nsresult
  GetPersisted(nsIVariant* aResult);

  template <typename T>
  nsresult
  OnCompleteOrUsageResult(T* aRequest);

  nsresult
  Finish();
};

// This class is used to return promise on worker thread.
class RequestResolver::FinishWorkerRunnable final
  : public WorkerRunnable
{
  RefPtr<RequestResolver> mResolver;

public:
  explicit FinishWorkerRunnable(RequestResolver* aResolver)
    : WorkerRunnable(aResolver->mProxy->GetWorkerPrivate())
    , mResolver(aResolver)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aResolver);
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override;
};

class EstimateWorkerMainThreadRunnable final
  : public WorkerMainThreadRunnable
{
  RefPtr<PromiseWorkerProxy> mProxy;

public:
  EstimateWorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                   PromiseWorkerProxy* aProxy)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("StorageManager :: Estimate"))
    , mProxy(aProxy)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aProxy);
  }

  bool
  MainThreadRun() override;
};

class PersistedWorkerMainThreadRunnable final
  : public WorkerMainThreadRunnable
{
  RefPtr<PromiseWorkerProxy> mProxy;

public:
  PersistedWorkerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                    PromiseWorkerProxy* aProxy)
    : WorkerMainThreadRunnable(aWorkerPrivate,
                               NS_LITERAL_CSTRING("StorageManager :: Persisted"))
    , mProxy(aProxy)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
    MOZ_ASSERT(aProxy);
  }

  bool
  MainThreadRun() override;
};

/*******************************************************************************
 * PersistentStoragePermissionRequest
 ******************************************************************************/

class PersistentStoragePermissionRequest final
  : public nsIContentPermissionRequest
{
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  bool mIsHandlingUserInput;
  RefPtr<Promise> mPromise;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;

public:
  PersistentStoragePermissionRequest(nsIPrincipal* aPrincipal,
                                     nsPIDOMWindowInner* aWindow,
                                     bool aIsHandlingUserInput,
                                     Promise* aPromise)
    : mPrincipal(aPrincipal)
    , mWindow(aWindow)
    , mIsHandlingUserInput(aIsHandlingUserInput)
    , mPromise(aPromise)
  {
    MOZ_ASSERT(aPrincipal);
    MOZ_ASSERT(aWindow);
    MOZ_ASSERT(aPromise);

    mRequester = new nsContentPermissionRequester(mWindow);
  }

  nsresult
  Start();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_CYCLE_COLLECTION_CLASS(PersistentStoragePermissionRequest)

private:
  ~PersistentStoragePermissionRequest()
  { }
};

nsresult
GetUsageForPrincipal(nsIPrincipal* aPrincipal,
                     nsIQuotaUsageCallback* aCallback,
                     nsIQuotaUsageRequest** aRequest)
{
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(aRequest);

  nsCOMPtr<nsIQuotaManagerService> qms = QuotaManagerService::GetOrCreate();
  if (NS_WARN_IF(!qms)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = qms->GetUsageForPrincipal(aPrincipal,
                                          aCallback,
                                          true,
                                          aRequest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
};

nsresult
Persisted(nsIPrincipal* aPrincipal,
          nsIQuotaCallback* aCallback,
          nsIQuotaRequest** aRequest)
{
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

already_AddRefed<Promise>
ExecuteOpOnMainOrWorkerThread(nsIGlobalObject* aGlobal,
                              RequestResolver::Type aType,
                              ErrorResult& aRv)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT_IF(aType == RequestResolver::Type::Persist,
                NS_IsMainThread());

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

    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    if (NS_WARN_IF(!doc)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return nullptr;
    }

    nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
    MOZ_ASSERT(principal);

    // Storage Standard 7. API
    // If origin is an opaque origin, then reject promise with a TypeError.
    if (principal->GetIsNullPrincipal()) {
      promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
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
          new PersistentStoragePermissionRequest(principal,
                                                 window,
                                                 EventStateManager::IsHandlingUserInput(),
                                                 promise);

        // In private browsing mode, no permission prompt.
        if (nsContentUtils::IsInPrivateBrowsing(doc)) {
          aRv = request->Cancel();
        } else {
          aRv = request->Start();
        }

        break;
      }

      case RequestResolver::Type::Estimate: {
        RefPtr<RequestResolver> resolver =
          new RequestResolver(RequestResolver::Type::Estimate, promise);

        RefPtr<nsIQuotaUsageRequest> request;
        aRv = GetUsageForPrincipal(principal,
                                   resolver,
                                   getter_AddRefs(request));

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
    return nullptr;
  }

  switch (aType) {
    case RequestResolver::Type::Estimate: {
      RefPtr<EstimateWorkerMainThreadRunnable> runnnable =
        new EstimateWorkerMainThreadRunnable(promiseProxy->GetWorkerPrivate(),
                                             promiseProxy);
      runnnable->Dispatch(Terminating, aRv);

      break;
    }

    case RequestResolver::Type::Persisted: {
      RefPtr<PersistedWorkerMainThreadRunnable> runnnable =
        new PersistedWorkerMainThreadRunnable(promiseProxy->GetWorkerPrivate(),
                                              promiseProxy);
      runnnable->Dispatch(Terminating, aRv);

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

} // namespace

/*******************************************************************************
 * Local class implementations
 ******************************************************************************/

void
RequestResolver::ResolveOrReject()
{
  class MOZ_STACK_CLASS AutoCleanup final
  {
    RefPtr<PromiseWorkerProxy> mProxy;

  public:
    explicit AutoCleanup(PromiseWorkerProxy* aProxy)
      : mProxy(aProxy)
    {
      MOZ_ASSERT(aProxy);
    }

    ~AutoCleanup()
    {
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
      promise->MaybeReject(NS_ERROR_DOM_TYPE_ERR);
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

NS_IMPL_ISUPPORTS(RequestResolver, nsIQuotaUsageCallback, nsIQuotaCallback)

nsresult
RequestResolver::GetStorageEstimate(nsIVariant* aResult)
{
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mType == Type::Estimate);

#ifdef DEBUG
  uint16_t dataType;
  MOZ_ALWAYS_SUCCEEDS(aResult->GetDataType(&dataType));
  MOZ_ASSERT(dataType == nsIDataType::VTYPE_INTERFACE_IS);
#endif

  nsID* iid;
  nsCOMPtr<nsISupports> supports;
  nsresult rv = aResult->GetAsInterface(&iid, getter_AddRefs(supports));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  free(iid);

  nsCOMPtr<nsIQuotaOriginUsageResult> originUsageResult =
    do_QueryInterface(supports);
  MOZ_ASSERT(originUsageResult);

  MOZ_ALWAYS_SUCCEEDS(
    originUsageResult->GetUsage(&mStorageEstimate.mUsage.Construct()));

  MOZ_ALWAYS_SUCCEEDS(
    originUsageResult->GetLimit(&mStorageEstimate.mQuota.Construct()));

  return NS_OK;
}

nsresult
RequestResolver::GetPersisted(nsIVariant* aResult)
{
  MOZ_ASSERT(aResult);
  MOZ_ASSERT(mType == Type::Persist || mType == Type::Persisted);

#ifdef DEBUG
  uint16_t dataType;
  MOZ_ALWAYS_SUCCEEDS(aResult->GetDataType(&dataType));
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

template <typename T>
nsresult
RequestResolver::OnCompleteOrUsageResult(T* aRequest)
{
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

nsresult
RequestResolver::Finish()
{
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
RequestResolver::OnComplete(nsIQuotaRequest *aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  mResultCode = OnCompleteOrUsageResult(aRequest);

  nsresult rv = Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
RequestResolver::OnUsageResult(nsIQuotaUsageRequest *aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequest);

  mResultCode = OnCompleteOrUsageResult(aRequest);

  nsresult rv = Finish();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
RequestResolver::
FinishWorkerRunnable::WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aWorkerPrivate);
  aWorkerPrivate->AssertIsOnWorkerThread();

  MOZ_ASSERT(mResolver);
  mResolver->ResolveOrReject();

  return true;
}

bool
EstimateWorkerMainThreadRunnable::MainThreadRun()
{
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

  RefPtr<nsIQuotaUsageRequest> request;
  nsresult rv =
    GetUsageForPrincipal(principal, resolver, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return false;
  }

  return true;
}

bool
PersistedWorkerMainThreadRunnable::MainThreadRun()
{
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

nsresult
PersistentStoragePermissionRequest::Start()
{
  MOZ_ASSERT(NS_IsMainThread());

  // Grant permission if pref'ed on.
  if (Preferences::GetBool("dom.storageManager.prompt.testing", false)) {
    if (Preferences::GetBool("dom.storageManager.prompt.testing.allow",
                             false)) {
      return Allow(JS::UndefinedHandleValue);
    }

    return Cancel();
  }

  return nsContentPermissionUtils::AskPermission(this, mWindow);
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(PersistentStoragePermissionRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PersistentStoragePermissionRequest)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PersistentStoragePermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION(PersistentStoragePermissionRequest, mWindow, mPromise)

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetPrincipal(nsIPrincipal** aPrincipal)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(mPrincipal);

  NS_ADDREF(*aPrincipal = mPrincipal);

  return NS_OK;
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetIsHandlingUserInput(bool* aIsHandlingUserInput)
{
  *aIsHandlingUserInput = mIsHandlingUserInput;
  return NS_OK;
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetWindow(mozIDOMWindow** aRequestingWindow)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequestingWindow);
  MOZ_ASSERT(mWindow);

  NS_ADDREF(*aRequestingWindow = mWindow);

  return NS_OK;
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetElement(Element** aElement)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aElement);

  *aElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::Cancel()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPromise);

  RefPtr<RequestResolver> resolver =
    new RequestResolver(RequestResolver::Type::Persisted, mPromise);

  RefPtr<nsIQuotaRequest> request;

  return Persisted(mPrincipal, resolver, getter_AddRefs(request));
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::Allow(JS::HandleValue aChoices)
{
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

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetRequester(
                                     nsIContentPermissionRequester** aRequester)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aRequester);

  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);

  return NS_OK;
}

NS_IMETHODIMP
PersistentStoragePermissionRequest::GetTypes(nsIArray** aTypes)
{
  MOZ_ASSERT(aTypes);

  nsTArray<nsString> emptyOptions;

  return nsContentPermissionUtils::CreatePermissionArray(
                                       NS_LITERAL_CSTRING("persistent-storage"),
                                       NS_LITERAL_CSTRING("unused"),
                                       emptyOptions,
                                       aTypes);
}

/*******************************************************************************
 * StorageManager
 ******************************************************************************/

StorageManager::StorageManager(nsIGlobalObject* aGlobal)
  : mOwner(aGlobal)
{
  MOZ_ASSERT(aGlobal);
}

StorageManager::~StorageManager()
{
}

already_AddRefed<Promise>
StorageManager::Persisted(ErrorResult& aRv)
{
  MOZ_ASSERT(mOwner);

  return ExecuteOpOnMainOrWorkerThread(mOwner,
                                       RequestResolver::Type::Persisted,
                                       aRv);
}

already_AddRefed<Promise>
StorageManager::Persist(ErrorResult& aRv)
{
  MOZ_ASSERT(mOwner);

  Telemetry::ScalarAdd(Telemetry::ScalarID::NAVIGATOR_STORAGE_PERSIST_COUNT, 1);
  return ExecuteOpOnMainOrWorkerThread(mOwner,
                                       RequestResolver::Type::Persist,
                                       aRv);
}

already_AddRefed<Promise>
StorageManager::Estimate(ErrorResult& aRv)
{
  MOZ_ASSERT(mOwner);

  Telemetry::ScalarAdd(Telemetry::ScalarID::NAVIGATOR_STORAGE_ESTIMATE_COUNT,
                       1);
  return ExecuteOpOnMainOrWorkerThread(mOwner,
                                       RequestResolver::Type::Estimate,
                                       aRv);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(StorageManager, mOwner)

NS_IMPL_CYCLE_COLLECTING_ADDREF(StorageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(StorageManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StorageManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
StorageManager::WrapObject(JSContext* aCx,
                           JS::Handle<JSObject*> aGivenProto)
{
  return StorageManager_Binding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
