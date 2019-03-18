/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* bit vectors for sets of CSS properties */

#ifndef nsCSSPropertyIDSet_h__
#define nsCSSPropertyIDSet_h__

#include "mozilla/ArrayUtils.h"

#include "nsCSSPropertyID.h"
#include <limits.h>  // for CHAR_BIT
#include <initializer_list>

// For COMPOSITOR_ANIMATABLE_PROPERTY_LIST and
// COMPOSITOR_ANIMATABLE_PROPERTY_LIST_LENGTH
#include "mozilla/CompositorAnimatableProperties.h"

/**
 * nsCSSPropertyIDSet maintains a set of non-shorthand CSS properties.  In
 * other words, for each longhand CSS property we support, it has a bit
 * for whether that property is in the set.
 */
class nsCSSPropertyIDSet {
 public:
  nsCSSPropertyIDSet() { Empty(); }
  // auto-generated copy-constructor OK

  explicit constexpr nsCSSPropertyIDSet(
      std::initializer_list<nsCSSPropertyID> aProperties)
      : mProperties{0} {
    for (auto property : aProperties) {
      size_t p = property;
      mProperties[p / kBitsInChunk] |= property_set_type(1)
                                       << (p % kBitsInChunk);
    }
  }

  void AssertInSetRange(nsCSSPropertyID aProperty) const {
    NS_ASSERTION(0 <= aProperty && aProperty < eCSSProperty_COUNT_no_shorthands,
                 "out of bounds");
  }

  // Conversion of aProperty to |size_t| after AssertInSetRange
  // lets the compiler generate significantly tighter code.

  void AddProperty(nsCSSPropertyID aProperty) {
    AssertInSetRange(aProperty);
    size_t p = aProperty;
    mProperties[p / kBitsInChunk] |= property_set_type(1) << (p % kBitsInChunk);
  }

  void RemoveProperty(nsCSSPropertyID aProperty) {
    AssertInSetRange(aProperty);
    size_t p = aProperty;
    mProperties[p / kBitsInChunk] &=
        ~(property_set_type(1) << (p % kBitsInChunk));
  }

  bool HasProperty(nsCSSPropertyID aProperty) const {
    AssertInSetRange(aProperty);
    size_t p = aProperty;
    return (mProperties[p / kBitsInChunk] &
            (property_set_type(1) << (p % kBitsInChunk))) != 0;
  }

  // Returns an nsCSSPropertyIDSet including all properties that can be run
  // on the compositor.
  static constexpr nsCSSPropertyIDSet CompositorAnimatables() {
    return nsCSSPropertyIDSet(COMPOSITOR_ANIMATABLE_PROPERTY_LIST);
  }

  static constexpr size_t CompositorAnimatableCount() {
    return COMPOSITOR_ANIMATABLE_PROPERTY_LIST_LENGTH;
  }

  static constexpr size_t CompositorAnimatableDisplayItemCount() {
    // We have 3 individual transforms, and they also use
    // DisplayItemType::TYPE_TRANSFORM.
    // TODO: Bug 1429305: In order to support motion path transoform, we have
    // to update this count.
    return COMPOSITOR_ANIMATABLE_PROPERTY_LIST_LENGTH - 3;
  }

  static constexpr nsCSSPropertyIDSet TransformLikeProperties() {
    // FIXME: Bug 1186329: Add motion-path transform properties.
    return nsCSSPropertyIDSet{eCSSProperty_transform, eCSSProperty_translate,
                              eCSSProperty_rotate, eCSSProperty_scale};
  }

  static constexpr nsCSSPropertyIDSet OpacityProperties() {
    return nsCSSPropertyIDSet{eCSSProperty_opacity};
  }

  bool Intersects(const nsCSSPropertyIDSet& aOther) const {
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      if (mProperties[i] & aOther.mProperties[i]) {
        return true;
      }
    }
    return false;
  }

  void Empty() { memset(mProperties, 0, sizeof(mProperties)); }

  void AssertIsEmpty(const char* aText) const {
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      NS_ASSERTION(mProperties[i] == 0, aText);
    }
  }

  bool Equals(const nsCSSPropertyIDSet& aOther) const {
    return mozilla::ArrayEqual(mProperties, aOther.mProperties);
  }

  bool IsEmpty() const {
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      if (mProperties[i] != 0) {
        return false;
      }
    }
    return true;
  }

  bool IsSubsetOf(const nsCSSPropertyIDSet& aOther) const {
    return this->Intersect(aOther).Equals(*this);
  }

  // Return a new nsCSSPropertyIDSet which is the inverse of this set.
  nsCSSPropertyIDSet Inverse() const {
    nsCSSPropertyIDSet result;
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      result.mProperties[i] = ~mProperties[i];
    }
    return result;
  }

  // Returns a new nsCSSPropertyIDSet with all properties that are both in
  // this set and |aOther|.
  nsCSSPropertyIDSet Intersect(const nsCSSPropertyIDSet& aOther) const {
    nsCSSPropertyIDSet result;
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      result.mProperties[i] = mProperties[i] & aOther.mProperties[i];
    }
    return result;
  }

  // Return a new nsCSSPropertyIDSet with all properties that are in either
  // this set or |aOther| but not both.
  nsCSSPropertyIDSet Xor(const nsCSSPropertyIDSet& aOther) const {
    nsCSSPropertyIDSet result;
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      result.mProperties[i] = mProperties[i] ^ aOther.mProperties[i];
    }
    return result;
  }

  nsCSSPropertyIDSet& operator|=(const nsCSSPropertyIDSet& aOther) {
    for (size_t i = 0; i < mozilla::ArrayLength(mProperties); ++i) {
      mProperties[i] |= aOther.mProperties[i];
    }
    return *this;
  }

 private:
  typedef unsigned long property_set_type;

 public:
  // number of bits in |property_set_type|.
  static const size_t kBitsInChunk = sizeof(property_set_type) * CHAR_BIT;
  // number of |property_set_type|s in the set
  static const size_t kChunkCount =
      (eCSSProperty_COUNT_no_shorthands + kBitsInChunk - 1) / kBitsInChunk;

  /*
   * For fast enumeration of all the bits that are set, callers can
   * check each chunk against zero (since in normal cases few bits are
   * likely to be set).
   */
  bool HasPropertyInChunk(size_t aChunk) const {
    return mProperties[aChunk] != 0;
  }
  bool HasPropertyAt(size_t aChunk, size_t aBit) const {
    return (mProperties[aChunk] & (property_set_type(1) << aBit)) != 0;
  }
  static nsCSSPropertyID CSSPropertyAt(size_t aChunk, size_t aBit) {
    return nsCSSPropertyID(aChunk * kBitsInChunk + aBit);
  }

 private:
  property_set_type mProperties[kChunkCount];
};

#endif /* !defined(nsCSSPropertyIDSet_h__) */
