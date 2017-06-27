/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushSubscription.h"

#include "nsIPushService.h"
#include "nsIScriptObjectPrincipal.h"

#include "mozilla/Base64.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"
#include "mozilla/dom/PushSubscriptionOptions.h"
#include "mozilla/dom/PushUtil.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/workers/Workers.h"

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

class UnsubscribeResultCallback final : public nsIUnsubscribeResultCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit UnsubscribeResultCallback(Promise* aPromise)
    : mPromise(aPromise)
  {
    AssertIsOnMainThread();
  }

  NS_IMETHOD
  OnUnsubscribe(nsresult aStatus, bool aSuccess) override
  {
    if (NS_SUCCEEDED(aStatus)) {
      mPromise->MaybeResolve(aSuccess);
    } else {
      mPromise->MaybeReject(NS_ERROR_DOM_PUSH_SERVICE_UNREACHABLE);
    }

    return NS_OK;
  }

private:
  ~UnsubscribeResultCallback()
  {}

  RefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(UnsubscribeResultCallback, nsIUnsubscribeResultCallback)

class UnsubscribeResultRunnable final : public WorkerRunnable
{
public:
  UnsubscribeResultRunnable(WorkerPrivate* aWorkerPrivate,
                            already_AddRefed<PromiseWorkerProxy>&& aProxy,
                            nsresult aStatus,
                            bool aSuccess)
    : WorkerRunnable(aWorkerPrivate)
    , mProxy(Move(aProxy))
    , mStatus(aStatus)
    , mSuccess(aSuccess)
  {
    AssertIsOnMainThread();
  }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mProxy->WorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(mSuccess);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_PUSH_SERVICE_UNREACHABLE);
    }

    mProxy->CleanUp();

    return true;
  }
private:
  ~UnsubscribeResultRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  bool mSuccess;
};

class WorkerUnsubscribeResultCallback final : public nsIUnsubscribeResultCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit WorkerUnsubscribeResultCallback(PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
  {
    AssertIsOnMainThread();
  }

  NS_IMETHOD
  OnUnsubscribe(nsresult aStatus, bool aSuccess) override
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mProxy, "OnUnsubscribe() called twice?");

    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return NS_OK;
    }

    WorkerPrivate* worker = mProxy->GetWorkerPrivate();
    RefPtr<UnsubscribeResultRunnable> r =
      new UnsubscribeResultRunnable(worker, mProxy.forget(), aStatus, aSuccess);
    MOZ_ALWAYS_TRUE(r->Dispatch());

    return NS_OK;
  }

private:
  ~WorkerUnsubscribeResultCallback()
  {
  }

  RefPtr<PromiseWorkerProxy> mProxy;
};

NS_IMPL_ISUPPORTS(WorkerUnsubscribeResultCallback, nsIUnsubscribeResultCallback)

class UnsubscribeRunnable final : public Runnable
{
public:
  UnsubscribeRunnable(PromiseWorkerProxy* aProxy, const nsAString& aScope)
    : Runnable("dom::UnsubscribeRunnable")
    , mProxy(aProxy)
    , mScope(aScope)
  {
    MOZ_ASSERT(aProxy);
    MOZ_ASSERT(!aScope.IsEmpty());
  }

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;

    {
      MutexAutoLock lock(mProxy->Lock());
      if (mProxy->CleanedUp()) {
        return NS_OK;
      }
      principal = mProxy->GetWorkerPrivate()->GetPrincipal();
    }

    MOZ_ASSERT(principal);

    RefPtr<WorkerUnsubscribeResultCallback> callback =
      new WorkerUnsubscribeResultCallback(mProxy);

    nsCOMPtr<nsIPushService> service =
      do_GetService("@mozilla.org/push/Service;1");
    if (NS_WARN_IF(!service)) {
      callback->OnUnsubscribe(NS_ERROR_FAILURE, false);
      return NS_OK;
    }

    if (NS_WARN_IF(NS_FAILED(service->Unsubscribe(mScope, principal, callback)))) {
      callback->OnUnsubscribe(NS_ERROR_FAILURE, false);
      return NS_OK;
    }

    return NS_OK;
  }

private:
  ~UnsubscribeRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
};

} // anonymous namespace

PushSubscription::PushSubscription(nsIGlobalObject* aGlobal,
                                   const nsAString& aEndpoint,
                                   const nsAString& aScope,
                                   nsTArray<uint8_t>&& aRawP256dhKey,
                                   nsTArray<uint8_t>&& aAuthSecret,
                                   nsTArray<uint8_t>&& aAppServerKey)
  : mEndpoint(aEndpoint)
  , mScope(aScope)
  , mRawP256dhKey(Move(aRawP256dhKey))
  , mAuthSecret(Move(aAuthSecret))
{
  if (NS_IsMainThread()) {
    mGlobal = aGlobal;
  } else {
#ifdef DEBUG
    // There's only one global on a worker, so we don't need to pass a global
    // object to the constructor.
    WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
    MOZ_ASSERT(worker);
    worker->AssertIsOnWorkerThread();
#endif
  }
  mOptions = new PushSubscriptionOptions(mGlobal, Move(aAppServerKey));
}

