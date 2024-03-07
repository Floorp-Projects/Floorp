/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageOriginAttributes.h"

#include "nsURLHelper.h"

namespace mozilla {

bool StorageOriginAttributes::PopulateFromSuffix(const nsACString& aStr) {
  if (aStr.IsEmpty()) {
    return true;
  }

  if (aStr[0] != '^') {
    return false;
  }

  bool ok =
      URLParams::Parse(Substring(aStr, 1, aStr.Length() - 1),
                       [this](const nsAString& aName, const nsAString& aValue) {
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

void StorageOriginAttributes::CreateSuffix(nsACString& aStr) const {
  URLParams params;
  nsAutoString value;

  if (mInIsolatedMozBrowser) {
    params.Set(u"inBrowser"_ns, u"1"_ns);
  }

  params.Serialize(value, true);
  if (!value.IsEmpty()) {
    aStr.AppendLiteral("^");
    aStr.Append(NS_ConvertUTF16toUTF8(value));
  }
}

}  // namespace mozilla
