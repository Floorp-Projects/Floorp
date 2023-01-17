/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageManager2.h"

// Local includes
#include "ActorsChild.h"
#include "LSObject.h"

// Global includes
#include <utility>
#include "MainThreadUtils.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/MacroForEach.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/RefPtr.h"
#include "mozilla/RemoteLazyInputStreamThread.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/PBackgroundLSRequest.h"
#include "mozilla/dom/PBackgroundLSSharedTypes.h"
#include "mozilla/dom/PBackgroundLSSimpleRequest.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsError.h"
#include "nsIEventTarget.h"
#include "nsILocalStorageManager.h"
#include "nsIPrincipal.h"
#include "nsIRunnable.h"
#include "nsPIDOMWindow.h"
#include "nsStringFwd.h"
#include "nsThreadUtils.h"
#include "nscore.h"
#include "xpcpublic.h"

namespace mozilla::dom {

namespace {

class AsyncRequestHelper final : public Runnable,
                                 public LSRequestChildCallback {
  enum class State {
    /**
     * The AsyncRequestHelper has been created and dispatched to the
     * RemoteLazyInputStream Thread.
     */
    Initial,
    /**
     * Start() has been invoked on the RemoteLazyInputStream Thread and
     * LocalStorageManager2::StartRequest has been invoked from there, sending
     * an IPC message to PBackground to service the request.  We stay in this
     * state until a response is received.
     */
    ResponsePending,
    /**
     * A response has been received and AsyncRequestHelper has been dispatched
     * back to the owning event target to call Finish().
     */
    Finishing,
    /**
     * Finish() has been called on the main thread. The promise will be resolved
     * according to the received response.
     */
    Complete
  };

  // The object we are issuing a request on behalf of.  Present because of the
  // need to invoke LocalStorageManager2::StartRequest off the main thread.
  // Dropped on return to the main-thread in Finish().
  RefPtr<LocalStorageManager2> mManager;
  // The thread the AsyncRequestHelper was created on.  This should be the main
  // thread.
  nsCOMPtr<nsIEventTarget> mOwningEventTarget;
  // The IPC actor handling the request with standard IPC allocation rules.
  // Our reference is nulled in OnResponse which corresponds to the actor's
  // __destroy__ method.
  LSRequestChild* mActor;
  RefPtr<Promise> mPromise;
  const LSRequestParams mParams;
  LSRequestResponse mResponse;
  nsresult mResultCode;
  State mState;

 public:
  AsyncRequestHelper(LocalStorageManager2* aManager, Promise* aPromise,
                     const LSRequestParams& aParams)
      : Runnable("dom::LocalStorageManager2::AsyncRequestHelper"),
        mManager(aManager),
        mOwningEventTarget(GetCurrentEventTarget()),
        mActor(nullptr),
        mPromise(aPromise),
        mParams(aParams),
        mResultCode(NS_OK),
        mState(State::Initial) {}

  bool IsOnOwningThread() const {
    MOZ_ASSERT(mOwningEventTarget);

    bool current;
    return NS_SUCCEEDED(mOwningEventTarget->IsOnCurrentThread(&current)) &&
           current;
  }

  void AssertIsOnOwningThread() const {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(IsOnOwningThread());
  }

  nsresult Dispatch();

 private:
  ~AsyncRequestHelper() = default;

  nsresult Start();

  void Finish();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIRUNNABLE

  // LSRequestChildCallback
  void OnResponse(const LSRequestResponse& aResponse) override;
};

class SimpleRequestResolver final : public LSSimpleRequestChildCallback {
  RefPtr<Promise> mPromise;

 public:
  explicit SimpleRequestResolver(Promise* aPromise) : mPromise(aPromise) {}

  NS_INLINE_DECL_REFCOUNTING(SimpleRequestResolver, override);

 private:
  ~SimpleRequestResolver() = default;

  void HandleResponse(nsresult aResponse);

