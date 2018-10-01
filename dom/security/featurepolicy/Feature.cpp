/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Feature.h"

using namespace mozilla::dom;

void
Feature::GetWhiteListedOrigins(nsTArray<nsString>& aList) const
{
  MOZ_ASSERT(mPolicy == eWhiteList);
  aList.AppendElements(mWhiteListedOrigins);
}

bool
Feature::Allows(const nsAString& aOrigin) const
{
  if (mPolicy == eNone) {
    return false;
  }

  if (mPolicy == eAll) {
    return true;
  }

  for (const nsString& whiteListedOrigin : mWhiteListedOrigins) {
    if (whiteListedOrigin.Equals(aOrigin)) {
      return true;
    }
  }

  return false;
}

Feature::Feature(const nsAString& aFeatureName)
  : mFeatureName(aFeatureName)
  , mPolicy(eWhiteList)
{}

Feature::~Feature() = default;

const nsAString&
Feature::Name() const
{
  return mFeatureName;
}

void
Feature::SetAllowsNone()
{
  mPolicy = eNone;
  mWhiteListedOrigins.Clear();
}

bool
Feature::AllowsNone() const
{
  return mPolicy == eNone;
}

void
Feature::SetAllowsAll()
{
  mPolicy = eAll;
  mWhiteListedOrigins.Clear();
}

bool
Feature::AllowsAll() const
{
  return mPolicy == eAll;
}

void
Feature::AppendOriginToWhiteList(const nsAString& aOrigin)
{
  mPolicy = eWhiteList;
  mWhiteListedOrigins.AppendElement(aOrigin);
}

bool
Feature::WhiteListContains(const nsAString& aOrigin) const
{
  if (!IsWhiteList()) {
    return false;
  }

  return mWhiteListedOrigins.Contains(aOrigin);
}

bool
Feature::IsWhiteList() const
{
  return mPolicy == eWhiteList;
}
