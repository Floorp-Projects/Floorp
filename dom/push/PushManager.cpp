/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushManager.h"

#include "mozilla/Base64.h"
#include "mozilla/Preferences.h"
#include "mozilla/Components.h"
#include "mozilla/Unused.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "mozilla/dom/PushManagerBinding.h"
#include "mozilla/dom/PushSubscription.h"
#include "mozilla/dom/PushSubscriptionOptionsBinding.h"
#include "mozilla/dom/PushUtil.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ServiceWorker.h"
#include "mozilla/dom/WorkerRunnable.h"
#include "mozilla/dom/WorkerScope.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseWorkerProxy.h"

#include "nsIGlobalObject.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIPushService.h"

#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla::dom {

namespace {

nsresult GetPermissionState(nsIPrincipal* aPrincipal, PermissionState& aState) {
  nsCOMPtr<nsIPermissionManager> permManager =
      mozilla::components::PermissionManager::Service();

  if (!permManager) {
    return NS_ERROR_FAILURE;
  }
  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  nsresult rv = permManager->TestExactPermissionFromPrincipal(
      aPrincipal, "desktop-notification"_ns, &permission);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (permission == nsIPermissionManager::ALLOW_ACTION ||
      Preferences::GetBool("dom.push.testing.ignorePermission", false)) {
    aState = PermissionState::Granted;
  } else if (permission == nsIPermissionManager::DENY_ACTION) {
    aState = PermissionState::Denied;
  } else {
    aState = PermissionState::Prompt;
  }

  return NS_OK;
}

nsresult GetSubscriptionParams(nsIPushSubscription* aSubscription,
                               nsAString& aEndpoint,
                               nsTArray<uint8_t>& aRawP256dhKey,
                               nsTArray<uint8_t>& aAuthSecret,
                               nsTArray<uint8_t>& aAppServerKey) {
  if (!aSubscription) {
    return NS_OK;
  }

  nsresult rv = aSubscription->GetEndpoint(aEndpoint);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aSubscription->GetKey(u"p256dh"_ns, aRawP256dhKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = aSubscription->GetKey(u"auth"_ns, aAuthSecret);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = aSubscription->GetKey(u"appServer"_ns, aAppServerKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

class GetSubscriptionResultRunnable final : public WorkerRunnable {
 public:
  GetSubscriptionResultRunnable(WorkerPrivate* aWorkerPrivate,
                                RefPtr<PromiseWorkerProxy>&& aProxy,
                                nsresult aStatus, const nsAString& aEndpoint,
                                const nsAString& aScope,
                                Nullable<EpochTimeStamp>&& aExpirationTime,
                                nsTArray<uint8_t>&& aRawP256dhKey,
                                nsTArray<uint8_t>&& aAuthSecret,
                                nsTArray<uint8_t>&& aAppServerKey)
      : WorkerRunnable(aWorkerPrivate, "GetSubscriptionResultRunnable"),
        mProxy(std::move(aProxy)),
        mStatus(aStatus),
        mEndpoint(aEndpoint),
        mScope(aScope),
        mExpirationTime(std::move(aExpirationTime)),
        mRawP256dhKey(std::move(aRawP256dhKey)),
        mAuthSecret(std::move(aAuthSecret)),
        mAppServerKey(std::move(aAppServerKey)) {}

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    {
      MutexAutoLock lock(mProxy->Lock());
      if (mProxy->CleanedUp()) {
        return true;
      }
    }
    RefPtr<Promise> promise = mProxy->WorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      if (mEndpoint.IsEmpty()) {
        promise->MaybeResolve(JS::NullHandleValue);
      } else {
        RefPtr<PushSubscription> sub = new PushSubscription(
            nullptr, mEndpoint, mScope, std::move(mExpirationTime),
            std::move(mRawP256dhKey), std::move(mAuthSecret),
            std::move(mAppServerKey));
        promise->MaybeResolve(sub);
      }
    } else if (NS_ERROR_GET_MODULE(mStatus) == NS_ERROR_MODULE_DOM_PUSH) {
      promise->MaybeReject(mStatus);
    } else {
      promise->MaybeReject(NS_ERROR_DOM_PUSH_ABORT_ERR);
    }

    mProxy->CleanUp();

    return true;
  }

 private:
  ~GetSubscriptionResultRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  nsString mEndpoint;
  nsString mScope;
  Nullable<EpochTimeStamp> mExpirationTime;
  nsTArray<uint8_t> mRawP256dhKey;
  nsTArray<uint8_t> mAuthSecret;
  nsTArray<uint8_t> mAppServerKey;
};

class GetSubscriptionCallback final : public nsIPushSubscriptionCallback {
 public:
  NS_DECL_ISUPPORTS

  explicit GetSubscriptionCallback(PromiseWorkerProxy* aProxy,
                                   const nsAString& aScope)
      : mProxy(aProxy), mScope(aScope) {}

  NS_IMETHOD
  OnPushSubscription(nsresult aStatus,
                     nsIPushSubscription* aSubscription) override {
    AssertIsOnMainThread();
    MOZ_ASSERT(mProxy, "OnPushSubscription() called twice?");

    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return NS_OK;
    }

    nsAutoString endpoint;
    nsTArray<uint8_t> rawP256dhKey, authSecret, appServerKey;
    if (NS_SUCCEEDED(aStatus)) {
      aStatus = GetSubscriptionParams(aSubscription, endpoint, rawP256dhKey,
                                      authSecret, appServerKey);
    }

    WorkerPrivate* worker = mProxy->GetWorkerPrivate();
    RefPtr<GetSubscriptionResultRunnable> r = new GetSubscriptionResultRunnable(
        worker, std::move(mProxy), aStatus, endpoint, mScope,
        std::move(mExpirationTime), std::move(rawP256dhKey),
        std::move(authSecret), std::move(appServerKey));
    if (!r->Dispatch()) {
      return NS_ERROR_UNEXPECTED;
    }

    return NS_OK;
  }

  // Convenience method for use in this file.
  void OnPushSubscriptionError(nsresult aStatus) {
    Unused << NS_WARN_IF(NS_FAILED(OnPushSubscription(aStatus, nullptr)));
  }

 protected:
  ~GetSubscriptionCallback() = default;

 private:
  RefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
  Nullable<EpochTimeStamp> mExpirationTime;
};

NS_IMPL_ISUPPORTS(GetSubscriptionCallback, nsIPushSubscriptionCallback)

class GetSubscriptionRunnable final : public Runnable {
 public:
  GetSubscriptionRunnable(PromiseWorkerProxy* aProxy, const nsAString& aScope,
                          PushManager::SubscriptionAction aAction,
                          nsTArray<uint8_t>&& aAppServerKey)
      : Runnable("dom::GetSubscriptionRunnable"),
        mProxy(aProxy),
        mScope(aScope),
        mAction(aAction),
        mAppServerKey(std::move(aAppServerKey)) {}

