/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushManager.h"

#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushSubscription.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"

#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPushService.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"

#include "WorkerRunnable.h"
#include "WorkerPrivate.h"
#include "WorkerScope.h"

namespace mozilla {
namespace dom {

using namespace workers;

namespace {

nsresult
GetPermissionState(nsIPrincipal* aPrincipal,
                   PushPermissionState& aState)
{
  nsCOMPtr<nsIPermissionManager> permManager =
    mozilla::services::GetPermissionManager();

  if (!permManager) {
    return NS_ERROR_FAILURE;
  }
  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  nsresult rv = permManager->TestExactPermissionFromPrincipal(
                  aPrincipal,
                  "desktop-notification",
                  &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (permission == nsIPermissionManager::ALLOW_ACTION) {
    aState = PushPermissionState::Granted;
  } else if (permission == nsIPermissionManager::DENY_ACTION) {
    aState = PushPermissionState::Denied;
  } else {
    aState = PushPermissionState::Prompt;
  }

  return NS_OK;
}

class GetSubscriptionResultRunnable final : public WorkerRunnable
{
public:
  GetSubscriptionResultRunnable(WorkerPrivate* aWorkerPrivate,
                                already_AddRefed<PromiseWorkerProxy>&& aProxy,
                                nsresult aStatus,
                                const nsAString& aEndpoint,
                                const nsAString& aScope,
                                nsTArray<uint8_t>&& aRawP256dhKey,
                                nsTArray<uint8_t>&& aAuthSecret)
    : WorkerRunnable(aWorkerPrivate, WorkerThreadModifyBusyCount)
    , mProxy(Move(aProxy))
    , mStatus(aStatus)
    , mEndpoint(aEndpoint)
    , mScope(aScope)
    , mRawP256dhKey(Move(aRawP256dhKey))
    , mAuthSecret(Move(aAuthSecret))
  { }

  bool
  WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override
  {
    RefPtr<Promise> promise = mProxy->WorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      if (mEndpoint.IsEmpty()) {
        promise->MaybeResolve(JS::NullHandleValue);
      } else {
        RefPtr<PushSubscription> sub =
            new PushSubscription(nullptr, mEndpoint, mScope,
                                 Move(mRawP256dhKey), Move(mAuthSecret));
        promise->MaybeResolve(sub);
      }
    } else if (NS_ERROR_GET_MODULE(mStatus) == NS_ERROR_MODULE_DOM_PUSH ) {
      promise->MaybeReject(mStatus);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_PUSH_ABORT_ERR);
    }

    mProxy->CleanUp();

    return true;
  }
private:
  ~GetSubscriptionResultRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  nsString mEndpoint;
  nsString mScope;
  nsTArray<uint8_t> mRawP256dhKey;
  nsTArray<uint8_t> mAuthSecret;
};

class GetSubscriptionCallback final : public nsIPushSubscriptionCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GetSubscriptionCallback(PromiseWorkerProxy* aProxy,
                                   const nsAString& aScope)
    : mProxy(aProxy)
    , mScope(aScope)
  {}

  NS_IMETHOD
  OnPushSubscription(nsresult aStatus,
                     nsIPushSubscription* aSubscription) override
  {
    AssertIsOnMainThread();
    MOZ_ASSERT(mProxy, "OnPushSubscription() called twice?");

    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return NS_OK;
    }

    nsAutoString endpoint;
    nsTArray<uint8_t> rawP256dhKey, authSecret;
    if (NS_SUCCEEDED(aStatus)) {
      aStatus = GetSubscriptionParams(aSubscription, endpoint, rawP256dhKey,
                                      authSecret);
    }

    WorkerPrivate* worker = mProxy->GetWorkerPrivate();
    RefPtr<GetSubscriptionResultRunnable> r =
      new GetSubscriptionResultRunnable(worker,
                                        mProxy.forget(),
                                        aStatus,
                                        endpoint,
                                        mScope,
                                        Move(rawP256dhKey),
                                        Move(authSecret));
    MOZ_ALWAYS_TRUE(r->Dispatch());

    return NS_OK;
  }

