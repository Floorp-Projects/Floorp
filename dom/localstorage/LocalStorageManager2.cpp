/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorageManager2.h"

#include "LSObject.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

namespace {

class RequestResolver final : public LSRequestChildCallback {
  RefPtr<Promise> mPromise;

 public:
  explicit RequestResolver(Promise* aPromise) : mPromise(aPromise) {}

  NS_INLINE_DECL_REFCOUNTING(mozilla::dom::RequestResolver, override);

 private:
  ~RequestResolver() = default;

  void HandleResponse(nsresult aResponse);

  void HandleResponse(const NullableDatastoreId& aDatastoreId);

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

nsresult CheckedPrincipalToPrincipalInfo(nsIPrincipal* aPrincipal,
                                         PrincipalInfo& aPrincipalInfo) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);

  nsresult rv = PrincipalToPrincipalInfo(aPrincipal, &aPrincipalInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aPrincipalInfo.type() != PrincipalInfo::TContentPrincipalInfo &&
      aPrincipalInfo.type() != PrincipalInfo::TSystemPrincipalInfo) {
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
                                      Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  // This method was created as part of the e10s-ification of the old LS
  // implementation to perform a preload in the content/current process.  That's
  // not how things work in LSNG.  Instead everything happens in the parent
  // process, triggered by the official preloading spot,
  // ContentParent::AboutToLoadHttpFtpWyciwygDocumentForChild.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
LocalStorageManager2::CreateStorage(mozIDOMWindow* aWindow,
                                    nsIPrincipal* aPrincipal,
                                    const nsAString& aDocumentURI,
                                    bool aPrivate, Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  nsCOMPtr<nsPIDOMWindowInner> inner = nsPIDOMWindowInner::From(aWindow);

  RefPtr<LSObject> object;
  nsresult rv = LSObject::CreateForPrincipal(inner, aPrincipal, aDocumentURI,
                                             aPrivate, getter_AddRefs(object));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  object.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::GetStorage(mozIDOMWindow* aWindow,
                                 nsIPrincipal* aPrincipal, bool aPrivate,
                                 Storage** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
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
                              nsISupports** _retval) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPrincipal);
  MOZ_ASSERT(_retval);

  nsresult rv;

  RefPtr<Promise> promise;

  if (aContext) {
    rv = CreatePromise(aContext, getter_AddRefs(promise));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  LSRequestPrepareDatastoreParams params;
  params.createIfNotExists() = false;

  rv = CheckedPrincipalToPrincipalInfo(aPrincipal, params.principalInfo());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = StartRequest(promise, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
LocalStorageManager2::IsPreloaded(nsIPrincipal* aPrincipal, JSContext* aContext,
                                  nsISupports** _retval) {
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

  rv = StartSimpleRequest(promise, params);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  promise.forget(_retval);
  return NS_OK;
}

nsresult LocalStorageManager2::StartRequest(Promise* aPromise,
                                            const LSRequestParams& aParams) {
  MOZ_ASSERT(NS_IsMainThread());

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RequestResolver> resolver = new RequestResolver(aPromise);

  auto actor = new LSRequestChild(resolver);

  if (!backgroundActor->SendPBackgroundLSRequestConstructor(actor, aParams)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult LocalStorageManager2::StartSimpleRequest(
    Promise* aPromise, const LSSimpleRequestParams& aParams) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aPromise);

  PBackgroundChild* backgroundActor =
      BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!backgroundActor)) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<SimpleRequestResolver> resolver = new SimpleRequestResolver(aPromise);

  auto actor = new LSSimpleRequestChild(resolver);

  if (!backgroundActor->SendPBackgroundLSSimpleRequestConstructor(actor,
                                                                  aParams)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void RequestResolver::HandleResponse(nsresult aResponse) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPromise) {
    return;
  }

  mPromise->MaybeReject(aResponse);
}

void RequestResolver::HandleResponse(const NullableDatastoreId& aDatastoreId) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mPromise) {
    return;
  }

  switch (aDatastoreId.type()) {
    case NullableDatastoreId::Tnull_t:
      mPromise->MaybeResolve(JS::NullHandleValue);
      break;

    case NullableDatastoreId::Tuint64_t:
      mPromise->MaybeResolve(aDatastoreId.get_uint64_t());
      break;

    default:
      MOZ_CRASH("Unknown datastore id type!");
  }
}

void RequestResolver::OnResponse(const LSRequestResponse& aResponse) {
  MOZ_ASSERT(NS_IsMainThread());

  switch (aResponse.type()) {
    case LSRequestResponse::Tnsresult:
      HandleResponse(aResponse.get_nsresult());
      break;

    case LSRequestResponse::TLSRequestPrepareDatastoreResponse:
      HandleResponse(
          aResponse.get_LSRequestPrepareDatastoreResponse().datastoreId());
      break;
    default:
      MOZ_CRASH("Unknown response type!");
  }
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

    default:
      MOZ_CRASH("Unknown response type!");
  }
}

}  // namespace dom
}  // namespace mozilla
