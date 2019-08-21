/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontFeatures.h"
#include "nsUnicharUtils.h"
#include "nsHashKeys.h"

using namespace mozilla;

gfxFontFeatureValueSet::gfxFontFeatureValueSet() : mFontFeatureValues(8) {}

bool gfxFontFeatureValueSet::GetFontFeatureValuesFor(
    const nsACString& aFamily, uint32_t aVariantProperty,
    const nsAString& aName, nsTArray<uint32_t>& aValues) {
  nsAutoCString family(aFamily);
  ToLowerCase(family);
  FeatureValueHashKey key(family, aVariantProperty, aName);

  aValues.Clear();
  FeatureValueHashEntry* entry = mFontFeatureValues.GetEntry(key);
  if (entry) {
    NS_ASSERTION(entry->mValues.Length() > 0,
                 "null array of font feature values");
    aValues.AppendElements(entry->mValues);
    return true;
  }

  return false;
}

nsTArray<uint32_t>* gfxFontFeatureValueSet::AppendFeatureValueHashEntry(
    const nsACString& aFamily, const nsAString& aName, uint32_t aAlternate) {
  FeatureValueHashKey key(aFamily, aAlternate, aName);
  FeatureValueHashEntry* entry = mFontFeatureValues.PutEntry(key);
  entry->mKey = key;
  return &entry->mValues;
}

bool gfxFontFeatureValueSet::FeatureValueHashEntry::KeyEquals(
    const KeyTypePointer aKey) const {
  return aKey->mPropVal == mKey.mPropVal &&
         aKey->mFamily.Equals(mKey.mFamily) && aKey->mName.Equals(mKey.mName);
}

PLDHashNumber gfxFontFeatureValueSet::FeatureValueHashEntry::HashKey(
    const KeyTypePointer aKey) {
  return HashString(aKey->mFamily) + HashString(aKey->mName) +
         aKey->mPropVal * uint32_t(0xdeadbeef);
}
