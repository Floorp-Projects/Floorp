/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushManager.h"

#include "mozilla/Base64.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/unused.h"
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushSubscription.h"
#include "mozilla/dom/ServiceWorkerGlobalScopeBinding.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"

#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPushService.h"

#include "nsComponentManagerUtils.h"
#include "nsFrameMessageManager.h"
#include "nsContentCID.h"

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

} // anonymous namespace

PushManager::PushManager(nsIGlobalObject* aGlobal, const nsAString& aScope)
  : mGlobal(aGlobal), mScope(aScope)
{
  AssertIsOnMainThread();
}

PushManager::~PushManager()
{}

JSObject*
PushManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  // XXXnsm I don't know if this is the right way to do it, but I want to assert
  // that an implementation has been set before this object gets exposed to JS.
  MOZ_ASSERT(mImpl);
  return PushManagerBinding::Wrap(aCx, this, aGivenProto);
}

void
PushManager::SetPushManagerImpl(PushManagerImpl& foo, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mImpl);
  mImpl = &foo;
}

already_AddRefed<Promise>
PushManager::Subscribe(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->Subscribe(aRv);
}

already_AddRefed<Promise>
PushManager::GetSubscription(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->GetSubscription(aRv);
}

already_AddRefed<Promise>
PushManager::PermissionState(ErrorResult& aRv)
{
  MOZ_ASSERT(mImpl);
  return mImpl->PermissionState(aRv);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushManager, mGlobal, mImpl)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// WorkerPushManager

WorkerPushManager::WorkerPushManager(const nsAString& aScope)
  : mScope(aScope)
{
}

JSObject*
WorkerPushManager::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PushManagerBinding_workers::Wrap(aCx, this, aGivenProto);
}

class GetSubscriptionResultRunnable final : public WorkerRunnable
{
public:
  GetSubscriptionResultRunnable(PromiseWorkerProxy* aProxy,
                                nsresult aStatus,
                                const nsAString& aEndpoint,
                                const nsAString& aScope,
                                const nsTArray<uint8_t>& aRawP256dhKey,
                                const nsTArray<uint8_t>& aAuthSecret)
    : WorkerRunnable(aProxy->GetWorkerPrivate(), WorkerThreadModifyBusyCount)
    , mProxy(aProxy)
    , mStatus(aStatus)
    , mEndpoint(aEndpoint)
    , mScope(aScope)
    , mRawP256dhKey(aRawP256dhKey)
    , mAuthSecret(aAuthSecret)
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
            new PushSubscription(nullptr, mEndpoint, mScope, mRawP256dhKey,
                                 mAuthSecret);
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

    RefPtr<PromiseWorkerProxy> proxy = mProxy.forget();

    MutexAutoLock lock(proxy->Lock());
    if (proxy->CleanedUp()) {
      return NS_OK;
    }

    nsAutoString endpoint;
    nsTArray<uint8_t> rawP256dhKey, authSecret;
    if (NS_SUCCEEDED(aStatus)) {
      aStatus = GetSubscriptionParams(aSubscription, endpoint, rawP256dhKey,
                                      authSecret);
    }

    RefPtr<GetSubscriptionResultRunnable> r =
      new GetSubscriptionResultRunnable(proxy,
                                        aStatus,
                                        endpoint,
                                        mScope,
                                        rawP256dhKey,
                                        authSecret);
    r->Dispatch();
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
                          WorkerPushManager::SubscriptionAction aAction)
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
      if (mAction == WorkerPushManager::GetSubscriptionAction) {
        callback->OnPushSubscriptionError(NS_OK);
        return NS_OK;
      }
      callback->OnPushSubscriptionError(NS_ERROR_DOM_PUSH_DENIED_ERR);
      return NS_OK;
    }

    nsCOMPtr<nsIPushService> service =
      do_GetService("@mozilla.org/push/Service;1");
    if (!service) {
      callback->OnPushSubscriptionError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    if (mAction == WorkerPushManager::SubscribeAction) {
      rv = service->Subscribe(mScope, principal, callback);
    } else {
      MOZ_ASSERT(mAction == WorkerPushManager::GetSubscriptionAction);
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
  WorkerPushManager::SubscriptionAction mAction;
};

already_AddRefed<Promise>
WorkerPushManager::PerformSubscriptionAction(SubscriptionAction aAction, ErrorResult& aRv)
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

already_AddRefed<Promise>
WorkerPushManager::Subscribe(ErrorResult& aRv)
{
  return PerformSubscriptionAction(SubscribeAction, aRv);
}

already_AddRefed<Promise>
WorkerPushManager::GetSubscription(ErrorResult& aRv)
{
  return PerformSubscriptionAction(GetSubscriptionAction, aRv);
}

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
    r->Dispatch();
    return NS_OK;
  }

private:
  ~PermissionStateRunnable()
  {}

  RefPtr<PromiseWorkerProxy> mProxy;
};

already_AddRefed<Promise>
WorkerPushManager::PermissionState(ErrorResult& aRv)
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
    p->MaybeReject(worker->GetJSContext(), JS::UndefinedHandleValue);
    return p.forget();
  }

  RefPtr<PermissionStateRunnable> r =
    new PermissionStateRunnable(proxy);
  NS_DispatchToMainThread(r);

  return p.forget();
}

WorkerPushManager::~WorkerPushManager()
{}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WorkerPushManager)
NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkerPushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkerPushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkerPushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END
} // namespace dom
} // namespace mozilla
