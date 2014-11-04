/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_origincollection_h__
#define mozilla_dom_quota_origincollection_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "nsHashKeys.h"
#include "nsTHashtable.h"

#include "Utilities.h"

BEGIN_QUOTA_NAMESPACE

class OriginCollection
{
public:
  bool
  ContainsPattern(const nsACString& aPattern)
  {
    return mPatterns.Contains(aPattern);
  }

  void
  AddPattern(const nsACString& aPattern)
  {
    MOZ_ASSERT(!mOrigins.Count());
    if (!ContainsPattern(aPattern)) {
      mPatterns.AppendElement(aPattern);
    }
  }

  bool
  ContainsOrigin(const nsACString& aOrigin)
  {
    for (uint32_t index = 0; index < mPatterns.Length(); index++) {
      if (PatternMatchesOrigin(mPatterns[index], aOrigin)) {
        return true;
      }
    }

    return mOrigins.GetEntry(aOrigin);
  }

  void
  AddOrigin(const nsACString& aOrigin)
  {
    if (!ContainsOrigin(aOrigin)) {
      mOrigins.PutEntry(aOrigin);
    }
  }

private:
  nsTArray<nsCString> mPatterns;
  nsTHashtable<nsCStringHashKey> mOrigins;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_origincollection_h__
