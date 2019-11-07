/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowInner.h"
#include "PermissionDelegateHandler.h"
#include "nsPIDOMWindow.h"
#include "nsPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsContentPermissionHelper.h"

#include "mozilla/StaticPrefs_permissions.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FeaturePolicyUtils.h"

using namespace mozilla;
using namespace mozilla::dom;
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
};

NS_IMPL_CYCLE_COLLECTION(PermissionDelegateHandler)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PermissionDelegateHandler)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PermissionDelegateHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PermissionDelegateHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

PermissionDelegateHandler::PermissionDelegateHandler(dom::Document* aDocument)
    : mDocument(aDocument) {
  MOZ_ASSERT(aDocument);
}

const DelegateInfo* PermissionDelegateHandler::GetPermissionDelegateInfo(
    const nsAString& aPermissionName) const {
  nsAutoString lowerContent(aPermissionName);
  ToLowerCase(lowerContent);

  for (const auto& perm : sPermissionsMap) {
    if (lowerContent.EqualsASCII(perm.mPermissionName)) {
      return &perm;
    }
  }

  return nullptr;
}

bool PermissionDelegateHandler::Initialize() {
  MOZ_ASSERT(mDocument);

  mPermissionManager = nsPermissionManager::GetInstance();
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

bool PermissionDelegateHandler::HasPermissionDelegated(
    const nsACString& aType) {
  MOZ_ASSERT(mDocument);

  if (!StaticPrefs::permissions_delegation_enable()) {
    return true;
  }

  // System principal should have right to make permission request
  if (mPrincipal->IsSystemPrincipal()) {
    return true;
  }

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));

  // If the type is not in the supported list, auto denied
  if (!info) {
    return false;
  }

  if (info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy &&
      info->mFeatureName) {
    nsAutoString featureName(info->mFeatureName);
    // Default allowlist for a feature used in permissions delegate should be
    // set to eSelf, to ensure that permission is denied by default and only
    // have the opportunity to request permission with allow attribute.
    if (!FeaturePolicyUtils::IsFeatureAllowed(mDocument, featureName)) {
      return false;
    }
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

  nsresult (NS_STDCALL nsIPermissionManager::*testPermission)(
      nsIPrincipal*, const nsACString&, uint32_t*) =
      aExactHostMatch ? &nsIPermissionManager::TestExactPermissionFromPrincipal
                      : &nsIPermissionManager::TestPermissionFromPrincipal;

  if (!StaticPrefs::permissions_delegation_enable()) {
    return (mPermissionManager->*testPermission)(mPrincipal, aType,
                                                 aPermission);
  }

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));

  // If the type is not in the supported list, auto denied
  if (!info) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return NS_OK;
  }

  if (info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy &&
      info->mFeatureName) {
    nsAutoString featureName(info->mFeatureName);
    // Default allowlist for a feature used in permissions delegate should be
    // set to eSelf, to ensure that permission is denied by default and only
    // have the opportunity to request permission with allow attribute.
    if (!FeaturePolicyUtils::IsFeatureAllowed(mDocument, featureName)) {
      *aPermission = nsIPermissionManager::DENY_ACTION;
      return NS_OK;
    }
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
