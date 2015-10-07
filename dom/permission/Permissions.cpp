/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/Permissions.h"

#include "mozilla/dom/PermissionsBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/Services.h"
#include "nsIPermissionManager.h"
#include "PermissionUtils.h"

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Permissions)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(Permissions)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Permissions)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Permissions, mWindow)

Permissions::Permissions(nsPIDOMWindow* aWindow)
  : mWindow(aWindow)
{
}

Permissions::~Permissions()
{
}

JSObject*
Permissions::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PermissionsBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

already_AddRefed<PermissionStatus>
CreatePushPermissionStatus(JSContext* aCx,
                           JS::Handle<JSObject*> aPermission,
                           nsPIDOMWindow* aWindow,
                           ErrorResult& aRv)
{
  PushPermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  if (permission.mUserVisible) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return PermissionStatus::Create(aWindow, permission.mName, aRv);
}

already_AddRefed<PermissionStatus>
CreatePermissionStatus(JSContext* aCx,
                       JS::Handle<JSObject*> aPermission,
                       nsPIDOMWindow* aWindow,
                       ErrorResult& aRv)
{
  PermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  switch (permission.mName) {
    case PermissionName::Geolocation:
    case PermissionName::Notifications:
      return PermissionStatus::Create(aWindow, permission.mName, aRv);

    case PermissionName::Push:
      return CreatePushPermissionStatus(aCx, aPermission, aWindow, aRv);

    case PermissionName::Midi:
    default:
      aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
      return nullptr;
  }
}

} // namespace

already_AddRefed<Promise>
Permissions::Query(JSContext* aCx,
                   JS::Handle<JSObject*> aPermission,
                   ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(mWindow);
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  RefPtr<PermissionStatus> status =
    CreatePermissionStatus(aCx, aPermission, mWindow, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    MOZ_ASSERT(!status);
    promise->MaybeReject(aRv);
  } else {
    MOZ_ASSERT(status);
    promise->MaybeResolve(status);
  }
  return promise.forget();
}

} // namespace dom
} // namespace mozilla
