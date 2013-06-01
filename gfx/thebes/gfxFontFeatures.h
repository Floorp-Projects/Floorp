/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FEATURES_H
#define GFX_FONT_FEATURES_H

#include "gfxTypes.h"
#include "nsTHashtable.h"
#include "nsTArray.h"
#include "nsString.h"

// An OpenType feature tag and value pair
struct gfxFontFeature {
    uint32_t mTag; // see http://www.microsoft.com/typography/otspec/featuretags.htm
    uint32_t mValue; // 0 = off, 1 = on, larger values may be used as parameters
                     // to features that select among multiple alternatives
};

inline bool
operator<(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag < b.mTag) || ((a.mTag == b.mTag) && (a.mValue < b.mValue));
}

inline bool
operator==(const gfxFontFeature& a, const gfxFontFeature& b)
{
    return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}

struct gfxAlternateValue {
    uint32_t           alternate;  // constants in gfxFontConstants.h
    nsString           value;      // string value to be looked up
};

inline bool
operator<(const gfxAlternateValue& a, const gfxAlternateValue& b)
{
    return (a.alternate < b.alternate) ||
        ((a.alternate == b.alternate) && (a.value < b.value));
}

inline bool
operator==(const gfxAlternateValue& a, const gfxAlternateValue& b)
{
    return (a.alternate == b.alternate) && (a.value == b.value);
}

class gfxFontFeatureValueSet {
public:
    NS_INLINE_DECL_REFCOUNTING(gfxFontFeatureValueSet)

    gfxFontFeatureValueSet();
    virtual ~gfxFontFeatureValueSet() {}

    struct ValueList {
        ValueList(const nsAString& aName, const nsTArray<uint32_t>& aSelectors)
          : name(aName), featureSelectors(aSelectors)
        {}
        nsString             name;
        nsTArray<uint32_t>   featureSelectors;
    };

    struct FeatureValues {
        uint32_t             alternate;
        nsTArray<ValueList>  valuelist;
    };

    // returns true if found, false otherwise
    bool
    GetFontFeatureValuesFor(const nsAString& aFamily,
                            uint32_t aVariantProperty,
                            const nsAString& aName,
                            nsTArray<uint32_t>& aValues);
    void
    AddFontFeatureValues(const nsAString& aFamily,
                const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aValues);

protected:
    struct FeatureValueHashKey {
        nsString mFamily;
        uint32_t mPropVal;
        nsString mName;

        FeatureValueHashKey()
            : mPropVal(0)
        { }
        FeatureValueHashKey(const nsAString& aFamily,
                            uint32_t aPropVal,
                            const nsAString& aName)
            : mFamily(aFamily), mPropVal(aPropVal), mName(aName)
        { }
        FeatureValueHashKey(const FeatureValueHashKey& aKey)
            : mFamily(aKey.mFamily), mPropVal(aKey.mPropVal), mName(aKey.mName)
        { }
    };

    class FeatureValueHashEntry : public PLDHashEntryHdr {
    public:
        typedef const FeatureValueHashKey &KeyType;
        typedef const FeatureValueHashKey *KeyTypePointer;

        FeatureValueHashEntry(KeyTypePointer aKey) { }
        FeatureValueHashEntry(const FeatureValueHashEntry& toCopy)
        {
            NS_ERROR("Should not be called");
        }
        ~FeatureValueHashEntry() { }

        bool KeyEquals(const KeyTypePointer aKey) const;
        static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
        static PLDHashNumber HashKey(const KeyTypePointer aKey);
        enum { ALLOW_MEMMOVE = true };

        FeatureValueHashKey mKey;
        nsTArray<uint32_t>  mValues;
    };

    nsTHashtable<FeatureValueHashEntry> mFontFeatureValues;
  };

#endif
