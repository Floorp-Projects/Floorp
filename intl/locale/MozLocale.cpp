/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/MozLocale.h"

using namespace mozilla::intl;
using namespace mozilla::intl::ffi;

/**
 * Note: The file name is `MozLocale` to avoid compilation problems on
 * case-insensitive Windows. The class name is `Locale`.
 */
Locale::Locale(const nsACString& aLocale)
    : mRaw(unic_langid_new(&aLocale, &mIsWellFormed)) {}

const nsCString Locale::AsString() const {
  nsCString tag;
  unic_langid_as_string(mRaw.get(), &tag);
  return tag;
}

const nsDependentCSubstring Locale::GetLanguage() const {
  uint32_t len;
  const uint8_t* chars = unic_langid_get_language(mRaw.get(), &len);
  return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
}

const nsDependentCSubstring Locale::GetScript() const {
  uint32_t len;
  const uint8_t* chars = unic_langid_get_script(mRaw.get(), &len);
  return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
}

const nsDependentCSubstring Locale::GetRegion() const {
  uint32_t len;
  const uint8_t* chars = unic_langid_get_region(mRaw.get(), &len);
  return nsDependentCSubstring(reinterpret_cast<const char*>(chars), len);
}

void Locale::GetVariants(nsTArray<nsCString>& aRetVal) const {
  unic_langid_get_variants(mRaw.get(), &aRetVal);
}

bool Locale::Matches(const Locale& aOther, bool aThisRange,
                     bool aOtherRange) const {
  if (!IsWellFormed() || !aOther.IsWellFormed()) {
    return false;
  }

  return unic_langid_matches(mRaw.get(), aOther.Raw(), aThisRange, aOtherRange);
}

bool Locale::Maximize() { return unic_langid_maximize(mRaw.get()); }

void Locale::ClearVariants() { unic_langid_clear_variants(mRaw.get()); }

void Locale::ClearRegion() { unic_langid_clear_region(mRaw.get()); }
