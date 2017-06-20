/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageUtils.h"

namespace mozilla {
namespace dom {
namespace StorageUtils {

nsresult
GenerateOriginKey(nsIPrincipal* aPrincipal, nsACString& aOriginAttrSuffix,
                  nsACString& aOriginKey)
{
  if (NS_WARN_IF(!aPrincipal)) {
    return NS_ERROR_UNEXPECTED;
  }

  aPrincipal->OriginAttributesRef().CreateSuffix(aOriginAttrSuffix);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = aPrincipal->GetURI(getter_AddRefs(uri));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!uri) {
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoCString domainOrigin;
  rv = uri->GetAsciiHost(domainOrigin);
  NS_ENSURE_SUCCESS(rv, rv);

  if (domainOrigin.IsEmpty()) {
    // For the file:/// protocol use the exact directory as domain.
    bool isScheme = false;
    if (NS_SUCCEEDED(uri->SchemeIs("file", &isScheme)) && isScheme) {
      nsCOMPtr<nsIURL> url = do_QueryInterface(uri, &rv);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = url->GetDirectory(domainOrigin);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Append reversed domain
  nsAutoCString reverseDomain;
  rv = CreateReversedDomain(domainOrigin, reverseDomain);
  if (NS_FAILED(rv)) {
    return rv;
  }

  aOriginKey.Append(reverseDomain);

  // Append scheme
  nsAutoCString scheme;
  rv = uri->GetScheme(scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  aOriginKey.Append(':');
  aOriginKey.Append(scheme);

  // Append port if any
  int32_t port = NS_GetRealPort(uri);
  if (port != -1) {
    aOriginKey.Append(nsPrintfCString(":%d", port));
  }

  return NS_OK;
}

bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal,
                nsIPrincipal* aSubjectPrincipal)
{
  if (!aSubjectPrincipal) {
    return true;
  }

  if (!aObjectPrincipal) {
    return false;
  }

  return aSubjectPrincipal->Equals(aObjectPrincipal);
}

void
ReverseString(const nsACString& aSource, nsACString& aResult)
{
  nsACString::const_iterator sourceBegin, sourceEnd;
  aSource.BeginReading(sourceBegin);
  aSource.EndReading(sourceEnd);

  aResult.SetLength(aSource.Length());
  nsACString::iterator destEnd;
  aResult.EndWriting(destEnd);

  while (sourceBegin != sourceEnd) {
    *(--destEnd) = *sourceBegin;
    ++sourceBegin;
  }
}

nsresult
CreateReversedDomain(const nsACString& aAsciiDomain,
                     nsACString& aKey)
{
  if (aAsciiDomain.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ReverseString(aAsciiDomain, aKey);

  aKey.Append('.');
  return NS_OK;
}

} // StorageUtils namespace
} // dom namespace
} // mozilla namespace
