/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_intl_Locale_h__
#define mozilla_intl_Locale_h__

#include "nsString.h"

namespace mozilla {
namespace intl {

/**
 * Locale object, a BCP47-style tag decomposed into subtags for
 * matching purposes.
 *
 * If constructed with aRange = true, any missing subtags will be
 * set to "*".
 *
 * Note: The file name is `MozLocale` to avoid compilation problems on case-insensitive
 * Windows. The class name is `Locale`.
 */
class Locale {
  public:
    Locale(const nsCString& aLocale, bool aRange);

    bool Matches(const Locale& aLocale) const;
    bool LanguageMatches(const Locale& aLocale) const;


    void SetVariantRange();
    void SetRegionRange();

    // returns false if nothing changed
    bool AddLikelySubtags();
    bool AddLikelySubtagsWithoutRegion();

    const nsCString& AsString() const {
      return mLocaleStr;
    }

    bool operator== (const Locale& aOther) {
      const auto& cmp = nsCaseInsensitiveCStringComparator();
      return mLanguage.Equals(aOther.mLanguage, cmp) &&
             mScript.Equals(aOther.mScript, cmp) &&
             mRegion.Equals(aOther.mRegion, cmp) &&
             mVariant.Equals(aOther.mVariant, cmp);
    }

  private:
    const nsCString& mLocaleStr;
    nsCString mLanguage;
    nsCString mScript;
    nsCString mRegion;
    nsCString mVariant;

    bool AddLikelySubtagsForLocale(const nsACString& aLocale);
};

} // intl
} // namespace mozilla

#endif /* mozilla_intl_Locale_h__ */
