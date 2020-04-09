/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PermissionDelegateHandler.h"

#include "nsGlobalWindowInner.h"
#include "nsPIDOMWindow.h"
#include "nsIPrincipal.h"
#include "nsContentPermissionHelper.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/PermissionManager.h"

using namespace mozilla::dom;

namespace mozilla {

typedef PermissionDelegateHandler::PermissionDelegatePolicy DelegatePolicy;
typedef PermissionDelegateHandler::PermissionDelegateInfo DelegateInfo;

// Particular type of permissions to care about. We decide cases by case and
// give various types of controls over each of these.
static const DelegateInfo sPermissionsMap[] = {
    // Permissions API map
    {"geo", u"geolocation", DelegatePolicy::eDelegateUseFeaturePolicy},
    // The same with geo, but we support both to save some conversions between
    // "geo" and "geolocation"
    {"geolocation", u"geolocation", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"desktop-notification", nullptr,
     DelegatePolicy::ePersistDeniedCrossOrigin},
    {"persistent-storage", nullptr, DelegatePolicy::ePersistDeniedCrossOrigin},
    {"vibration", nullptr, DelegatePolicy::ePersistDeniedCrossOrigin},
    {"midi", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
    {"storage-access", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
    {"camera", u"camera", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"microphone", u"microphone", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"screen", u"display-capture", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"xr", u"xr-spatial-tracking", DelegatePolicy::eDelegateUseFeaturePolicy},
};

NS_IMPL_CYCLE_COLLECTION(PermissionDelegateHandler)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PermissionDelegateHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PermissionDelegateHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PermissionDelegateHandler)
  NS_INTERFACE_MAP_ENTRY(nsIPermissionDelegateHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PermissionDelegateHandler::PermissionDelegateHandler(dom::Document* aDocument)
    : mDocument(aDocument) {
  MOZ_ASSERT(aDocument);
}

/* static */
const DelegateInfo* PermissionDelegateHandler::GetPermissionDelegateInfo(
    const nsAString& aPermissionName) {
  nsAutoString lowerContent(aPermissionName);
  ToLowerCase(lowerContent);

  for (const auto& perm : sPermissionsMap) {
    if (lowerContent.EqualsASCII(perm.mPermissionName)) {
      return &perm;
    }
  }

  return nullptr;
}

NS_IMETHODIMP
PermissionDelegateHandler::MaybeUnsafePermissionDelegate(
    const nsTArray<nsCString>& aTypes, bool* aMaybeUnsafe) {
  *aMaybeUnsafe = false;
  if (!StaticPrefs::permissions_delegation_enabled()) {
    return NS_OK;
  }

  for (auto& type : aTypes) {
    const DelegateInfo* info =
        GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(type));
    if (!info) {
      continue;
    }

    nsAutoString featureName(info->mFeatureName);
    if (FeaturePolicyUtils::IsFeatureUnsafeAllowedAll(mDocument, featureName)) {
      *aMaybeUnsafe = true;
      return NS_OK;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
PermissionDelegateHandler::GetPermissionDelegateFPEnabled(bool* aEnabled) {
  MOZ_ASSERT(NS_IsMainThread());
  *aEnabled = StaticPrefs::permissions_delegation_enabled() &&
              StaticPrefs::dom_security_featurePolicy_enabled();
  return NS_OK;
}

/* static */
nsresult PermissionDelegateHandler::GetDelegatePrincipal(
    const nsACString& aType, nsIContentPermissionRequest* aRequest,
    nsIPrincipal** aResult) {
  MOZ_ASSERT(aRequest);

  if (!StaticPrefs::permissions_delegation_enabled()) {
    return aRequest->GetPrincipal(aResult);
  }

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));
  if (!info) {
    *aResult = nullptr;
    return NS_OK;
  }

  if (info->mPolicy == DelegatePolicy::eDelegateUseTopOrigin ||
      (info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy &&
       StaticPrefs::dom_security_featurePolicy_enabled())) {
    return aRequest->GetTopLevelPrincipal(aResult);
  }

  return aRequest->GetPrincipal(aResult);
}

bool PermissionDelegateHandler::Initialize() {
  MOZ_ASSERT(mDocument);

  mPermissionManager = PermissionManager::GetInstance();
  if (!mPermissionManager) {
    return false;
  }

  mPrincipal = mDocument->NodePrincipal();
  nsPIDOMWindowInner* window = mDocument->GetInnerWindow();
  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(window);
  if (innerWindow) {
    mTopLevelPrincipal = innerWindow->GetTopLevelAntiTrackingPrincipal();
  }

  return true;
}

static bool IsTopWindowContent(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  BrowsingContext* browsingContext = aDocument->GetBrowsingContext();
  return browsingContext && browsingContext->IsTopContent();
}

bool PermissionDelegateHandler::HasFeaturePolicyAllowed(
    const DelegateInfo* info) const {
  if (info->mPolicy != DelegatePolicy::eDelegateUseFeaturePolicy ||
      !info->mFeatureName) {
    return true;
  }

  nsAutoString featureName(info->mFeatureName);
  return FeaturePolicyUtils::IsFeatureAllowed(mDocument, featureName);
}

bool PermissionDelegateHandler::HasPermissionDelegated(
    const nsACString& aType) {
  MOZ_ASSERT(mDocument);

  // System principal should have right to make permission request
  if (mPrincipal->IsSystemPrincipal()) {
    return true;
  }

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));
  if (!info || !HasFeaturePolicyAllowed(info)) {
    return false;
  }

  if (!StaticPrefs::permissions_delegation_enabled()) {
    return true;
  }

  if (info->mPolicy == DelegatePolicy::ePersistDeniedCrossOrigin &&
      !IsTopWindowContent(mDocument) &&
      !mPrincipal->Subsumes(mTopLevelPrincipal)) {
    return false;
  }

  return true;
}

nsresult PermissionDelegateHandler::GetPermission(const nsACString& aType,
                                                  uint32_t* aPermission,
                                                  bool aExactHostMatch) {
  MOZ_ASSERT(mDocument);

  if (mPrincipal->IsSystemPrincipal()) {
    *aPermission = nsIPermissionManager::ALLOW_ACTION;
    return NS_OK;
  }

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));
  if (!info || !HasFeaturePolicyAllowed(info)) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return NS_OK;
  }

  nsresult (NS_STDCALL nsIPermissionManager::*testPermission)(
      nsIPrincipal*, const nsACString&, uint32_t*) =
      aExactHostMatch ? &nsIPermissionManager::TestExactPermissionFromPrincipal
                      : &nsIPermissionManager::TestPermissionFromPrincipal;

  if (!StaticPrefs::permissions_delegation_enabled()) {
    return (mPermissionManager->*testPermission)(mPrincipal, aType,
                                                 aPermission);
  }

  if (info->mPolicy == DelegatePolicy::ePersistDeniedCrossOrigin &&
      !IsTopWindowContent(mDocument) &&
      !mPrincipal->Subsumes(mTopLevelPrincipal)) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return NS_OK;
  }

  nsIPrincipal* principal = mPrincipal;
  if (mTopLevelPrincipal &&
      (info->mPolicy == DelegatePolicy::eDelegateUseTopOrigin ||
       (info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy &&
        StaticPrefs::dom_security_featurePolicy_enabled()))) {
    principal = mTopLevelPrincipal;
  }

  return (mPermissionManager->*testPermission)(principal, aType, aPermission);
}

nsresult PermissionDelegateHandler::GetPermissionForPermissionsAPI(
    const nsACString& aType, uint32_t* aPermission) {
  return GetPermission(aType, aPermission, false);
}

}  // namespace mozilla
