/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlobalWindowInner.h"
#include "PermissionDelegateHandler.h"
#include "nsPIDOMWindow.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"

#include "mozilla/Preferences.h"
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
    {"desktop-notification", nullptr,
     DelegatePolicy::ePersistDeniedCrossOrigin},
    {"persistent-storage", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
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

nsresult PermissionDelegateHandler::GetPermissionForPermissionsAPI(
    const nsACString& aType, uint32_t* aPermission) {
  MOZ_ASSERT(mDocument);

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));

  // If the type is not in the supported list, auto denied
  if (!info) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return NS_OK;
  }

  nsresult rv;
  nsCOMPtr<nsIPermissionManager> permMgr =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return rv;
  }

  nsCOMPtr<nsIPrincipal> principal = mDocument->NodePrincipal();
  if (!Preferences::GetBool("permissions.delegation.enable", false)) {
    return permMgr->TestPermissionFromPrincipal(principal, aType, aPermission);
  }

  if (info->mPolicy == DelegatePolicy::eDelegateUseIframeOrigin) {
    return permMgr->TestPermissionFromPrincipal(principal, aType, aPermission);
  }

  nsPIDOMWindowInner* window = mDocument->GetInnerWindow();
  nsGlobalWindowInner* innerWindow = nsGlobalWindowInner::Cast(window);
  nsIPrincipal* topPrincipal = innerWindow->GetTopLevelPrincipal();

  // Permission is delegated in same origin
  if (principal->Subsumes(topPrincipal)) {
    return permMgr->TestPermissionFromPrincipal(topPrincipal, aType,
                                                aPermission);
  }

  if (info->mPolicy == DelegatePolicy::ePersistDeniedCrossOrigin) {
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

  return permMgr->TestPermissionFromPrincipal(topPrincipal, aType, aPermission);
}
