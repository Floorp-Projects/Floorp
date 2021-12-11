/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/MozLocale.h"

using namespace mozilla::intl;
using namespace mozilla::intl::ffi;

MozLocale::MozLocale(const nsACString& aLocale)
    : mRaw(unic_langid_new(&aLocale, &mIsWellFormed)) {}

const nsCString MozLocale::AsString() const {
  nsCString tag;
  unic_langid_as_string(mRaw.get(), &tag);
  return tag;
}

const nsDependentCSubstring MozLocale::GetLanguage() const {
  nsDependentCSubstring sub;
  unic_langid_get_language(mRaw.get(), &sub);
  return sub;
}

const nsDependentCSubstring MozLocale::GetScript() const {
  nsDependentCSubstring sub;
  unic_langid_get_script(mRaw.get(), &sub);
  return sub;
}

const nsDependentCSubstring MozLocale::GetRegion() const {
  nsDependentCSubstring sub;
  unic_langid_get_region(mRaw.get(), &sub);
  return sub;
}

void MozLocale::GetVariants(nsTArray<nsCString>& aRetVal) const {
  unic_langid_get_variants(mRaw.get(), &aRetVal);
}

bool MozLocale::Matches(const MozLocale& aOther, bool aThisRange,
                        bool aOtherRange) const {
  if (!IsWellFormed() || !aOther.IsWellFormed()) {
    return false;
  }

  return unic_langid_matches(mRaw.get(), aOther.Raw(), aThisRange, aOtherRange);
}

bool MozLocale::Maximize() { return unic_langid_maximize(mRaw.get()); }

void MozLocale::ClearVariants() { unic_langid_clear_variants(mRaw.get()); }

void MozLocale::ClearRegion() { unic_langid_clear_region(mRaw.get()); }
