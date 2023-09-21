/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/WebAuthnUtil.h"
#include "nsComponentManagerUtils.h"
#include "nsICryptoHash.h"
#include "nsIEffectiveTLDService.h"
#include "nsNetUtil.h"
#include "mozpkix/pkixutil.h"
#include "nsHTMLDocument.h"
#include "hasht.h"

namespace mozilla::dom {

// Bug #1436078 - Permit Google Accounts. Remove in Bug #1436085 in Jan 2023.
constexpr auto kGoogleAccountsAppId1 =
    u"https://www.gstatic.com/securitykey/origins.json"_ns;
constexpr auto kGoogleAccountsAppId2 =
    u"https://www.gstatic.com/securitykey/a/google.com/origins.json"_ns;

bool EvaluateAppID(nsPIDOMWindowInner* aParent, const nsString& aOrigin,
                   /* in/out */ nsString& aAppId) {
  // Facet is the specification's way of referring to the web origin.
  nsAutoCString facetString = NS_ConvertUTF16toUTF8(aOrigin);
  nsCOMPtr<nsIURI> facetUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(facetUri), facetString))) {
    return false;
  }

  // If the facetId (origin) is not HTTPS, reject
  if (!facetUri->SchemeIs("https")) {
    return false;
  }

  // If the appId is empty or null, overwrite it with the facetId and accept
  if (aAppId.IsEmpty() || aAppId.EqualsLiteral("null")) {
    aAppId.Assign(aOrigin);
    return true;
  }

  // AppID is user-supplied. It's quite possible for this parse to fail.
  nsAutoCString appIdString = NS_ConvertUTF16toUTF8(aAppId);
  nsCOMPtr<nsIURI> appIdUri;
  if (NS_FAILED(NS_NewURI(getter_AddRefs(appIdUri), appIdString))) {
    return false;
  }

  // if the appId URL is not HTTPS, reject.
  if (!appIdUri->SchemeIs("https")) {
    return false;
  }

  nsAutoCString appIdHost;
  if (NS_FAILED(appIdUri->GetAsciiHost(appIdHost))) {
    return false;
  }

  // Allow localhost.
  if (appIdHost.EqualsLiteral("localhost")) {
    nsAutoCString facetHost;
    if (NS_FAILED(facetUri->GetAsciiHost(facetHost))) {
      return false;
    }

    if (facetHost.EqualsLiteral("localhost")) {
      return true;
    }
  }

  // Run the HTML5 algorithm to relax the same-origin policy, copied from W3C
  // Web Authentication. See Bug 1244959 comment #8 for context on why we are
  // doing this instead of implementing the external-fetch FacetID logic.
  nsCOMPtr<Document> document = aParent->GetDoc();
  if (!document || !document->IsHTMLDocument()) {
    return false;
  }

  nsHTMLDocument* html = document->AsHTMLDocument();
  // Use the base domain as the facet for evaluation. This lets this algorithm
  // relax the whole eTLD+1.
  nsCOMPtr<nsIEffectiveTLDService> tldService =
      do_GetService(NS_EFFECTIVETLDSERVICE_CONTRACTID);
  if (!tldService) {
    return false;
  }

  nsAutoCString lowestFacetHost;
  if (NS_FAILED(tldService->GetBaseDomain(facetUri, 0, lowestFacetHost))) {
    return false;
  }

  if (html->IsRegistrableDomainSuffixOfOrEqualTo(
          NS_ConvertUTF8toUTF16(lowestFacetHost), appIdHost)) {
    return true;
  }

  // Bug #1436078 - Permit Google Accounts. Remove in Bug #1436085 in Jan 2023.
  if (lowestFacetHost.EqualsLiteral("google.com") &&
      (aAppId.Equals(kGoogleAccountsAppId1) ||
       aAppId.Equals(kGoogleAccountsAppId2))) {
    return true;
  }

  return false;
}

static nsresult HashCString(nsICryptoHash* aHashService, const nsACString& aIn,
                            /* out */ nsTArray<uint8_t>& aOut) {
  MOZ_ASSERT(aHashService);

  nsresult rv = aHashService->Init(nsICryptoHash::SHA256);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = aHashService->Update(
      reinterpret_cast<const uint8_t*>(aIn.BeginReading()), aIn.Length());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoCString fullHash;
  // Passing false below means we will get a binary result rather than a
  // base64-encoded string.
  rv = aHashService->Finish(false, fullHash);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aOut.Clear();
  aOut.AppendElements(reinterpret_cast<uint8_t const*>(fullHash.BeginReading()),
                      fullHash.Length());

  return NS_OK;
}

nsresult HashCString(const nsACString& aIn, /* out */ nsTArray<uint8_t>& aOut) {
  nsresult srv;
  nsCOMPtr<nsICryptoHash> hashService =
      do_CreateInstance(NS_CRYPTO_HASH_CONTRACTID, &srv);
  if (NS_FAILED(srv)) {
    return srv;
  }

  srv = HashCString(hashService, aIn, aOut);
  if (NS_WARN_IF(NS_FAILED(srv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla::dom
