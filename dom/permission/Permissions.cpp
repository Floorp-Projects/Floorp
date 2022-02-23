/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Permissions.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Components.h"
#include "nsIPermissionManager.h"
#include "PermissionUtils.h"

namespace mozilla::dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Permissions)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Permissions)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Permissions)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Permissions, mWindow)

Permissions::Permissions(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

Permissions::~Permissions() = default;

JSObject* Permissions::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return Permissions_Binding::Wrap(aCx, this, aGivenProto);
}

namespace {

already_AddRefed<PermissionStatus> CreatePermissionStatus(
    JSContext* aCx, JS::Handle<JSObject*> aPermission,
    nsPIDOMWindowInner* aWindow, ErrorResult& aRv) {
  PermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  switch (permission.mName) {
    case PermissionName::Geolocation:
    case PermissionName::Notifications:
    case PermissionName::Push:
    case PermissionName::Persistent_storage:
      return PermissionStatus::Create(aWindow, permission.mName, aRv);

    default:
      MOZ_ASSERT_UNREACHABLE("Unhandled type");
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return nullptr;
  }
}

}  // namespace

already_AddRefed<Promise> Permissions::Query(JSContext* aCx,
                                             JS::Handle<JSObject*> aPermission,
                                             ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<PermissionStatus> status =
      CreatePermissionStatus(aCx, aPermission, mWindow, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_ASSERT(!status);
    return nullptr;
  }

  MOZ_ASSERT(status);
  RefPtr<Promise> promise = Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  promise->MaybeResolve(status);
  return promise.forget();
}

/* static */
nsresult Permissions::RemovePermission(nsIPrincipal* aPrincipal,
                                       const nsACString& aPermissionType) {
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  if (NS_WARN_IF(!permMgr)) {
    return NS_ERROR_FAILURE;
  }

  return permMgr->RemoveFromPrincipal(aPrincipal, aPermissionType);
}

already_AddRefed<Promise> Permissions::Revoke(JSContext* aCx,
                                              JS::Handle<JSObject*> aPermission,
                                              ErrorResult& aRv) {
  if (!mWindow) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  PermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    aRv.NoteJSContextException(aCx);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(mWindow->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<Document> document = mWindow->GetExtantDoc();
  if (!document) {
    promise->MaybeReject(NS_ERROR_UNEXPECTED);
    return promise.forget();
  }

  nsCOMPtr<nsIPermissionManager> permMgr =
      components::PermissionManager::Service();
  if (NS_WARN_IF(!permMgr)) {
    promise->MaybeReject(NS_ERROR_FAILURE);
    return promise.forget();
  }

  const nsLiteralCString& permissionType =
      PermissionNameToType(permission.mName);

  nsresult rv;
  if (XRE_IsParentProcess()) {
    rv = RemovePermission(document->NodePrincipal(), permissionType);
  } else {
    // Permissions can't be removed from the content process. Send a message
    // to the parent; `ContentParent::RecvRemovePermission` will call
    // `RemovePermission`.
    ContentChild::GetSingleton()->SendRemovePermission(
        IPC::Principal(document->NodePrincipal()), permissionType, &rv);
  }

  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  RefPtr<PermissionStatus> status =
      CreatePermissionStatus(aCx, aPermission, mWindow, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_ASSERT(!status);
    return nullptr;
  }

  MOZ_ASSERT(status);
  promise->MaybeResolve(status);
  return promise.forget();
}

}  // namespace mozilla::dom