  // Convenience method for use in this file.
  void
  OnPushSubscriptionError(nsresult aStatus)
  {
    Unused << NS_WARN_IF(NS_FAILED(
        OnPushSubscription(aStatus, nullptr)));
  }

protected:
  ~GetSubscriptionCallback()
  {}

private:
  inline nsresult
  FreeKeys(nsresult aStatus, uint8_t* aKey, uint8_t* aAuthSecret)
  {
    NS_Free(aKey);
    NS_Free(aAuthSecret);

    return aStatus;
  }

  nsresult
  GetSubscriptionParams(nsIPushSubscription* aSubscription,
                        nsAString& aEndpoint,
                        nsTArray<uint8_t>& aRawP256dhKey,
                        nsTArray<uint8_t>& aAuthSecret)
  {
    if (!aSubscription) {
      return NS_OK;
    }

    nsresult rv = aSubscription->GetEndpoint(aEndpoint);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    uint8_t* key = nullptr;
    uint8_t* authSecret = nullptr;

    uint32_t keyLen;
    rv = aSubscription->GetKey(NS_LITERAL_STRING("p256dh"), &keyLen, &key);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FreeKeys(rv, key, authSecret);
    }

    uint32_t authSecretLen;
    rv = aSubscription->GetKey(NS_LITERAL_STRING("auth"), &authSecretLen,
                               &authSecret);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return FreeKeys(rv, key, authSecret);
    }

    if (!aRawP256dhKey.SetLength(keyLen, fallible) ||
        !aRawP256dhKey.ReplaceElementsAt(0, keyLen, key, keyLen, fallible) ||
        !aAuthSecret.SetLength(authSecretLen, fallible) ||
        !aAuthSecret.ReplaceElementsAt(0, authSecretLen, authSecret,
                                       authSecretLen, fallible)) {

      return FreeKeys(NS_ERROR_OUT_OF_MEMORY, key, authSecret);
    }

    return FreeKeys(NS_OK, key, authSecret);
  }

  RefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
};

NS_IMPL_ISUPPORTS(GetSubscriptionCallback, nsIPushSubscriptionCallback)

class GetSubscriptionRunnable final : public nsRunnable
{
public:
  GetSubscriptionRunnable(PromiseWorkerProxy* aProxy,
                          const nsAString& aScope,
                          PushManager::SubscriptionAction aAction)
    : mProxy(aProxy)
    , mScope(aScope), mAction(aAction)
  {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();

    nsCOMPtr<nsIPrincipal> principal;

    {
      // Bug 1228723: If permission is revoked or an error occurs, the
      // subscription callback will be called synchronously. This causes
      // `GetSubscriptionCallback::OnPushSubscription` to deadlock when
      // it tries to acquire the lock.
      MutexAutoLock lock(mProxy->Lock());
      if (mProxy->CleanedUp()) {
        return NS_OK;
      }
      principal = mProxy->GetWorkerPrivate()->GetPrincipal();
    }

    MOZ_ASSERT(principal);

    RefPtr<GetSubscriptionCallback> callback = new GetSubscriptionCallback(mProxy, mScope);

    PushPermissionState state;
    nsresult rv = GetPermissionState(principal, state);
    if (NS_FAILED(rv)) {
      callback->OnPushSubscriptionError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    if (state != PushPermissionState::Granted) {
      if (mAction == PushManager::GetSubscriptionAction) {
        callback->OnPushSubscriptionError(NS_OK);
        return NS_OK;
      }
      callback->OnPushSubscriptionError(NS_ERROR_DOM_PUSH_DENIED_ERR);
      return NS_OK;
    }

    nsCOMPtr<nsIPushService> service =
      do_GetService("@mozilla.org/push/Service;1");
    if (NS_WARN_IF(!service)) {
      callback->OnPushSubscriptionError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    if (mAction == PushManager::SubscribeAction) {
      rv = service->Subscribe(mScope, principal, callback);
    } else {
      MOZ_ASSERT(mAction == PushManager::GetSubscriptionAction);
      rv = service->GetSubscription(mScope, principal, callback);
    }

    if (NS_WARN_IF(NS_FAILED(rv))) {
      callback->OnPushSubscriptionError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    return NS_OK;
  }

private:
  ~GetSubscriptionRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
  PushManager::SubscriptionAction mAction;
};

class PermissionResultRunnable final : public WorkerRunnable
{
public:
  PermissionResultRunnable(PromiseWorkerProxy *aProxy,
                           nsresult aStatus,
                           PushPermissionState aState)
    : WorkerRunnable(aProxy->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mProxy(aProxy)
    , mStatus(aStatus)
    , mState(aState)
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
      promise->MaybeResolve(mState);
    } else {
      promise->MaybeReject(aCx, JS::UndefinedHandleValue);
    }

    mProxy->CleanUp();

    return true;
  }

private:
  ~PermissionResultRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  PushPermissionState mState;
};

class PermissionStateRunnable final : public nsRunnable
{
public:
  explicit PermissionStateRunnable(PromiseWorkerProxy* aProxy)
    : mProxy(aProxy)
  {}

  NS_IMETHOD
  Run() override
  {
    AssertIsOnMainThread();
    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return NS_OK;
    }

    PushPermissionState state;
    nsresult rv = GetPermissionState(
      mProxy->GetWorkerPrivate()->GetPrincipal(),
      state
    );

    RefPtr<PermissionResultRunnable> r =
      new PermissionResultRunnable(mProxy, rv, state);
    MOZ_ALWAYS_TRUE(r->Dispatch());

    return NS_OK;
  }

private:
  ~PermissionStateRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
};

} // anonymous namespace

