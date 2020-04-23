/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Permission.h"
#include "nsIClassInfoImpl.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_permissions.h"

namespace mozilla {

// Permission Implementation

NS_IMPL_CLASSINFO(Permission, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(Permission, nsIPermission)

Permission::Permission(nsIPrincipal* aPrincipal, const nsACString& aType,
                       uint32_t aCapability, uint32_t aExpireType,
                       int64_t aExpireTime, int64_t aModificationTime)
    : mPrincipal(aPrincipal),
      mType(aType),
      mCapability(aCapability),
      mExpireType(aExpireType),
      mExpireTime(aExpireTime),
      mModificationTime(aModificationTime) {}

already_AddRefed<nsIPrincipal> Permission::ClonePrincipalForPermission(
    nsIPrincipal* aPrincipal) {
  MOZ_ASSERT(aPrincipal);

  mozilla::OriginAttributes attrs = aPrincipal->OriginAttributesRef();
  if (!StaticPrefs::permissions_isolateBy_userContext()) {
    attrs.StripAttributes(mozilla::OriginAttributes::STRIP_USER_CONTEXT_ID);
  }

  nsAutoCString originNoSuffix;
  nsresult rv = aPrincipal->GetOriginNoSuffix(originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), originNoSuffix);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return mozilla::BasePrincipal::CreateContentPrincipal(uri, attrs);
}

already_AddRefed<Permission> Permission::Create(
    nsIPrincipal* aPrincipal, const nsACString& aType, uint32_t aCapability,
    uint32_t aExpireType, int64_t aExpireTime, int64_t aModificationTime) {
  NS_ENSURE_TRUE(aPrincipal, nullptr);

  nsCOMPtr<nsIPrincipal> principal =
      Permission::ClonePrincipalForPermission(aPrincipal);
  NS_ENSURE_TRUE(principal, nullptr);

  RefPtr<Permission> permission =
      new Permission(principal, aType, aCapability, aExpireType, aExpireTime,
                     aModificationTime);
  return permission.forget();
}

NS_IMETHODIMP
Permission::GetPrincipal(nsIPrincipal** aPrincipal) {
  nsCOMPtr<nsIPrincipal> copy = mPrincipal;
  copy.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
Permission::GetType(nsACString& aType) {
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
Permission::GetCapability(uint32_t* aCapability) {
  *aCapability = mCapability;
  return NS_OK;
}

NS_IMETHODIMP
Permission::GetExpireType(uint32_t* aExpireType) {
  *aExpireType = mExpireType;
  return NS_OK;
}

NS_IMETHODIMP
Permission::GetExpireTime(int64_t* aExpireTime) {
  *aExpireTime = mExpireTime;
  return NS_OK;
}

NS_IMETHODIMP
Permission::GetModificationTime(int64_t* aModificationTime) {
  *aModificationTime = mModificationTime;
  return NS_OK;
}

NS_IMETHODIMP
Permission::Matches(nsIPrincipal* aPrincipal, bool aExactHost, bool* aMatches) {
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aMatches);

  *aMatches = false;

  nsCOMPtr<nsIPrincipal> principal =
      Permission::ClonePrincipalForPermission(aPrincipal);
  if (!principal) {
    *aMatches = false;
    return NS_OK;
  }

  return MatchesPrincipalForPermission(principal, aExactHost, aMatches);
}

NS_IMETHODIMP
Permission::MatchesPrincipalForPermission(nsIPrincipal* aPrincipal,
                                          bool aExactHost, bool* aMatches) {
  return mPrincipal->EqualsForPermission(aPrincipal, aExactHost, aMatches);
}

NS_IMETHODIMP
Permission::MatchesURI(nsIURI* aURI, bool aExactHost, bool* aMatches) {
  NS_ENSURE_ARG_POINTER(aURI);

  mozilla::OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> principal =
      mozilla::BasePrincipal::CreateContentPrincipal(aURI, attrs);
  NS_ENSURE_TRUE(principal, NS_ERROR_FAILURE);

  return Matches(principal, aExactHost, aMatches);
}

}  // namespace mozilla
