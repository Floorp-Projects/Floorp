/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPermission.h"
#include "nsContentUtils.h"
#include "nsIClassInfoImpl.h"
#include "nsIEffectiveTLDService.h"

// nsPermission Implementation

NS_IMPL_CLASSINFO(nsPermission, nullptr, 0, {0})
NS_IMPL_ISUPPORTS_CI(nsPermission, nsIPermission)

nsPermission::nsPermission(nsIPrincipal*    aPrincipal,
                           const nsACString &aType,
                           uint32_t         aCapability,
                           uint32_t         aExpireType,
                           int64_t          aExpireTime)
 : mPrincipal(aPrincipal)
 , mType(aType)
 , mCapability(aCapability)
 , mExpireType(aExpireType)
 , mExpireTime(aExpireTime)
{
}

NS_IMETHODIMP
nsPermission::GetPrincipal(nsIPrincipal** aPrincipal)
{
  nsCOMPtr<nsIPrincipal> copy = mPrincipal;
  copy.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetType(nsACString &aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetCapability(uint32_t *aCapability)
{
  *aCapability = mCapability;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetExpireType(uint32_t *aExpireType)
{
  *aExpireType = mExpireType;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::GetExpireTime(int64_t *aExpireTime)
{
  *aExpireTime = mExpireTime;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::Matches(nsIPrincipal* aPrincipal, bool aExactHost, bool* aMatches)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  NS_ENSURE_ARG_POINTER(aMatches);

  *aMatches = false;

  // If the principals are equal, then they match.
  if (mPrincipal->Equals(aPrincipal)) {
    *aMatches = true;
    return NS_OK;
  }

  // Make sure that the OriginAttributes of the two entries are the same
  nsAutoCString theirSuffix;
  nsresult rv = aPrincipal->GetOriginSuffix(theirSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString ourSuffix;
  rv = mPrincipal->GetOriginSuffix(ourSuffix);
  NS_ENSURE_SUCCESS(rv, rv);

  if (theirSuffix != ourSuffix) {
    return NS_OK;
  }

  // Right now, we only care about the hosts
  nsCOMPtr<nsIURI> theirURI;
  rv = aPrincipal->GetURI(getter_AddRefs(theirURI));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> ourURI;
  rv = mPrincipal->GetURI(getter_AddRefs(ourURI));
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the hosts so we can compare them
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

  if (aExactHost) { // If we only care about the exact host, we compare them and are done
    *aMatches = theirHost == ourHost;
    return NS_OK;
  }

  nsCOMPtr<nsIEffectiveTLDService> tldService =
    do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    NS_ERROR("Should have a tld service!");
    return NS_ERROR_FAILURE;
  }

  // Check if the host or any subdomain of the host matches. This loop will
  // not loop forever, as GetNextSubDomain will eventually fail with
  // NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS.
  while (theirHost != ourHost) {
    rv = tldService->GetNextSubDomain(theirHost, theirHost);
    if (NS_FAILED(rv)) {
      if (rv == NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS) {
        return NS_OK;
      } else {
        return rv;
      }
    }
  }

  *aMatches = true;
  return NS_OK;
}

NS_IMETHODIMP
nsPermission::MatchesURI(nsIURI* aURI, bool aExactHost, bool* aMatches)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  NS_ENSURE_TRUE(secMan, NS_ERROR_FAILURE);

  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv = secMan->GetNoAppCodebasePrincipal(aURI, getter_AddRefs(principal));
  NS_ENSURE_SUCCESS(rv, rv);

  return Matches(principal, aExactHost, aMatches);
}
