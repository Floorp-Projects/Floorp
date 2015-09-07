/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_UNICODEPROPERTIES_H
#define NS_UNICODEPROPERTIES_H

#include "nsBidiUtils.h"
#include "nsIUGenCategory.h"
#include "nsUnicodeScriptCodes.h"

const nsCharProps2& GetCharProps2(uint32_t aCh);

namespace mozilla {

namespace unicode {

extern nsIUGenCategory::nsUGenCategory sDetailedToGeneralCategory[];

// Return whether the char has a mirrored-pair counterpart.
uint32_t GetMirroredChar(uint32_t aCh);

bool HasMirroredChar(uint32_t aChr);

uint8_t GetCombiningClass(uint32_t aCh);

// returns the detailed General Category in terms of HB_UNICODE_* values
inline uint8_t GetGeneralCategory(uint32_t aCh) {
  return GetCharProps2(aCh).mCategory;
}

// returns the simplified Gen Category as defined in nsIUGenCategory
inline nsIUGenCategory::nsUGenCategory GetGenCategory(uint32_t aCh) {
  return sDetailedToGeneralCategory[GetGeneralCategory(aCh)];
}

inline uint8_t GetScriptCode(uint32_t aCh) {
  return GetCharProps2(aCh).mScriptCode;
}

uint32_t GetScriptTagForCode(int32_t aScriptCode);

inline nsCharType GetBidiCat(uint32_t aCh) {
  return nsCharType(GetCharProps2(aCh).mBidiCategory);
}

/* This MUST match the values assigned by genUnicodePropertyData.pl! */
enum VerticalOrientation {
  VERTICAL_ORIENTATION_U  = 0,
  VERTICAL_ORIENTATION_R  = 1,
  VERTICAL_ORIENTATION_Tu = 2,
  VERTICAL_ORIENTATION_Tr = 3
};

inline VerticalOrientation GetVerticalOrientation(uint32_t aCh) {
  return VerticalOrientation(GetCharProps2(aCh).mVertOrient);
}

enum XidmodType {
  XIDMOD_RECOMMENDED,
  XIDMOD_INCLUSION,
  XIDMOD_UNCOMMON_USE,
  XIDMOD_TECHNICAL,
  XIDMOD_OBSOLETE,
  XIDMOD_ASPIRATIONAL,
  XIDMOD_LIMITED_USE,
  XIDMOD_EXCLUSION,
  XIDMOD_NOT_XID,
  XIDMOD_NOT_NFKC,
  XIDMOD_DEFAULT_IGNORABLE,
  XIDMOD_DEPRECATED,
  XIDMOD_NOT_CHARS
};

inline XidmodType GetIdentifierModification(uint32_t aCh) {
  return XidmodType(GetCharProps2(aCh).mXidmod);
}

/**
 * Return the numeric value of the character. The value returned is the value
 * of the Numeric_Value in field 7 of the UCD, or -1 if field 7 is empty.
 * To restrict to decimal digits, the caller should also check whether
 * GetGeneralCategory returns HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER
 */
inline int8_t GetNumericValue(uint32_t aCh) {
  return GetCharProps2(aCh).mNumericValue;
}

enum HanVariantType {
  HVT_NotHan = 0x0,
  HVT_SimplifiedOnly = 0x1,
  HVT_TraditionalOnly = 0x2,
  HVT_AnyHan = 0x3
};

HanVariantType GetHanVariant(uint32_t aCh);

uint32_t GetFullWidth(uint32_t aCh);

bool IsClusterExtender(uint32_t aCh, uint8_t aCategory);

inline bool IsClusterExtender(uint32_t aCh) {
  return IsClusterExtender(aCh, GetGeneralCategory(aCh));
}

// Case mappings for the full Unicode range;
// note that it may be worth testing for ASCII chars and taking
// a separate fast-path before calling these, in perf-critical places
uint32_t GetUppercase(uint32_t aCh);
uint32_t GetLowercase(uint32_t aCh);
uint32_t GetTitlecaseForLower(uint32_t aCh); // maps LC to titlecase, UC unchanged
uint32_t GetTitlecaseForAll(uint32_t aCh); // maps both UC and LC to titlecase

// A simple iterator for a string of char16_t codepoints that advances
// by Unicode grapheme clusters
class ClusterIterator
{
public:
    ClusterIterator(const char16_t* aText, uint32_t aLength)
        : mPos(aText), mLimit(aText + aLength)
#ifdef DEBUG
        , mText(aText)
#endif
    { }

    operator const char16_t* () const {
        return mPos;
    }

    bool AtEnd() const {
        return mPos >= mLimit;
    }

    void Next();

private:
    const char16_t* mPos;
    const char16_t* mLimit;
#ifdef DEBUG
    const char16_t* mText;
#endif
};

} // end namespace unicode

} // end namespace mozilla

#endif /* NS_UNICODEPROPERTIES_H */
