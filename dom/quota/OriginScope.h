/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_originorpatternstring_h__
#define mozilla_dom_quota_originorpatternstring_h__

#include "mozilla/dom/quota/QuotaCommon.h"

BEGIN_QUOTA_NAMESPACE

class OriginScope : public nsCString
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
    return OriginScope(aOrigin, eOrigin);
  }

  static OriginScope
  FromPattern(const nsACString& aPattern)
  {
    return OriginScope(aPattern, ePattern);
  }

  static OriginScope
  FromNull()
  {
    return OriginScope(NullCString(), eNull);
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
    Assign(aOrigin);
    mType = eOrigin;
  }

  void
  SetFromPattern(const nsACString& aPattern)
  {
    Assign(aPattern);
    mType = ePattern;
  }

  void
  SetFromNull()
  {
    SetIsVoid(true);
    mType = eNull;
  }

private:
  OriginScope(const nsACString& aString, Type aType)
  : nsCString(aString), mType(aType)
  { }

  bool
  operator==(const OriginScope& aOther) = delete;

  Type mType;
};

END_QUOTA_NAMESPACE

#endif // mozilla_dom_quota_originorpatternstring_h__
