/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/PermissionDelegateHandler.h"

#include "nsPIDOMWindow.h"
#include "nsIPrincipal.h"
#include "nsContentPermissionHelper.h"

#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_permissions.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/FeaturePolicyUtils.h"
#include "mozilla/dom/WindowContext.h"
#include "mozilla/PermissionManager.h"

using namespace mozilla::dom;

namespace mozilla {

typedef PermissionDelegateHandler::PermissionDelegatePolicy DelegatePolicy;
typedef PermissionDelegateHandler::PermissionDelegateInfo DelegateInfo;

// Particular type of permissions to care about. We decide cases by case and
// give various types of controls over each of these.
static const DelegateInfo sPermissionsMap[] = {
    // Permissions API map. All permission names have to be in lowercase.
    {"geo", u"geolocation", DelegatePolicy::eDelegateUseFeaturePolicy},
    // The same with geo, but we support both to save some conversions between
    // "geo" and "geolocation"
    {"geolocation", u"geolocation", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"desktop-notification", nullptr,
     DelegatePolicy::ePersistDeniedCrossOrigin},
    {"persistent-storage", nullptr, DelegatePolicy::ePersistDeniedCrossOrigin},
    {"vibration", nullptr, DelegatePolicy::ePersistDeniedCrossOrigin},
    {"midi", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
    // Like "midi" but with sysex support.
    {"midi-sysex", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
    {"storage-access", nullptr, DelegatePolicy::eDelegateUseIframeOrigin},
    {"camera", u"camera", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"microphone", u"microphone", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"screen", u"display-capture", DelegatePolicy::eDelegateUseFeaturePolicy},
    {"xr", u"xr-spatial-tracking", DelegatePolicy::eDelegateUseFeaturePolicy},
};

static_assert(PermissionDelegateHandler::DELEGATED_PERMISSION_COUNT ==
                  (sizeof(sPermissionsMap) / sizeof(DelegateInfo)),
              "The PermissionDelegateHandler::DELEGATED_PERMISSION_COUNT must "
              "match to the "
              "length of sPermissionsMap. Please update it.");

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

/* static */
nsresult PermissionDelegateHandler::GetDelegatePrincipal(
    const nsACString& aType, nsIContentPermissionRequest* aRequest,
    nsIPrincipal** aResult) {
  MOZ_ASSERT(aRequest);

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));
  if (!info) {
    *aResult = nullptr;
    return NS_OK;
  }

  if (info->mPolicy == DelegatePolicy::eDelegateUseTopOrigin ||
      info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy) {
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
  return true;
}

static bool IsCrossOriginContentToTop(Document* aDocument) {
  MOZ_ASSERT(aDocument);

  RefPtr<BrowsingContext> bc = aDocument->GetBrowsingContext();
  if (!bc) {
    return true;
  }
  RefPtr<BrowsingContext> topBC = bc->Top();

  // In Fission, we can know if it is cross-origin by checking whether both
  // contexts in the same process. So, If they are not in the same process, we
  // can say that it's cross-origin.
  if (!topBC->IsInProcess()) {
    return true;
  }

  RefPtr<Document> topDoc = topBC->GetDocument();
  if (!topDoc) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> topLevelPrincipal = topDoc->NodePrincipal();

  return !aDocument->NodePrincipal()->Subsumes(topLevelPrincipal);
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
    const nsACString& aType) const {
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

  if (info->mPolicy == DelegatePolicy::ePersistDeniedCrossOrigin &&
      !mDocument->IsTopLevelContentDocument() &&
      IsCrossOriginContentToTop(mDocument)) {
    return false;
  }

  return true;
}

nsresult PermissionDelegateHandler::GetPermission(const nsACString& aType,
                                                  uint32_t* aPermission,
                                                  bool aExactHostMatch) {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mPrincipal);

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

  if (info->mPolicy == DelegatePolicy::ePersistDeniedCrossOrigin &&
      !mDocument->IsTopLevelContentDocument() &&
      IsCrossOriginContentToTop(mDocument)) {
    *aPermission = nsIPermissionManager::DENY_ACTION;
    return NS_OK;
  }

  nsIPrincipal* principal = mPrincipal;
  // If we cannot get the browsing context from the document, we fallback to use
  // the prinicpal of the document to test the permission.
  RefPtr<BrowsingContext> bc = mDocument->GetBrowsingContext();

  if ((info->mPolicy == DelegatePolicy::eDelegateUseTopOrigin ||
       info->mPolicy == DelegatePolicy::eDelegateUseFeaturePolicy) &&
      bc) {
    RefPtr<WindowContext> topWC = bc->GetTopWindowContext();

    if (topWC && topWC->IsInProcess()) {
      // If the top-level window context is in the same process, we directly get
      // the node principal from the top-level document to test the permission.
      // We cannot check the lists in the window context in this case since the
      // 'perm-changed' could be notified in the iframe before the top-level in
      // certain cases, for example, request permissions in first-party iframes.
      // In this case, the list in window context hasn't gotten updated, so it
      // would has an out-dated value until the top-level window get the
      // observer. So, we have to test permission manager directly if we can.
      RefPtr<Document> topDoc = topWC->GetBrowsingContext()->GetDocument();

      if (topDoc) {
        principal = topDoc->NodePrincipal();
      }
    } else if (topWC) {
      // Get the delegated permissions from the top-level window context.
      DelegatedPermissionList list =
          aExactHostMatch ? topWC->GetDelegatedExactHostMatchPermissions()
                          : topWC->GetDelegatedPermissions();
      size_t idx = std::distance(sPermissionsMap, info);
      *aPermission = list.mPermissions[idx];
      return NS_OK;
    }
  }

  return (mPermissionManager->*testPermission)(principal, aType, aPermission);
}

nsresult PermissionDelegateHandler::GetPermissionForPermissionsAPI(
    const nsACString& aType, uint32_t* aPermission) {
  return GetPermission(aType, aPermission, false);
}

void PermissionDelegateHandler::PopulateAllDelegatedPermissions() {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mPermissionManager);