PushManager::PushManager(nsIGlobalObject* aGlobal, PushManagerImpl* aImpl)
  : mGlobal(aGlobal)
  , mImpl(aImpl)
{
  AssertIsOnMainThread();
  MOZ_ASSERT(aImpl);
}

PushManager::PushManager(const nsAString& aScope)
  : mScope(aScope)
{
#ifdef DEBUG
  // There's only one global on a worker, so we don't need to pass a global
  // object to the constructor.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
#endif
}

PushManager::~PushManager()
{}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushManager, mGlobal, mImpl)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
PushManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushManagerBinding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<PushManager>
PushManager::Constructor(GlobalObject& aGlobal,
                         const nsAString& aScope,
                         ErrorResult& aRv)
{
  if (!NS_IsMainThread()) {
    RefPtr<PushManager> ret = new PushManager(aScope);
    return ret.forget();
  }

  RefPtr<PushManagerImpl> impl = PushManagerImpl::Constructor(aGlobal,
                                                              aGlobal.Context(),
                                                              aScope, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<PushManager> ret = new PushManager(global, impl);

  return ret.forget();
}

already_AddRefed<Promise>
PushManager::Subscribe(ErrorResult& aRv)
{
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->Subscribe(aRv);
  }

  return PerformSubscriptionActionFromWorker(SubscribeAction, aRv);
}

already_AddRefed<Promise>
PushManager::GetSubscription(ErrorResult& aRv)
{
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->GetSubscription(aRv);
  }

  return PerformSubscriptionActionFromWorker(GetSubscriptionAction, aRv);
}

already_AddRefed<Promise>
PushManager::PermissionState(ErrorResult& aRv)
{
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->PermissionState(aRv);
  }

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
    p->MaybeReject(worker->GetJSContext(), JS::UndefinedHandleValue);
    return p.forget();
  }

  RefPtr<PermissionStateRunnable> r =
    new PermissionStateRunnable(proxy);
  NS_DispatchToMainThread(r);

  return p.forget();
}

already_AddRefed<Promise>
PushManager::PerformSubscriptionActionFromWorker(
  SubscriptionAction aAction, ErrorResult& aRv)
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
    p->MaybeReject(NS_ERROR_DOM_PUSH_ABORT_ERR);
    return p.forget();
  }

  RefPtr<GetSubscriptionRunnable> r =
    new GetSubscriptionRunnable(proxy, mScope, aAction);
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

  return p.forget();
}

} // namespace dom
} // namespace mozilla