  NS_IMETHOD
  Run() override {
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

    RefPtr<GetSubscriptionCallback> callback =
        new GetSubscriptionCallback(mProxy, mScope);

    PermissionState state;
    nsresult rv = GetPermissionState(principal, state);
    if (NS_FAILED(rv)) {
      callback->OnPushSubscriptionError(NS_ERROR_FAILURE);
      return NS_OK;
    }

    if (state != PermissionState::Granted) {
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
      if (mAppServerKey.IsEmpty()) {
        rv = service->Subscribe(mScope, principal, callback);
      } else {
        rv = service->SubscribeWithKey(mScope, principal, mAppServerKey,
                                       callback);
      }
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
  ~GetSubscriptionRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsString mScope;
  PushManager::SubscriptionAction mAction;
  nsTArray<uint8_t> mAppServerKey;
};

class PermissionResultRunnable final : public WorkerRunnable {
 public:
  PermissionResultRunnable(PromiseWorkerProxy* aProxy, nsresult aStatus,
                           PermissionState aState)
      : WorkerRunnable(aProxy->GetWorkerPrivate(), "PermissionResultRunnable"),
        mProxy(aProxy),
        mStatus(aStatus),
        mState(aState) {
    AssertIsOnMainThread();
  }

  bool WorkerRun(JSContext* aCx, WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();

    RefPtr<Promise> promise = mProxy->WorkerPromise();
    if (NS_SUCCEEDED(mStatus)) {
      promise->MaybeResolve(mState);
    } else {
      promise->MaybeRejectWithUndefined();
    }

    mProxy->CleanUp();

    return true;
  }

 private:
  ~PermissionResultRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
  nsresult mStatus;
  PermissionState mState;
};

class PermissionStateRunnable final : public Runnable {
 public:
  explicit PermissionStateRunnable(PromiseWorkerProxy* aProxy)
      : Runnable("dom::PermissionStateRunnable"), mProxy(aProxy) {}

  NS_IMETHOD
  Run() override {
    AssertIsOnMainThread();
    MutexAutoLock lock(mProxy->Lock());
    if (mProxy->CleanedUp()) {
      return NS_OK;
    }

    PermissionState state;
    nsresult rv =
        GetPermissionState(mProxy->GetWorkerPrivate()->GetPrincipal(), state);

    RefPtr<PermissionResultRunnable> r =
        new PermissionResultRunnable(mProxy, rv, state);

    // This can fail if the worker thread is already shutting down, but there's
    // nothing we can do in that case.
    Unused << NS_WARN_IF(!r->Dispatch());

    return NS_OK;
  }

 private:
  ~PermissionStateRunnable() = default;

  RefPtr<PromiseWorkerProxy> mProxy;
};

}  // anonymous namespace

PushManager::PushManager(nsIGlobalObject* aGlobal, PushManagerImpl* aImpl)
    : mGlobal(aGlobal), mImpl(aImpl) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aImpl);
}

