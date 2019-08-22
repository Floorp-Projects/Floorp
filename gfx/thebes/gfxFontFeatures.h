/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONT_FEATURES_H
#define GFX_FONT_FEATURES_H

#include "nsTHashtable.h"
#include "nsTArray.h"
#include "nsString.h"

// An OpenType feature tag and value pair
struct gfxFontFeature {
  uint32_t
      mTag;  // see http://www.microsoft.com/typography/otspec/featuretags.htm
  uint32_t mValue;  // 0 = off, 1 = on, larger values may be used as parameters
                    // to features that select among multiple alternatives
};

inline bool operator<(const gfxFontFeature& a, const gfxFontFeature& b) {
  return (a.mTag < b.mTag) || ((a.mTag == b.mTag) && (a.mValue < b.mValue));
}

inline bool operator==(const gfxFontFeature& a, const gfxFontFeature& b) {
  return (a.mTag == b.mTag) && (a.mValue == b.mValue);
}

class nsAtom;

class gfxFontFeatureValueSet final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(gfxFontFeatureValueSet)

  gfxFontFeatureValueSet();

  struct ValueList {
    ValueList(const nsAString& aName, const nsTArray<uint32_t>& aSelectors)
        : name(aName), featureSelectors(aSelectors) {}
    nsString name;
    nsTArray<uint32_t> featureSelectors;
  };

  struct FeatureValues {
    uint32_t alternate;
    nsTArray<ValueList> valuelist;
  };

  // returns true if found, false otherwise
  bool GetFontFeatureValuesFor(const nsACString& aFamily,
                               uint32_t aVariantProperty,
                               nsAtom* aName,
                               nsTArray<uint32_t>& aValues);

  // Appends a new hash entry with given key values and returns a pointer to
  // mValues array to fill. This should be filled first.
  nsTArray<uint32_t>* AppendFeatureValueHashEntry(const nsACString& aFamily,
                                                  nsAtom* aName,
                                                  uint32_t aAlternate);

 private:
  // Private destructor, to discourage deletion outside of Release():
  ~gfxFontFeatureValueSet() {}

  struct FeatureValueHashKey {
    nsCString mFamily;
    uint32_t mPropVal;
    RefPtr<nsAtom> mName;

    FeatureValueHashKey() : mPropVal(0) {}
    FeatureValueHashKey(const nsACString& aFamily, uint32_t aPropVal,
                        nsAtom* aName)
        : mFamily(aFamily), mPropVal(aPropVal), mName(aName) {}
    FeatureValueHashKey(const FeatureValueHashKey& aKey)
        : mFamily(aKey.mFamily), mPropVal(aKey.mPropVal), mName(aKey.mName) {}
  };

  class FeatureValueHashEntry : public PLDHashEntryHdr {
   public:
    typedef const FeatureValueHashKey& KeyType;
    typedef const FeatureValueHashKey* KeyTypePointer;

    explicit FeatureValueHashEntry(KeyTypePointer aKey) {}
    FeatureValueHashEntry(FeatureValueHashEntry&& other)
        : PLDHashEntryHdr(std::move(other)),
          mKey(std::move(other.mKey)),
          mValues(std::move(other.mValues)) {
      NS_ERROR("Should not be called");
    }
    ~FeatureValueHashEntry() {}

    bool KeyEquals(const KeyTypePointer aKey) const;
    static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
    static PLDHashNumber HashKey(const KeyTypePointer aKey);
    enum { ALLOW_MEMMOVE = true };

    FeatureValueHashKey mKey;
    nsTArray<uint32_t> mValues;
  };

  nsTHashtable<FeatureValueHashEntry> mFontFeatureValues;
};

#endif
