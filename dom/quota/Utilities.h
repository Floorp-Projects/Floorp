/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_utilities_h__
#define mozilla_dom_quota_utilities_h__

#include "mozilla/dom/quota/QuotaCommon.h"

BEGIN_QUOTA_NAMESPACE

inline void
IncrementUsage(uint64_t* aUsage, uint64_t aDelta)
{
  // Watch for overflow!
  if ((UINT64_MAX - *aUsage) < aDelta) {
    NS_WARNING("Usage exceeds the maximum!");
    *aUsage = UINT64_MAX;
  }
  else {
    *aUsage += aDelta;
  }
}

inline bool
PatternMatchesOrigin(const nsACString& aPatternString, const nsACString& aOrigin)
{
  // Aren't we smart!
  return StringBeginsWith(aOrigin, aPatternString);
}

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_utilities_h__