  // We only populate the delegated permissions for the top-level content.
  if (!mDocument->IsTopLevelContentDocument()) {
    return;
  }

  RefPtr<WindowContext> wc = mDocument->GetWindowContext();
  NS_ENSURE_TRUE_VOID(wc && !wc->IsDiscarded());

  DelegatedPermissionList list;
  DelegatedPermissionList exactHostMatchList;

  for (const auto& perm : sPermissionsMap) {
    size_t idx = std::distance(sPermissionsMap, &perm);

    nsDependentCString type(perm.mPermissionName);
    // Populate the permission.
    uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
    Unused << mPermissionManager->TestPermissionFromPrincipal(mPrincipal, type,
                                                              &permission);
    list.mPermissions[idx] = permission;

    // Populate the exact-host-match permission.
    permission = nsIPermissionManager::UNKNOWN_ACTION;
    Unused << mPermissionManager->TestExactPermissionFromPrincipal(
        mPrincipal, type, &permission);
    exactHostMatchList.mPermissions[idx] = permission;
  }

  WindowContext::Transaction txn;
  txn.SetDelegatedPermissions(list);
  txn.SetDelegatedExactHostMatchPermissions(exactHostMatchList);
  MOZ_ALWAYS_SUCCEEDS(txn.Commit(wc));
}

void PermissionDelegateHandler::UpdateDelegatedPermission(
    const nsACString& aType) {
  MOZ_ASSERT(mDocument);
  MOZ_ASSERT(mPermissionManager);

  // We only update the delegated permission for the top-level content.
  if (!mDocument->IsTopLevelContentDocument()) {
    return;
  }

  RefPtr<WindowContext> wc = mDocument->GetWindowContext();
  NS_ENSURE_TRUE_VOID(wc);

  const DelegateInfo* info =
      GetPermissionDelegateInfo(NS_ConvertUTF8toUTF16(aType));
  if (!info) {
    return;
  }
  size_t idx = std::distance(sPermissionsMap, info);

  WindowContext::Transaction txn;
  bool changed = false;
  DelegatedPermissionList list = wc->GetDelegatedPermissions();

  if (UpdateDelegatePermissionInternal(
          list, aType, idx,
          &nsIPermissionManager::TestPermissionFromPrincipal)) {
    txn.SetDelegatedPermissions(list);
    changed = true;
  }

  DelegatedPermissionList exactHostMatchList =
      wc->GetDelegatedExactHostMatchPermissions();

  if (UpdateDelegatePermissionInternal(
          exactHostMatchList, aType, idx,
          &nsIPermissionManager::TestExactPermissionFromPrincipal)) {
    txn.SetDelegatedExactHostMatchPermissions(exactHostMatchList);
    changed = true;
  }

  // We only commit if there is any change of permissions.
  if (changed) {
    MOZ_ALWAYS_SUCCEEDS(txn.Commit(wc));
  }
}

bool PermissionDelegateHandler::UpdateDelegatePermissionInternal(
    PermissionDelegateHandler::DelegatedPermissionList& aList,
    const nsACString& aType, size_t aIdx,
    nsresult (NS_STDCALL nsIPermissionManager::*aTestFunc)(nsIPrincipal*,
                                                           const nsACString&,
                                                           uint32_t*)) {
  MOZ_ASSERT(aTestFunc);
  MOZ_ASSERT(mPermissionManager);
  MOZ_ASSERT(mPrincipal);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  Unused << (mPermissionManager->*aTestFunc)(mPrincipal, aType, &permission);

  if (aList.mPermissions[aIdx] != permission) {
    aList.mPermissions[aIdx] = permission;
    return true;
  }

  return false;
}

}  // namespace mozilla
