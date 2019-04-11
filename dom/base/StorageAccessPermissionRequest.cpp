/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageAccessPermissionRequest.h"
#include "nsGlobalWindowInner.h"
#include "mozilla/StaticPrefs.h"
#include <cstdlib>

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
    : ContentPermissionRequestBase(aNodePrincipal, aWindow,
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
    mTimer = nullptr;
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
      unsigned simulatedDelay = CalculateSimulatedDelay();
      if (simulatedDelay) {
        MOZ_ASSERT(!mTimer);
        RefPtr<StorageAccessPermissionRequest> self = this;
        rv = NS_NewTimerWithFuncCallback(
            getter_AddRefs(mTimer), CallAutoGrantCallback, this, simulatedDelay,
            nsITimer::TYPE_ONE_SHOT, "DelayedAllowAutoGrantCallback");
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        NS_ADDREF(this);
      } else {
        mAllowAutoGrantCallback();
      }
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

unsigned StorageAccessPermissionRequest::CalculateSimulatedDelay() {
  if (!StaticPrefs::dom_storage_access_auto_grants_delayed()) {
    return 0;
  }

  // Generate a random time value that is at least 5 seconds and at most 15
  // minutes.
  std::srand(static_cast<unsigned>(PR_Now()));

  const unsigned kMin = 5000;
  const unsigned kMax = 6000;
  const unsigned random = std::abs(std::rand());

  return kMin + random % (kMax - kMin);
}

void StorageAccessPermissionRequest::CallAutoGrantCallback(nsITimer* aTimer,
                                                           void* aClosure) {
  auto self = static_cast<StorageAccessPermissionRequest*>(aClosure);
  self->mAllowAutoGrantCallback();
  self->mTimer = nullptr;
  NS_RELEASE(self);
}

}  // namespace dom
}  // namespace mozilla
