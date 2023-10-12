/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/StorageAccessPermissionStatus.h"

#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/dom/PermissionStatusBinding.h"
#include "nsIPermissionManager.h"

namespace mozilla::dom {

// static
RefPtr<PermissionStatus::CreatePromise> StorageAccessPermissionStatus::Create(
    nsPIDOMWindowInner* aWindow) {
  RefPtr<PermissionStatus> status = new StorageAccessPermissionStatus(aWindow);
  return status->Init()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [status](nsresult aOk) {
        MOZ_ASSERT(NS_SUCCEEDED(aOk));
        return MozPromise<RefPtr<PermissionStatus>, nsresult,
                          true>::CreateAndResolve(status, __func__);
      },
      [](nsresult aError) {
        MOZ_ASSERT(NS_FAILED(aError));
        return MozPromise<RefPtr<PermissionStatus>, nsresult,
                          true>::CreateAndReject(aError, __func__);
      });
}

StorageAccessPermissionStatus::StorageAccessPermissionStatus(
    nsPIDOMWindowInner* aWindow)
    : PermissionStatus(aWindow, PermissionName::Storage_access) {}

RefPtr<PermissionStatus::SimplePromise>
StorageAccessPermissionStatus::UpdateState() {
  nsCOMPtr<nsPIDOMWindowInner> window = GetOwner();
  if (NS_WARN_IF(!window)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  WindowGlobalChild* wgc = window->GetWindowGlobalChild();
  if (NS_WARN_IF(!wgc)) {
    return SimplePromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  // Perform a Permission Policy Request
  if (!FeaturePolicyUtils::IsFeatureAllowed(window->GetExtantDoc(),
                                            u"storage-access"_ns)) {
    mState = PermissionState::Prompt;
    return SimplePromise::CreateAndResolve(NS_OK, __func__);
  }

  RefPtr<StorageAccessPermissionStatus> self(this);
  return wgc->SendGetStorageAccessPermission()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self](uint32_t aAction) {
        if (aAction == nsIPermissionManager::ALLOW_ACTION) {
          self->mState = PermissionState::Granted;
        } else {
          // We never reveal PermissionState::Denied here
          self->mState = PermissionState::Prompt;
        }
        return SimplePromise::CreateAndResolve(NS_OK, __func__);
      },
      [](mozilla::ipc::ResponseRejectReason aError) {
        return SimplePromise::CreateAndResolve(NS_ERROR_FAILURE, __func__);
      });
}

bool StorageAccessPermissionStatus::MaybeUpdatedBy(
    nsIPermission* aPermission) const {
  return false;
}

bool StorageAccessPermissionStatus::MaybeUpdatedByNotifyOnly(
    nsPIDOMWindowInner* aInnerWindow) const {
  nsPIDOMWindowInner* owner = GetOwner();
  NS_ENSURE_TRUE(owner, false);
  NS_ENSURE_TRUE(aInnerWindow, false);
  return owner->WindowID() == aInnerWindow->WindowID();
}

}  // namespace mozilla::dom
