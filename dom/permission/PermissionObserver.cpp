/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PermissionObserver.h"

#include "mozilla/dom/PermissionStatus.h"
#include "mozilla/dom/WindowGlobalChild.h"
#include "mozilla/Services.h"
#include "mozilla/UniquePtr.h"
#include "nsIObserverService.h"
#include "nsIPermission.h"
#include "PermissionUtils.h"

namespace mozilla::dom {

namespace {
PermissionObserver* gInstance = nullptr;
}  // namespace

NS_IMPL_ISUPPORTS(PermissionObserver, nsIObserver, nsISupportsWeakReference)

PermissionObserver::PermissionObserver() { MOZ_ASSERT(!gInstance); }

PermissionObserver::~PermissionObserver() {
  MOZ_ASSERT(mSinks.IsEmpty());
  MOZ_ASSERT(gInstance == this);

  gInstance = nullptr;
}

/* static */
already_AddRefed<PermissionObserver> PermissionObserver::GetInstance() {
  RefPtr<PermissionObserver> instance = gInstance;
  if (!instance) {
    instance = new PermissionObserver();

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (NS_WARN_IF(!obs)) {
      return nullptr;
    }

    nsresult rv = obs->AddObserver(instance, "perm-changed", true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    rv = obs->AddObserver(instance, "perm-changed-notify-only", true);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    gInstance = instance;
  }

  return instance.forget();
}

void PermissionObserver::AddSink(PermissionStatus* aSink) {
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(!mSinks.Contains(aSink));

  mSinks.AppendElement(aSink);
}

void PermissionObserver::RemoveSink(PermissionStatus* aSink) {
  MOZ_ASSERT(aSink);
  MOZ_ASSERT(mSinks.Contains(aSink));

  mSinks.RemoveElement(aSink);
}

NS_IMETHODIMP
PermissionObserver::Observe(nsISupports* aSubject, const char* aTopic,
                            const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "perm-changed") ||
             !strcmp(aTopic, "perm-changed-notify-only"));

  if (mSinks.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIPermission> perm = nullptr;
  nsCOMPtr<nsPIDOMWindowInner> innerWindow = nullptr;
  nsAutoCString type;

  if (!strcmp(aTopic, "perm-changed")) {
    perm = do_QueryInterface(aSubject);
    if (!perm) {
      return NS_OK;
    }
    perm->GetType(type);
  } else if (!strcmp(aTopic, "perm-changed-notify-only")) {
    innerWindow = do_QueryInterface(aSubject);
    if (!innerWindow) {
      return NS_OK;
    }
    type = NS_ConvertUTF16toUTF8(aData);
  }

  Maybe<PermissionName> permission = TypeToPermissionName(type);
  if (permission) {
    for (auto* sink : mSinks) {
      if (sink->mName != permission.value()) {
        continue;
      }
      // Check for permissions that are changed for this sink's principal
      // via the "perm-changed" notification. These permissions affect
      // the window the sink (PermissionStatus) is held in directly.
      if (perm && sink->MaybeUpdatedBy(perm)) {
        sink->PermissionChanged();
      }
      // Check for permissions that are changed for this sink's principal
      // via the "perm-changed-notify-only" notification. These permissions
      // affect the window the sink (PermissionStatus) is held in indirectly- if
      // the window is same-party with the secondary key of a permission. For
      // example, a "3rdPartyFrameStorage^https://example.com" permission would
      // return true on these checks where sink is in a window that is same-site
      // with https://example.com.
      if (innerWindow && sink->MaybeUpdatedByNotifyOnly(innerWindow)) {
        sink->PermissionChanged();
      }
    }
  }

  return NS_OK;
}

}  // namespace mozilla::dom
