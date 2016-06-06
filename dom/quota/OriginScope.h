/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_originorpatternstring_h__
#define mozilla_dom_quota_originorpatternstring_h__

#include "mozilla/dom/quota/QuotaCommon.h"

#include "mozilla/BasePrincipal.h"

BEGIN_QUOTA_NAMESPACE

class OriginScope
{
public:
  enum Type
  {
    eOrigin,
    ePattern,
    eNull
  };

  static OriginScope
  FromOrigin(const nsACString& aOrigin)
  {
    return OriginScope(aOrigin);
  }

  static OriginScope
  FromPattern(const mozilla::OriginAttributesPattern& aPattern)
  {
    return OriginScope(aPattern);
  }

  static OriginScope
  FromJSONPattern(const nsAString& aJSONPattern)
  {
    return OriginScope(aJSONPattern);
  }

  static OriginScope
  FromNull()
  {
    return OriginScope();
  }

  bool
  IsOrigin() const
  {
    return mType == eOrigin;
  }

  bool
  IsPattern() const
  {
    return mType == ePattern;
  }

  bool
  IsNull() const
  {
    return mType == eNull;
  }

  Type
  GetType() const
  {
    return mType;
  }

  void
  SetFromOrigin(const nsACString& aOrigin)
  {
    mOrigin = aOrigin;
    mType = eOrigin;

    nsCString originNoSuffix;
    MOZ_ALWAYS_TRUE(mOriginAttributes.PopulateFromOrigin(aOrigin,
                                                         originNoSuffix));
  }

  void
  SetFromPattern(const mozilla::OriginAttributesPattern& aPattern)
  {
    mPattern = aPattern;
    mType = ePattern;
  }

  void
  SetFromJSONPattern(const nsAString& aJSONPattern)
  {
    MOZ_ALWAYS_TRUE(mPattern.Init(aJSONPattern));
    mType = ePattern;
  }

  void
  SetFromNull()
  {
    mType = eNull;
  }

  const nsACString&
  GetOrigin() const
  {
    MOZ_ASSERT(IsOrigin());
    return mOrigin;
  }

  void
  SetOrigin(const nsACString& aOrigin)
  {
    MOZ_ASSERT(IsOrigin());
    mOrigin = aOrigin;
  }

  const mozilla::OriginAttributes&
  GetOriginAttributes() const
  {
    MOZ_ASSERT(IsOrigin());
    return mOriginAttributes;
  }

  const mozilla::OriginAttributesPattern&
  GetPattern() const
  {
    MOZ_ASSERT(IsPattern());
    return mPattern;
  }

  bool MatchesOrigin(const OriginScope& aOther) const
  {
    MOZ_ASSERT(aOther.IsOrigin());

    bool match;

    if (IsOrigin()) {
      match = mOrigin.Equals(aOther.mOrigin);
    } else if (IsPattern()) {
      match = mPattern.Matches(aOther.mOriginAttributes);
    } else {
      match = true;
    }

    return match;
  }

  bool MatchesPattern(const OriginScope& aOther) const
  {
    MOZ_ASSERT(aOther.IsPattern());

    bool match;

    if (IsOrigin()) {
      match = aOther.mPattern.Matches(mOriginAttributes);
    } else if (IsPattern()) {
      match = mPattern.Overlaps(aOther.mPattern);
    } else {
      match = true;
    }

    return match;
  }

  bool Matches(const OriginScope& aOther) const
  {
    bool match;

    if (aOther.IsOrigin()) {
      match = MatchesOrigin(aOther);
    } else if (aOther.IsPattern()) {
      match = MatchesPattern(aOther);
    } else {
      match = true;
    }

    return match;
  }

  OriginScope
  Clone()
  {
    if (IsOrigin()) {
      return OriginScope(mOrigin);
    }

    if (IsPattern()) {
      return OriginScope(mPattern);
    }

    MOZ_ASSERT(IsNull());
    return OriginScope();
  }

private:
  explicit OriginScope(const nsACString& aOrigin)
    : mOrigin(aOrigin)
    , mType(eOrigin)
  {
    nsCString originNoSuffix;
    MOZ_ALWAYS_TRUE(mOriginAttributes.PopulateFromOrigin(aOrigin,
                                                         originNoSuffix));
  }

  explicit OriginScope(const mozilla::OriginAttributesPattern& aPattern)
    : mPattern(aPattern)
    , mType(ePattern)
  { }

  explicit OriginScope(const nsAString& aJSONPattern)
    : mType(ePattern)
  {
    MOZ_ALWAYS_TRUE(mPattern.Init(aJSONPattern));
  }

  OriginScope()
    : mType(eNull)
  { }

  bool
  operator==(const OriginScope& aOther) = delete;

  nsCString mOrigin;
  PrincipalOriginAttributes mOriginAttributes;
  mozilla::OriginAttributesPattern mPattern;
  Type mType;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_originorpatternstring_h__