PushSubscription::~PushSubscription()
{}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushSubscription, mGlobal, mOptions)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushSubscription)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushSubscription)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushSubscription)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
PushSubscription::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushSubscriptionBinding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<PushSubscription>
PushSubscription::Constructor(GlobalObject& aGlobal,
                              const PushSubscriptionInit& aInitDict,
                              ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());

  nsTArray<uint8_t> rawKey;
  if (aInitDict.mP256dhKey.WasPassed() &&
      !aInitDict.mP256dhKey.Value().IsNull() &&
      !PushUtil::CopyArrayBufferToArray(aInitDict.mP256dhKey.Value().Value(),
                                        rawKey)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsTArray<uint8_t> authSecret;
  if (aInitDict.mAuthSecret.WasPassed() &&
      !aInitDict.mAuthSecret.Value().IsNull() &&
      !PushUtil::CopyArrayBufferToArray(aInitDict.mAuthSecret.Value().Value(),
                                        authSecret)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  nsTArray<uint8_t> appServerKey;
  if (aInitDict.mAppServerKey.WasPassed() &&
      !aInitDict.mAppServerKey.Value().IsNull()) {
    const OwningArrayBufferViewOrArrayBuffer& bufferSource =
      aInitDict.mAppServerKey.Value().Value();
    if (!PushUtil::CopyBufferSourceToArray(bufferSource, appServerKey)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }
  }

  RefPtr<PushSubscription> sub = new PushSubscription(global,
                                                      aInitDict.mEndpoint,
                                                      aInitDict.mScope,
                                                      Move(rawKey),
                                                      Move(authSecret),
                                                      Move(appServerKey));

  return sub.forget();
}

already_AddRefed<Promise>
PushSubscription::Unsubscribe(ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    RefPtr<Promise> p = UnsubscribeFromWorker(aRv);
    return p.forget();
  }

  MOZ_ASSERT(mGlobal);

  nsCOMPtr<nsIPushService> service =
    do_GetService("@mozilla.org/push/Service;1");
  if (NS_WARN_IF(!service)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(mGlobal);
  if (!sop) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> p = Promise::Create(mGlobal, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<UnsubscribeResultCallback> callback =
    new UnsubscribeResultCallback(p);
  Unused << NS_WARN_IF(NS_FAILED(
    service->Unsubscribe(mScope, sop->GetPrincipal(), callback)));

  return p.forget();
}

void
PushSubscription::GetKey(JSContext* aCx,
                         PushEncryptionKeyName aType,
                         JS::MutableHandle<JSObject*> aKey,
                         ErrorResult& aRv)
{
  if (aType == PushEncryptionKeyName::P256dh) {
    PushUtil::CopyArrayToArrayBuffer(aCx, mRawP256dhKey, aKey, aRv);
  } else if (aType == PushEncryptionKeyName::Auth) {
    PushUtil::CopyArrayToArrayBuffer(aCx, mAuthSecret, aKey, aRv);
  } else {
    aKey.set(nullptr);
  }
}

void
PushSubscription::ToJSON(PushSubscriptionJSON& aJSON, ErrorResult& aRv)
{
  aJSON.mEndpoint.Construct();
  aJSON.mEndpoint.Value() = mEndpoint;

  aJSON.mKeys.mP256dh.Construct();
  nsresult rv = Base64URLEncode(mRawP256dhKey.Length(),
                                mRawP256dhKey.Elements(),
                                Base64URLEncodePaddingPolicy::Omit,
                                aJSON.mKeys.mP256dh.Value());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }

  aJSON.mKeys.mAuth.Construct();
  rv = Base64URLEncode(mAuthSecret.Length(), mAuthSecret.Elements(),
                       Base64URLEncodePaddingPolicy::Omit,
                       aJSON.mKeys.mAuth.Value());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(rv);
    return;
  }
}

already_AddRefed<PushSubscriptionOptions>
PushSubscription::Options()
{
  RefPtr<PushSubscriptionOptions> options = mOptions;
  return options.forget();
}

already_AddRefed<Promise>
PushSubscription::UnsubscribeFromWorker(ErrorResult& aRv)
{
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();

  nsCOMPtr<nsIGlobalObject> global = worker->GlobalScope();
  RefPtr<Promise> p = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<PromiseWorkerProxy> proxy = PromiseWorkerProxy::Create(worker, p);
  if (!proxy) {
    p->MaybeReject(NS_ERROR_DOM_PUSH_SERVICE_UNREACHABLE);
    return p.forget();
  }

  RefPtr<UnsubscribeRunnable> r =
    new UnsubscribeRunnable(proxy, mScope);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

  return p.forget();
}

} // namespace dom
} // namespace mozilla
