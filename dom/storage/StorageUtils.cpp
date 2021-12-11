/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIURL.h"
#include "StorageUtils.h"

#include "mozilla/OriginAttributes.h"
#include "nsDebug.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace dom {
namespace StorageUtils {

bool PrincipalsEqual(nsIPrincipal* aObjectPrincipal,
                     nsIPrincipal* aSubjectPrincipal) {
  if (!aSubjectPrincipal) {
    return true;
  }

  if (!aObjectPrincipal) {
    return false;
  }

  return aSubjectPrincipal->Equals(aObjectPrincipal);
}

void ReverseString(const nsACString& aSource, nsACString& aResult) {
  nsACString::const_iterator sourceBegin, sourceEnd;
  aSource.BeginReading(sourceBegin);
  aSource.EndReading(sourceEnd);

  aResult.SetLength(aSource.Length());
  auto destEnd = aResult.EndWriting();

  while (sourceBegin != sourceEnd) {
    *(--destEnd) = *sourceBegin;
    ++sourceBegin;
  }
}

nsresult CreateReversedDomain(const nsACString& aAsciiDomain,
                              nsACString& aKey) {
  if (aAsciiDomain.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  ReverseString(aAsciiDomain, aKey);

  aKey.Append('.');
  return NS_OK;
}

// This is only a compatibility code for schema version 0.  Returns the 'scope'
// key in the schema version 0 format for the scope column.
nsCString Scheme0Scope(const nsACString& aOriginSuffix,
                       const nsACString& aOriginNoSuffix) {
  nsCString result;

  OriginAttributes oa;
  if (!aOriginSuffix.IsEmpty()) {
    DebugOnly<bool> success = oa.PopulateFromSuffix(aOriginSuffix);
    MOZ_ASSERT(success);
  }

  if (oa.mInIsolatedMozBrowser) {
    result.AppendInt(0);  // This is the appId to be removed.
    result.Append(':');
    result.Append(oa.mInIsolatedMozBrowser ? 't' : 'f');
    result.Append(':');
  }

  // If there is more than just appid and/or inbrowser stored in origin
  // attributes, put it to the schema 0 scope as well.  We must do that
  // to keep the scope column unique (same resolution as schema 1 has
  // with originAttributes and originKey columns) so that switch between
  // schema 1 and 0 always works in both ways.
  nsAutoCString remaining;
  oa.mInIsolatedMozBrowser = false;
  oa.CreateSuffix(remaining);
  if (!remaining.IsEmpty()) {
    MOZ_ASSERT(!aOriginSuffix.IsEmpty());

    if (result.IsEmpty()) {
      // Must contain the old prefix, otherwise we won't search for the whole
      // origin attributes suffix.
      result.AppendLiteral("0:f:");
    }

    // Append the whole origin attributes suffix despite we have already stored
    // appid and inbrowser.  We are only looking for it when the scope string
    // starts with "$appid:$inbrowser:" (with whatever valid values).
    //
    // The OriginAttributes suffix is a string in a form like:
    // "^addonId=101&userContextId=5" and it's ensured it always starts with '^'
    // and never contains ':'.  See OriginAttributes::CreateSuffix.
    result.Append(aOriginSuffix);
    result.Append(':');
  }

  result.Append(aOriginNoSuffix);

  return result;
}

}  // namespace StorageUtils
}  // namespace dom
}  // namespace mozilla
