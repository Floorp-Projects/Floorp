/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/intl/MozLocale.h"

#include "unicode/uloc.h"

using namespace mozilla::intl;

/**
 * Note: The file name is `MozLocale` to avoid compilation problems on case-insensitive
 * Windows. The class name is `Locale`.
 */
Locale::Locale(const nsCString& aLocale, bool aRange)
  : mLocaleStr(aLocale)
{
  int32_t partNum = 0;

  nsAutoCString normLocale(aLocale);
  normLocale.ReplaceChar('_', '-');

  for (const nsACString& part : normLocale.Split('-')) {
    switch (partNum) {
      case 0:
        if (part.EqualsLiteral("*") ||
            part.Length() == 2 || part.Length() == 3) {
          mLanguage.Assign(part);
        }
        break;
      case 1:
        if (part.EqualsLiteral("*") || part.Length() == 4) {
          mScript.Assign(part);
          break;
        }

        // fallover to region case
        partNum++;
        MOZ_FALLTHROUGH;
      case 2:
        if (part.EqualsLiteral("*") || part.Length() == 2) {
          mRegion.Assign(part);
        }
        break;
      case 3:
        if (part.EqualsLiteral("*") || (part.Length() >= 3 && part.Length() <= 8)) {
          mVariant.Assign(part);
        }
        break;
    }
    partNum++;
  }

  if (aRange) {
    if (mLanguage.IsEmpty()) {
      mLanguage.AssignLiteral("*");
    }
    if (mScript.IsEmpty()) {
      mScript.AssignLiteral("*");
    }
    if (mRegion.IsEmpty()) {
      mRegion.AssignLiteral("*");
    }
    if (mVariant.IsEmpty()) {
      mVariant.AssignLiteral("*");
    }
  }
}

static bool
SubtagMatches(const nsCString& aSubtag1, const nsCString& aSubtag2)
{
  return aSubtag1.EqualsLiteral("*") ||
         aSubtag2.EqualsLiteral("*") ||
         aSubtag1.Equals(aSubtag2, nsCaseInsensitiveCStringComparator());
}

bool
Locale::Matches(const Locale& aLocale) const
{
  return SubtagMatches(mLanguage, aLocale.mLanguage) &&
         SubtagMatches(mScript, aLocale.mScript) &&
         SubtagMatches(mRegion, aLocale.mRegion) &&
         SubtagMatches(mVariant, aLocale.mVariant);
}

bool
Locale::LanguageMatches(const Locale& aLocale) const
{
  return SubtagMatches(mLanguage, aLocale.mLanguage) &&
         SubtagMatches(mScript, aLocale.mScript);
}

void
Locale::SetVariantRange()
{
  mVariant.AssignLiteral("*");
}

void
Locale::SetRegionRange()
{
  mRegion.AssignLiteral("*");
}

bool
Locale::AddLikelySubtags()
{
  return AddLikelySubtagsForLocale(mLocaleStr);
}

bool
Locale::AddLikelySubtagsWithoutRegion()
{
  nsAutoCString locale(mLanguage);

  if (!mScript.IsEmpty()) {
    locale.Append("-");
    locale.Append(mScript);
  }

  // We don't add variant here because likelySubtag doesn't care about it.

  return AddLikelySubtagsForLocale(locale);
}

bool
Locale::AddLikelySubtagsForLocale(const nsACString& aLocale)
{
  const int32_t kLocaleMax = 160;
  char maxLocale[kLocaleMax];
  nsAutoCString locale(aLocale);

  UErrorCode status = U_ZERO_ERROR;
  uloc_addLikelySubtags(locale.get(), maxLocale, kLocaleMax, &status);

  if (U_FAILURE(status)) {
    return false;
  }

  nsDependentCString maxLocStr(maxLocale);
  Locale loc = Locale(maxLocStr, false);

  if (loc == *this) {
    return false;
  }

  mLanguage = loc.mLanguage;
  mScript = loc.mScript;
  mRegion = loc.mRegion;

  // We don't update variant from likelySubtag since it's not going to
  // provide it and we want to preserve the range

  return true;
}