PushManager::PushManager(const nsAString& aScope) : mScope(aScope) {
#ifdef DEBUG
  // There's only one global on a worker, so we don't need to pass a global
  // object to the constructor.
  WorkerPrivate* worker = GetCurrentThreadWorkerPrivate();
  MOZ_ASSERT(worker);
  worker->AssertIsOnWorkerThread();
#endif
}

PushManager::~PushManager() = default;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushManager, mGlobal, mImpl)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushManager)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject* PushManager::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return PushManager_Binding::Wrap(aCx, this, aGivenProto);
}

// static
already_AddRefed<PushManager> PushManager::Constructor(GlobalObject& aGlobal,
                                                       const nsAString& aScope,
                                                       ErrorResult& aRv) {
  if (!NS_IsMainThread()) {
    RefPtr<PushManager> ret = new PushManager(aScope);
    return ret.forget();
  }

  RefPtr<PushManagerImpl> impl =
      PushManagerImpl::Constructor(aGlobal, aGlobal.Context(), aScope, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<PushManager> ret = new PushManager(global, impl);

  return ret.forget();
}

bool PushManager::IsEnabled(JSContext* aCx, JSObject* aGlobal) {
  return StaticPrefs::dom_push_enabled() && ServiceWorkerVisible(aCx, aGlobal);
}

already_AddRefed<Promise> PushManager::Subscribe(
    const PushSubscriptionOptionsInit& aOptions, ErrorResult& aRv) {
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->Subscribe(aOptions, aRv);
  }

  return PerformSubscriptionActionFromWorker(SubscribeAction, aOptions, aRv);
}

already_AddRefed<Promise> PushManager::GetSubscription(ErrorResult& aRv) {
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->GetSubscription(aRv);
  }

  return PerformSubscriptionActionFromWorker(GetSubscriptionAction, aRv);
}

already_AddRefed<Promise> PushManager::PermissionState(
    const PushSubscriptionOptionsInit& aOptions, ErrorResult& aRv) {
  if (mImpl) {
    MOZ_ASSERT(NS_IsMainThread());
    return mImpl->PermissionState(aOptions, aRv);
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
    p->MaybeRejectWithUndefined();
    return p.forget();
  }

  RefPtr<PermissionStateRunnable> r = new PermissionStateRunnable(proxy);
  NS_DispatchToMainThread(r);

  return p.forget();
}

already_AddRefed<Promise> PushManager::PerformSubscriptionActionFromWorker(
    SubscriptionAction aAction, ErrorResult& aRv) {
  RootedDictionary<PushSubscriptionOptionsInit> options(RootingCx());
  return PerformSubscriptionActionFromWorker(aAction, options, aRv);
}

already_AddRefed<Promise> PushManager::PerformSubscriptionActionFromWorker(
    SubscriptionAction aAction, const PushSubscriptionOptionsInit& aOptions,
    ErrorResult& aRv) {
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

  nsTArray<uint8_t> appServerKey;
  if (!aOptions.mApplicationServerKey.IsNull()) {
    nsresult rv = NormalizeAppServerKey(aOptions.mApplicationServerKey.Value(),
                                        appServerKey);
    if (NS_FAILED(rv)) {
      p->MaybeReject(rv);
      return p.forget();
    }
  }

  RefPtr<GetSubscriptionRunnable> r = new GetSubscriptionRunnable(
      proxy, mScope, aAction, std::move(appServerKey));
  MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(r));

  return p.forget();
}

nsresult PushManager::NormalizeAppServerKey(
    const OwningArrayBufferViewOrArrayBufferOrString& aSource,
    nsTArray<uint8_t>& aAppServerKey) {
  if (aSource.IsString()) {
    NS_ConvertUTF16toUTF8 base64Key(aSource.GetAsString());
    FallibleTArray<uint8_t> decodedKey;
    nsresult rv = Base64URLDecode(
        base64Key, Base64URLDecodePaddingPolicy::Reject, decodedKey);
    if (NS_FAILED(rv)) {
      return NS_ERROR_DOM_INVALID_CHARACTER_ERR;
    }
    aAppServerKey = decodedKey;
  } else {
    if (!AppendTypedArrayDataTo(aSource, aAppServerKey)) {
      return NS_ERROR_DOM_PUSH_INVALID_KEY_ERR;
    }
  }
  if (aAppServerKey.IsEmpty()) {
    return NS_ERROR_DOM_PUSH_INVALID_KEY_ERR;
  }
  return NS_OK;
}

}  // namespace mozilla::dom
