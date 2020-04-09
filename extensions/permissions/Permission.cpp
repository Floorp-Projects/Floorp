/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Permission.h"
#include "nsContentUtils.h"
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
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aMatches);

  *aMatches = false;

  // If the principals are equal, then they match.
  if (mPrincipal->Equals(aPrincipal)) {
    *aMatches = true;
    return NS_OK;
  }

  // If we are matching with an exact host, we're done now - the permissions
  // don't match otherwise, we need to start comparing subdomains!
  if (aExactHost) {
    return NS_OK;
  }

  // Compare their OriginAttributes
  const mozilla::OriginAttributes& theirAttrs =
      aPrincipal->OriginAttributesRef();
  const mozilla::OriginAttributes& ourAttrs = mPrincipal->OriginAttributesRef();

  if (theirAttrs != ourAttrs) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> theirURI;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(theirURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> ourURI;
  rv = mPrincipal->GetURI(getter_AddRefs(ourURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Compare schemes
  nsAutoCString theirScheme;
  rv = theirURI->GetScheme(theirScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ourScheme;
  rv = ourURI->GetScheme(ourScheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (theirScheme != ourScheme) {
    return NS_OK;
  }

  // Compare ports
  int32_t theirPort;
  rv = theirURI->GetPort(&theirPort);
  NS_ENSURE_SUCCESS(rv, rv);

  int32_t ourPort;
  rv = ourURI->GetPort(&ourPort);
  NS_ENSURE_SUCCESS(rv, rv);

  if (theirPort != ourPort) {
    return NS_OK;
  }

  // Check if the host or any subdomain of their host matches.
  nsAutoCString theirHost;
  rv = theirURI->GetHost(theirHost);
  if (NS_FAILED(rv) || theirHost.IsEmpty()) {
    return NS_OK;
  }

  nsAutoCString ourHost;
  rv = ourURI->GetHost(ourHost);
  if (NS_FAILED(rv) || ourHost.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    NS_ERROR("Should have a tld service!");
    return NS_ERROR_FAILURE;
  }

  // This loop will not loop forever, as GetNextSubDomain will eventually fail
  // with NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS.
  while (theirHost != ourHost) {
    rv = tldService->GetNextSubDomain(theirHost, theirHost);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        return NS_OK;
      }
      return rv;
    }
  }

  *aMatches = true;
  return NS_OK;
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
