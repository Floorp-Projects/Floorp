/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageOriginAttributes.h"

#include "nsString.h"
#include "nsURLHelper.h"
#include "mozilla/Assertions.h"
#include "mozilla/dom/quota/QuotaManager.h"

namespace mozilla {

void StorageOriginAttributes::CreateSuffix(nsACString& aStr) const {
  nsCString str1;

  URLParams params;
  nsAutoCString value;

  if (mInIsolatedMozBrowser) {
    params.Set("inBrowser"_ns, "1"_ns);
  }

  str1.Truncate();
  params.Serialize(value, true);
  if (!value.IsEmpty()) {
    str1.AppendLiteral("^");
    str1.Append(value);
  }

  // Make sure that the string don't contain characters that would get replaced
  // with the plus character by quota manager, potentially causing ambiguity.
  MOZ_ASSERT(str1.FindCharInSet(dom::quota::QuotaManager::kReplaceChars) ==
             kNotFound);

  // Let OriginAttributes::CreateSuffix serialize other origin attributes.
  nsCString str2;
  mOriginAttributes.CreateSuffix(str2);

  aStr.Truncate();

  if (str1.IsEmpty()) {
    aStr.Append(str2);
    return;
  }

  if (str2.IsEmpty()) {
    aStr.Append(str1);
    return;
  }

  // If both strings are not empty, we need to combine them.
  aStr.Append(str1);
  aStr.Append('&');
  aStr.Append(Substring(str2, 1, str2.Length() - 1));
}

bool StorageOriginAttributes::PopulateFromSuffix(const nsACString& aStr) {
  if (aStr.IsEmpty()) {
    return true;
  }

  if (aStr[0] != '^') {
    return false;
  }

  bool ok = URLParams::Parse(
      Substring(aStr, 1, aStr.Length() - 1), true,
      [this](const nsACString& aName, const nsACString& aValue) {
        if (aName.EqualsLiteral("inBrowser")) {
          if (!aValue.EqualsLiteral("1")) {
            return false;
          }

          mInIsolatedMozBrowser = true;
          return true;
        }

        // Let OriginAttributes::PopulateFromSuffix parse other
        // origin attributes.
        return true;
      });
  if (!ok) {
    return false;
  }

  return mOriginAttributes.PopulateFromSuffix(aStr);
}

bool StorageOriginAttributes::PopulateFromOrigin(const nsACString& aOrigin,
                                                 nsACString& aOriginNoSuffix) {
  // RFindChar is only available on nsCString.
  nsCString origin(aOrigin);
  int32_t pos = origin.RFindChar('^');

  if (pos == kNotFound) {
    aOriginNoSuffix = origin;
    return true;
  }

  aOriginNoSuffix = Substring(origin, 0, pos);
  return PopulateFromSuffix(Substring(origin, pos));
}

}  // namespace mozilla
