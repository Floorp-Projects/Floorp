/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_originorpatternstring_h__
#define mozilla_dom_quota_originorpatternstring_h__

#include "mozilla/dom/quota/QuotaCommon.h"

BEGIN_QUOTA_NAMESPACE

class OriginOrPatternString : public nsCString
{
public:
  static OriginOrPatternString
  FromOrigin(const nsACString& aOrigin)
  {
    return OriginOrPatternString(aOrigin, true);
  }

  static OriginOrPatternString
  FromPattern(const nsACString& aPattern)
  {
    return OriginOrPatternString(aPattern, false);
  }

  bool
  IsOrigin() const
  {
    return mIsOrigin;
  }

  bool
  IsPattern() const
  {
    return !mIsOrigin;
  }

private:
  OriginOrPatternString(const nsACString& aOriginOrPattern, bool aIsOrigin)
  : nsCString(aOriginOrPattern), mIsOrigin(aIsOrigin)
  { }

  bool
  operator==(const OriginOrPatternString& aOther) MOZ_DELETE;

  bool mIsOrigin;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_originorpatternstring_h__