  void HandleResponse(bool aResponse);

  void HandleResponse(const nsTArray<LSItemInfo>& aResponse);

  // LSRequestChildCallback
  void OnResponse(const LSSimpleRequestResponse& aResponse) override;
};

nsresult CreatePromise(JSContext* aContext, Promise** aPromise) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aContext);

  nsIGlobalObject* global =
      xpc::NativeGlobal(JS::CurrentGlobalOrNull(aContext));
  if (NS_WARN_IF(!global)) {
    return NS_ERROR_FAILURE;
  }

  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(global, result);
  if (result.Failed()) {
    return result.StealNSResult();
  }

  promise.forget(aPromise);
  return NS_OK;
}

nsresult CheckedPrincipalToPrincipalInfo(
    nsIPrincipal* aPrincipal, mozilla::ipc::PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!quota::QuotaManager::IsPrincipalInfoValid(aPrincipalInfo))) {
    return NS_ERROR_FAILURE;
  }

  if (aPrincipalInfo.type() !=
          mozilla::ipc::PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo.type() !=
          mozilla::ipc::PrincipalInfo::TSystemPrincipalInfo) {
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

}  // namespace

LocalStorageManager2::LocalStorageManager2() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(NextGenLocalStorageEnabled());
}

LocalStorageManager2::~LocalStorageManager2() { MOZ_ASSERT(NS_IsMainThread()); }

NS_IMPL_ISUPPORTS(LocalStorageManager2, nsIDOMStorageManager,
                  nsILocalStorageManager)

