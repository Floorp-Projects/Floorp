/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageAccessPermissionRequest.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(StorageAccessPermissionRequest,
                                   ContentPermissionRequestBase)

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(StorageAccessPermissionRequest,
                                               ContentPermissionRequestBase)

StorageAccessPermissionRequest::StorageAccessPermissionRequest(
    nsPIDOMWindowInner* aWindow, nsIPrincipal* aNodePrincipal,
    AllowCallback&& aAllowCallback,
    AllowAutoGrantCallback&& aAllowAutoGrantCallback,
    AllowAnySiteCallback&& aAllowAnySiteCallback,
    CancelCallback&& aCancelCallback)
    : ContentPermissionRequestBase(aNodePrincipal, false, aWindow,
                                   NS_LITERAL_CSTRING("dom.storage_access"),
                                   NS_LITERAL_CSTRING("storage-access")),
      mAllowCallback(std::move(aAllowCallback)),
      mAllowAutoGrantCallback(std::move(aAllowAutoGrantCallback)),
      mAllowAnySiteCallback(std::move(aAllowAnySiteCallback)),
      mCancelCallback(std::move(aCancelCallback)),
      mCallbackCalled(false) {
  mPermissionRequests.AppendElement(
      PermissionRequest(mType, nsTArray<nsString>()));
}

StorageAccessPermissionRequest::~StorageAccessPermissionRequest() { Cancel(); }

NS_IMETHODIMP
StorageAccessPermissionRequest::Cancel() {
  if (!mCallbackCalled) {
    mCallbackCalled = true;
    mCancelCallback();
  }
  return NS_OK;
}

NS_IMETHODIMP
StorageAccessPermissionRequest::Allow(JS::HandleValue aChoices) {
  nsTArray<PermissionChoice> choices;
  nsresult rv = TranslateChoices(aChoices, mPermissionRequests, choices);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!mCallbackCalled) {
    mCallbackCalled = true;
    if (choices.Length() == 1 &&
        choices[0].choice().EqualsLiteral("allow-on-any-site")) {
      mAllowAnySiteCallback();
    } else if (choices.Length() == 1 &&
               choices[0].choice().EqualsLiteral("allow-auto-grant")) {
      mAllowAutoGrantCallback();
    } else {
      mAllowCallback();
    }
  }
  return NS_OK;
}

already_AddRefed<StorageAccessPermissionRequest>
StorageAccessPermissionRequest::Create(
    nsPIDOMWindowInner* aWindow, AllowCallback&& aAllowCallback,
    AllowAutoGrantCallback&& aAllowAutoGrantCallback,
    AllowAnySiteCallback&& aAllowAnySiteCallback,
    CancelCallback&& aCancelCallback) {
  if (!aWindow) {
    return nullptr;
  }
  nsGlobalWindowInner* win = nsGlobalWindowInner::Cast(aWindow);
  if (!win->GetPrincipal()) {
    return nullptr;
  }
  RefPtr<StorageAccessPermissionRequest> request =
      new StorageAccessPermissionRequest(
          aWindow, win->GetPrincipal(), std::move(aAllowCallback),
          std::move(aAllowAutoGrantCallback), std::move(aAllowAnySiteCallback),
          std::move(aCancelCallback));
  return request.forget();
}

}  // namespace dom
}  // namespace mozilla
