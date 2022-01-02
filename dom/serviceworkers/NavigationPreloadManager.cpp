/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NavigationPreloadManager.h"
#include "ServiceWorkerUtils.h"
#include "nsNetUtil.h"
#include "mozilla/dom/NavigationPreloadManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/ipc/MessageChannel.h"

namespace mozilla::dom {

using mozilla::ipc::ResponseRejectReason;

NS_IMPL_CYCLE_COLLECTING_ADDREF(NavigationPreloadManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(NavigationPreloadManager)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(NavigationPreloadManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(NavigationPreloadManager, mGlobal)

/* static */
bool NavigationPreloadManager::IsValidHeader(const nsACString& aHeader) {
  return NS_IsReasonableHTTPHeaderValue(aHeader);
}

NavigationPreloadManager::NavigationPreloadManager(
    nsCOMPtr<nsIGlobalObject>&& aGlobal,
    RefPtr<ServiceWorkerRegistration::Inner>& aInner)
    : mGlobal(aGlobal), mInner(aInner) {}

JSObject* NavigationPreloadManager::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return NavigationPreloadManager_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> NavigationPreloadManager::SetEnabled(bool aEnabled) {
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), result);

  if (NS_WARN_IF(result.Failed())) {
    result.SuppressException();
    return promise.forget();
  }

  if (!mInner) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mInner->SetNavigationPreloadEnabled(
      aEnabled,
      [promise](bool aSuccess) {
        if (aSuccess) {
          promise->MaybeResolveWithUndefined();
          return;
        }
        promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      },
      [promise](ErrorResult&& aRv) { promise->MaybeReject(std::move(aRv)); });

  return promise.forget();
}

already_AddRefed<Promise> NavigationPreloadManager::Enable() {
  return SetEnabled(true);
}

already_AddRefed<Promise> NavigationPreloadManager::Disable() {
  return SetEnabled(false);
}

already_AddRefed<Promise> NavigationPreloadManager::SetHeaderValue(
    const nsACString& aHeader) {
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), result);

  if (NS_WARN_IF(result.Failed())) {
    result.SuppressException();
    return promise.forget();
  }

  if (!IsValidHeader(aHeader)) {
    result.ThrowTypeError<MSG_INVALID_HEADER_VALUE>(aHeader);
    promise->MaybeReject(std::move(result));
    return promise.forget();
  }

  if (!mInner) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mInner->SetNavigationPreloadHeader(
      nsAutoCString(aHeader),
      [promise](bool aSuccess) {
        if (aSuccess) {
          promise->MaybeResolveWithUndefined();
          return;
        }
        promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
      },
      [promise](ErrorResult&& aRv) { promise->MaybeReject(std::move(aRv)); });

  return promise.forget();
}

already_AddRefed<Promise> NavigationPreloadManager::GetState() {
  ErrorResult result;
  RefPtr<Promise> promise = Promise::Create(GetParentObject(), result);

  if (NS_WARN_IF(result.Failed())) {
    result.SuppressException();
    return promise.forget();
  }

  if (!mInner) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  mInner->GetNavigationPreloadState(
      [promise](NavigationPreloadState&& aState) {
        promise->MaybeResolve(std::move(aState));
      },
      [promise](ErrorResult&& aRv) { promise->MaybeReject(std::move(aRv)); });

  return promise.forget();
}

}  // namespace mozilla::dom
