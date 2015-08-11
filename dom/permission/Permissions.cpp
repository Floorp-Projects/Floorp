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

PermissionState
ActionToPermissionState(uint32_t aAction)
{
  switch (aAction) {
    case nsIPermissionManager::ALLOW_ACTION:
      return PermissionState::Granted;

    case nsIPermissionManager::DENY_ACTION:
      return PermissionState::Denied;

    default:
    case nsIPermissionManager::PROMPT_ACTION:
      return PermissionState::Prompt;
  }
}

nsresult
CheckPermission(const char* aName,
                nsPIDOMWindow* aWindow,
                PermissionState& aResult)
{
  MOZ_ASSERT(aName);
  MOZ_ASSERT(aWindow);

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  if (NS_WARN_IF(!permMgr)) {
    return NS_ERROR_FAILURE;
  }

  uint32_t action = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permMgr->TestPermissionFromWindow(aWindow, aName, &action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  aResult = ActionToPermissionState(action);
  return NS_OK;
}

nsresult
CheckPushPermission(JSContext* aCx,
                    JS::Handle<JSObject*> aPermission,
                    nsPIDOMWindow* aWindow,
                    PermissionState& aResult)
{
  PushPermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    return NS_ERROR_UNEXPECTED;
  }

  if (permission.mUserVisible) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return CheckPermission("push", aWindow, aResult);
}

nsresult
CheckPermission(JSContext* aCx,
                JS::Handle<JSObject*> aPermission,
                nsPIDOMWindow* aWindow,
                PermissionState& aResult)
{
  PermissionDescriptor permission;
  JS::Rooted<JS::Value> value(aCx, JS::ObjectOrNullValue(aPermission));
  if (NS_WARN_IF(!permission.Init(aCx, value))) {
    return NS_ERROR_UNEXPECTED;
  }

  switch (permission.mName) {
    case PermissionName::Geolocation:
      return CheckPermission("geo", aWindow, aResult);

    case PermissionName::Notifications:
      return CheckPermission("desktop-notification", aWindow, aResult);

    case PermissionName::Push:
      return CheckPushPermission(aCx, aPermission, aWindow, aResult);

    case PermissionName::Midi:
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
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

  nsRefPtr<Promise> promise = Promise::Create(global, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  PermissionState state = PermissionState::Denied;
  nsresult rv = CheckPermission(aCx, aPermission, mWindow, state);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
  } else {
    promise->MaybeResolve(new PermissionStatus(mWindow, state));
  }
  return promise.forget();
}

} // namespace dom
} // namespace mozilla
