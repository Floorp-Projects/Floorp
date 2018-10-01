/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Feature.h"

#include "nsIURI.h"

using namespace mozilla::dom;

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
  mWhiteList.Clear();
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
  mWhiteList.Clear();
}

bool
Feature::AllowsAll() const
{
  return mPolicy == eAll;
}

void
Feature::AppendURIToWhiteList(nsIURI* aURI)
{
  mPolicy = eWhiteList;
  mWhiteList.AppendElement(aURI);
}

bool
Feature::WhiteListContains(nsIURI* aURI) const
{
  MOZ_ASSERT(aURI);

  if (!IsWhiteList()) {
    return false;
  }

  bool equal = false;
  for (nsIURI* uri : mWhiteList) {
    if (NS_SUCCEEDED(uri->Equals(aURI, &equal)) && equal) {
      return true;
    }
  }

  return false;
}

bool
Feature::IsWhiteList() const
{
  return mPolicy == eWhiteList;
}
