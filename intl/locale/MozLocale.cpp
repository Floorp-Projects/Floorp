/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/TextUtils.h"
#include "mozilla/intl/MozLocale.h"

#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"

#include "unicode/uloc.h"

using namespace mozilla::intl;
using mozilla::IsAscii;

/**
 * Note: The file name is `MozLocale` to avoid compilation problems on
 * case-insensitive Windows. The class name is `Locale`.
 */
Locale::Locale(const nsACString& aLocale) {
  if (aLocale.IsEmpty() || !IsAscii(aLocale)) {
    mIsWellFormed = false;
    return;
  }

  int32_t position = 0;

  nsAutoCString normLocale(aLocale);
  normLocale.ReplaceChar('_', '-');

  /**
   * BCP47 language tag:
   *
   * langtag = language            2*3ALPHA
   *           ["-" extlang]       3ALPHA *2("-" 3ALPHA)
   *           ["-" script]        4ALPHA
   *           ["-" region]        2ALPHA / 3DIGIT
   *           *("-" variant)      5*8alphanum / (DIGIT 3alphanum)
   *           *("-" extension)    [0-9a-wy-z] 1*("-" (1*8alphanum))
   *           ["-" privateuse]    x 1*("-" (1*8alphanum))
   *
   * This class currently supports a subset of the full BCP47 language tag
   * with a single extension of allowing variants to be 3ALPHA to support
   * `ja-JP-mac` code:
   *
   * langtag = language            2*3ALPHA
   *           ["-" script]        4ALPHA
   *           ["-" region]        2ALPHA
   *           *("-" variant)      3*8alphanum
   *           ["-"] privateuse]   "x" 1*("-" (1*8alphanum))
   *
   * The `position` variable represents the currently expected section of the
   * tag and intentionally skips positions (like `extlang`) which may be added
   * later.
   *
   *  language-extlangs-script-region-variant-extension-privateuse
   *  --- 0 -- --- 1 -- -- 2 - -- 3 - -- 4 -- --- x --- ---- 6 ---
   */
  for (const nsACString& subTag : normLocale.Split('-')) {
    auto slen = subTag.Length();
    if (slen > 8) {
      mIsWellFormed = false;
      return;
    } else if (position == 6) {
      ToLowerCase(*mPrivateUse.AppendElement(subTag));
    } else if (subTag.LowerCaseEqualsLiteral("x")) {
      position = 6;
    } else if (position == 0) {
      if (slen < 2 || slen > 3) {
        mIsWellFormed = false;
        return;
      }
      mLanguage = subTag;
      ToLowerCase(mLanguage);
      position = 2;
    } else if (position <= 2 && slen == 4) {
      mScript = subTag;
      ToLowerCase(mScript);
      mScript.Replace(0, 1, ToUpperCase(mScript[0]));
      position = 3;
    } else if (position <= 3 && slen == 2) {
      mRegion = subTag;
      ToUpperCase(mRegion);
      position = 4;
    } else if (position <= 4 && slen >= 5 && slen <= 8) {
      nsAutoCString lcSubTag(subTag);
      ToLowerCase(lcSubTag);
      mVariants.InsertElementSorted(lcSubTag);
      position = 4;
    }
  }
}

const nsCString Locale::AsString() const {
  nsCString tag;

  if (!mIsWellFormed) {
    tag.AppendLiteral("und");
    return tag;
  }

  tag.Append(mLanguage);

  if (!mScript.IsEmpty()) {
    tag.AppendLiteral("-");
    tag.Append(mScript);
  }

  if (!mRegion.IsEmpty()) {
    tag.AppendLiteral("-");
    tag.Append(mRegion);
  }

  for (const auto& variant : mVariants) {
    tag.AppendLiteral("-");
    tag.Append(variant);
  }

  if (!mPrivateUse.IsEmpty()) {
    if (tag.IsEmpty()) {
      tag.AppendLiteral("x");
    } else {
      tag.AppendLiteral("-x");
    }

    for (const auto& subTag : mPrivateUse) {
      tag.AppendLiteral("-");
      tag.Append(subTag);
    }
  }
  return tag;
}

const nsCString& Locale::GetLanguage() const { return mLanguage; }

const nsCString& Locale::GetScript() const { return mScript; }

const nsCString& Locale::GetRegion() const { return mRegion; }

const nsTArray<nsCString>& Locale::GetVariants() const { return mVariants; }

bool Locale::Matches(const Locale& aOther, bool aThisRange,
                     bool aOtherRange) const {
  if (!IsWellFormed() || !aOther.IsWellFormed()) {
    return false;
  }

  if ((!aThisRange || !mLanguage.IsEmpty()) &&
      (!aOtherRange || !aOther.mLanguage.IsEmpty()) &&
      !mLanguage.Equals(aOther.mLanguage)) {
    return false;
  }

  if ((!aThisRange || !mScript.IsEmpty()) &&
      (!aOtherRange || !aOther.mScript.IsEmpty()) &&
      !mScript.Equals(aOther.mScript)) {
    return false;
  }
  if ((!aThisRange || !mRegion.IsEmpty()) &&
      (!aOtherRange || !aOther.mRegion.IsEmpty()) &&
      !mRegion.Equals(aOther.mRegion)) {
    return false;
  }
  if ((!aThisRange || !mVariants.IsEmpty()) &&
      (!aOtherRange || !aOther.mVariants.IsEmpty()) &&
      mVariants != aOther.mVariants) {
    return false;
  }
  return true;
}

bool Locale::AddLikelySubtags() {
  const int32_t kLocaleMax = 160;
  char maxLocale[kLocaleMax];

  UErrorCode status = U_ZERO_ERROR;
  uloc_addLikelySubtags(AsString().get(), maxLocale, kLocaleMax, &status);

  if (U_FAILURE(status)) {
    return false;
  }

  nsDependentCString maxLocStr(maxLocale);
  Locale loc = Locale(maxLocStr);

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

void Locale::ClearVariants() { mVariants.Clear(); }

void Locale::ClearRegion() { mRegion.Truncate(); }
