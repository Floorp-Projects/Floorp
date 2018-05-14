/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxFontFeatures.h"
#include "nsUnicharUtils.h"
#include "nsHashKeys.h"

using namespace mozilla;

gfxFontFeatureValueSet::gfxFontFeatureValueSet()
    : mFontFeatureValues(8)
{
}

bool
gfxFontFeatureValueSet::GetFontFeatureValuesFor(const nsAString& aFamily,
                                                uint32_t aVariantProperty,
                                                const nsAString& aName,
                                                nsTArray<uint32_t>& aValues)
{
    nsAutoString family(aFamily);
    ToLowerCase(family);
    FeatureValueHashKey key(family, aVariantProperty, aName);

    aValues.Clear();
    FeatureValueHashEntry *entry = mFontFeatureValues.GetEntry(key);
    if (entry) {
        NS_ASSERTION(entry->mValues.Length() > 0,
                     "null array of font feature values");
        aValues.AppendElements(entry->mValues);
        return true;
    }

    return false;
}


void
gfxFontFeatureValueSet::AddFontFeatureValues(const nsAString& aFamily,
                 const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aValues)
{
    nsAutoString family(aFamily);
    ToLowerCase(family);

    uint32_t i, numFeatureValues = aValues.Length();
    for (i = 0; i < numFeatureValues; i++) {
        const FeatureValues& fv = aValues.ElementAt(i);
        uint32_t alternate = fv.alternate;
        uint32_t j, numValues = fv.valuelist.Length();
        for (j = 0; j < numValues; j++) {
            const ValueList& v = fv.valuelist.ElementAt(j);
            auto* array = AppendFeatureValueHashEntry(family, v.name, alternate);
            *array = v.featureSelectors;
        }
    }
}


nsTArray<uint32_t>*
gfxFontFeatureValueSet::AppendFeatureValueHashEntry(const nsAString& aFamily,
                                                    const nsAString& aName,
                                                    uint32_t aAlternate)
{
    FeatureValueHashKey key(aFamily, aAlternate, aName);
    FeatureValueHashEntry *entry = mFontFeatureValues.PutEntry(key);
    entry->mKey = key;
    return &entry->mValues;
}

bool
gfxFontFeatureValueSet::FeatureValueHashEntry::KeyEquals(
                                               const KeyTypePointer aKey) const
{
    return aKey->mPropVal == mKey.mPropVal &&
           aKey->mFamily.Equals(mKey.mFamily) &&
           aKey->mName.Equals(mKey.mName);
}

PLDHashNumber
gfxFontFeatureValueSet::FeatureValueHashEntry::HashKey(
                                                     const KeyTypePointer aKey)
{
    return HashString(aKey->mFamily) + HashString(aKey->mName) +
           aKey->mPropVal * uint32_t(0xdeadbeef);
}