NS_IMETHODIMP
LocalStorageManager2::PrecacheStorage(nsIPrincipal* aPrincipal,
                                      nsIPrincipal* aStoragePrincipal,
                                      Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStoragePrincipal);
  MOZ_ASSERT(_retval);

  // This method was created as part of the e10s-ification of the old LS
  // implementation to perform a preload in the content/current process.  That's
  // not how things work in LSNG.  Instead everything happens in the parent
  // process, triggered by the official preloading spot,
  // ContentParent::AboutToLoadHttpFtpDocumentForChild.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CreateStorage(mozIDOMWindow* aWindow,
                                    nsIPrincipal* aPrincipal,
                                    nsIPrincipal* aStoragePrincipal,
                                    const nsAString& aDocumentURI,
                                    bool aPrivate, Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStoragePrincipal);
  MOZ_ASSERT(_retval);

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<LSObject> object;
  nsresult rv = LSObject::CreateForPrincipal(inner, aPrincipal,
                                             aStoragePrincipal, aDocumentURI,
                                             aPrivate, getter_AddRefs(object));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  object.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::GetStorage(mozIDOMWindow* aWindow,
                                 nsIPrincipal* aPrincipal,
                                 nsIPrincipal* aStoragePrincipal, bool aPrivate,
                                 Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStoragePrincipal);
  MOZ_ASSERT(_retval);

  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CloneStorage(Storage* aStorageToCloneFrom) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aStorageToCloneFrom);

  // Cloning is specific to sessionStorage; state is forked when a new tab is
  // opened from an existing tab.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CheckStorage(nsIPrincipal* aPrincipal, Storage* aStorage,
                                   bool* _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(aStorage);
  MOZ_ASSERT(_retval);

  // Only used by sessionStorage.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::GetNextGenLocalStorageEnabled(bool* aResult) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aResult);

  *aResult = NextGenLocalStorageEnabled();
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::Preload(nsIPrincipal* aPrincipal, JSContext* aContext,
                              Promise** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  nsCString originAttrSuffix;
  nsCString originKey;
  nsresult rv = aPrincipal->GetStorageOriginKey(originKey);
  aPrincipal->OriginAttributesRef().CreateSuffix(originAttrSuffix);
  if (NS_FAILED(rv)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  mozilla::ipc::PrincipalInfo principalInfo;
  rv = CheckedPrincipalToPrincipalInfo(aPrincipal, principalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  RefPtr<Promise> promise;

  if (aContext) {
    rv = CreatePromise(aContext, getter_AddRefs(promise));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  LSRequestCommonParams commonParams;
  commonParams.principalInfo() = principalInfo;
  commonParams.storagePrincipalInfo() = principalInfo;
  commonParams.originKey() = originKey;

  LSRequestPreloadDatastoreParams params(commonParams);

  RefPtr<AsyncRequestHelper> helper =
      new AsyncRequestHelper(this, promise, params);

  // This will start and finish the async request on the RemoteLazyInputStream
  // thread.
  // This must be done on RemoteLazyInputStream Thread because it's very likely
  // that a content process will issue a prepare datastore request for the same
  // principal while blocking the content process on the main thread.
  // There would be a potential for deadlock if the preloading was initialized
  // from the main thread of the parent process and a11y issued a synchronous
  // message from the parent process to the content process (approximately at
  // the same time) because the preload request wouldn't be able to respond
  // to the Ready message by sending the Finish message which is needed to
  // finish the preload request and unblock the prepare datastore request.
  rv = helper->Dispatch();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::IsPreloaded(nsIPrincipal* aPrincipal, JSContext* aContext,
                                  Promise** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Promise> promise;
  nsresult rv = CreatePromise(aContext, getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LSSimpleRequestPreloadedParams params;

  rv = CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  params.storagePrincipalInfo() = params.principalInfo();

  rv = StartSimpleRequest(promise, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::GetState(nsIPrincipal* aPrincipal, JSContext* aContext,
                               Promise** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  RefPtr<Promise> promise;
  nsresult rv = CreatePromise(aContext, getter_AddRefs(promise));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  LSSimpleRequestGetStateParams params;

  rv = CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  params.storagePrincipalInfo() = params.principalInfo();

  rv = StartSimpleRequest(promise, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(_retval);
  return NS_OK;
}

LSRequestChild* LocalStorageManager2::StartRequest(
    const LSRequestParams& aParams, LSRequestChildCallback* aCallback) {
  AssertIsOnDOMFileThread();

  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return nullptr;
  }

  auto actor = new LSRequestChild();

  if (!backgroundActor->SendPBackgroundLSRequestConstructor(actor, aParams)) {
    return nullptr;
  }

  // Must set callback after calling SendPBackgroundLSRequestConstructor since
  // it can be called synchronously when SendPBackgroundLSRequestConstructor
  // fails.
  actor->SetCallback(aCallback);

  return actor;
}

nsresult LocalStorageManager2::StartSimpleRequest(
    Promise* aPromise, const LSSimpleRequestParams& aParams) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);

  mozilla::ipc::PBackgroundChild* backgroundActor =
      mozilla::ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  auto actor = new LSSimpleRequestChild();

  if (!backgroundActor->SendPBackgroundLSSimpleRequestConstructor(actor,
                                                                  aParams)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SimpleRequestResolver> resolver = new SimpleRequestResolver(aPromise);

  // Must set callback after calling SendPBackgroundLSRequestConstructor since
  // it can be called synchronously when SendPBackgroundLSRequestConstructor
  // fails.
  actor->SetCallback(resolver);

  return NS_OK;
}

nsresult AsyncRequestHelper::Dispatch() {
  AssertIsOnOwningThread();

  nsCOMPtr<nsIEventTarget> domFileThread =
      RemoteLazyInputStreamThread::GetOrCreate();
  if (NS_WARN_IF(!domFileThread)) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  nsresult rv = domFileThread->Dispatch(this, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult AsyncRequestHelper::Start() {
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::Initial);

  mState = State::ResponsePending;

  LSRequestChild* actor = mManager->StartRequest(mParams, this);
  if (NS_WARN_IF(!actor)) {
    return NS_ERROR_FAILURE;
  }

  mActor = actor;

  return NS_OK;
}

void AsyncRequestHelper::Finish() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mState == State::Finishing);

  if (NS_WARN_IF(NS_FAILED(mResultCode))) {
    if (mPromise) {
      mPromise->MaybeReject(mResultCode);
    }
  } else {
    switch (mResponse.type()) {
      case LSRequestResponse::Tnsresult:
        if (mPromise) {
          mPromise->MaybeReject(mResponse.get_nsresult());
        }
        break;

      case LSRequestResponse::TLSRequestPreloadDatastoreResponse:
        if (mPromise) {
          mPromise->MaybeResolveWithUndefined();
        }
        break;
      default:
        MOZ_CRASH("Unknown response type!");
    }
  }

  mManager = nullptr;

  mState = State::Complete;
}

NS_IMPL_ISUPPORTS_INHERITED0(AsyncRequestHelper, Runnable)

NS_IMETHODIMP
AsyncRequestHelper::Run() {
  nsresult rv;

  switch (mState) {
    case State::Initial:
      rv = Start();
      break;

    case State::Finishing:
      Finish();
      return NS_OK;

    default:
      MOZ_CRASH("Bad state!");
  }

  if (NS_WARN_IF(NS_FAILED(rv)) && mState != State::Finishing) {
    if (NS_SUCCEEDED(mResultCode)) {
      mResultCode = rv;
    }

    mState = State::Finishing;

    if (IsOnOwningThread()) {
      Finish();
    } else {
      MOZ_ALWAYS_SUCCEEDS(
          mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
    }
  }

  return NS_OK;
}

void AsyncRequestHelper::OnResponse(const LSRequestResponse& aResponse) {
  AssertIsOnDOMFileThread();
  MOZ_ASSERT(mState == State::ResponsePending);

  mActor = nullptr;

  mResponse = aResponse;

  mState = State::Finishing;

  MOZ_ALWAYS_SUCCEEDS(mOwningEventTarget->Dispatch(this, NS_DISPATCH_NORMAL));
}

void SimpleRequestResolver::HandleResponse(nsresult aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPromise);

  mPromise->MaybeReject(aResponse);
}

void SimpleRequestResolver::HandleResponse(bool aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPromise);

  mPromise->MaybeResolve(aResponse);
}

[[nodiscard]] static bool ToJSValue(JSContext* aCx,
                                    const nsTArray<LSItemInfo>& aArgument,
                                    JS::MutableHandle<JS::Value> aValue) {
  JS::Rooted<JSObject*> obj(aCx, JS_NewPlainObject(aCx));
  if (!obj) {
    return false;
  }

  for (size_t i = 0; i < aArgument.Length(); ++i) {
    const LSItemInfo& itemInfo = aArgument[i];

    const nsString& key = itemInfo.key();

    JS::Rooted<JS::Value> value(aCx);
    if (!ToJSValue(aCx, itemInfo.value().AsString(), &value)) {
      return false;
    }

    if (!JS_DefineUCProperty(aCx, obj, key.BeginReading(), key.Length(), value,
                             JSPROP_ENUMERATE)) {
      return false;
    }
  }

  aValue.setObject(*obj);
  return true;
}

void SimpleRequestResolver::HandleResponse(
    const nsTArray<LSItemInfo>& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mPromise);

  mPromise->MaybeResolve(aResponse);
}

void SimpleRequestResolver::OnResponse(
    const LSSimpleRequestResponse& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());

  switch (aResponse.type()) {
    case LSSimpleRequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case LSSimpleRequestResponse::TLSSimpleRequestPreloadedResponse:
      HandleResponse(
          aResponse.get_LSSimpleRequestPreloadedResponse().preloaded());
      break;

    case LSSimpleRequestResponse::TLSSimpleRequestGetStateResponse:
      HandleResponse(
          aResponse.get_LSSimpleRequestGetStateResponse().itemInfos());
      break;

    default:
      MOZ_CRASH("Unknown response type!");
  }
}

}  // namespace mozilla::dom
