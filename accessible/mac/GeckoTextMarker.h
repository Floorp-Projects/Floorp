/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GeckoTextMarker_H_
#define _GeckoTextMarker_H_

#include <Foundation/Foundation.h>
#import "LegacyTextMarker.h"
#import "CachedTextMarker.h"

namespace mozilla {
namespace a11y {

class Accessible;
class GeckoTextMarkerRange;

class GeckoTextMarker {
 public:
  GeckoTextMarker();

  GeckoTextMarker(const GeckoTextMarker& aOther) {
    mLegacy = aOther.mLegacy;
    if (mLegacy) {
      mLegacyTextMarker = aOther.mLegacyTextMarker;
    } else {
      mCachedTextMarker = aOther.mCachedTextMarker;
    }
  }

  explicit GeckoTextMarker(const LegacyTextMarker& aTextMarker)
      : mLegacy(true), mLegacyTextMarker(aTextMarker) {}

  explicit GeckoTextMarker(const CachedTextMarker& aTextMarker)
      : mLegacy(false), mCachedTextMarker(aTextMarker) {}

  explicit GeckoTextMarker(const TextLeafPoint& aTextLeafPoint)
      : mLegacy(false), mCachedTextMarker(aTextLeafPoint) {}

  GeckoTextMarker(Accessible* aContainer, int32_t aOffset);

  static GeckoTextMarker MarkerFromAXTextMarker(Accessible* aDoc,
                                                AXTextMarkerRef aTextMarker);

  static GeckoTextMarker MarkerFromIndex(Accessible* aRoot, int32_t aIndex);

  AXTextMarkerRef CreateAXTextMarker();

  bool Next() {
    return mLegacy ? mLegacyTextMarker.Next() : mCachedTextMarker.Next();
  }

  bool Previous() {
    return mLegacy ? mLegacyTextMarker.Previous()
                   : mCachedTextMarker.Previous();
  }

  GeckoTextMarkerRange LeftWordRange() const;

  GeckoTextMarkerRange RightWordRange() const;

  GeckoTextMarkerRange LineRange() const;

  GeckoTextMarkerRange LeftLineRange() const;

  GeckoTextMarkerRange RightLineRange() const;

  GeckoTextMarkerRange ParagraphRange() const;

  GeckoTextMarkerRange StyleRange() const;

  Accessible* Leaf() {
    return mLegacy ? mLegacyTextMarker.Leaf() : mCachedTextMarker.Leaf();
  }

  int32_t& Offset() {
    return mLegacy ? mLegacyTextMarker.mOffset
                   : mCachedTextMarker.mPoint.mOffset;
  }

  Accessible* Acc() const {
    return mLegacy ? mLegacyTextMarker.mContainer
                   : mCachedTextMarker.mPoint.mAcc;
  }

  bool IsValid() const {
    return mLegacy ? mLegacyTextMarker.IsValid() : mCachedTextMarker.IsValid();
  }

  bool operator<(const GeckoTextMarker& aPoint) const {
    return mLegacy ? (mLegacyTextMarker < aPoint.mLegacyTextMarker)
                   : (mCachedTextMarker < aPoint.mCachedTextMarker);
  }

  bool operator==(const GeckoTextMarker& aPoint) const {
    return mLegacy ? (mLegacyTextMarker == aPoint.mLegacyTextMarker)
                   : (mCachedTextMarker == aPoint.mCachedTextMarker);
  }

 private:
  bool mLegacy;
  union {
    LegacyTextMarker mLegacyTextMarker;
    CachedTextMarker mCachedTextMarker;
  };

  friend class GeckoTextMarkerRange;
};

class GeckoTextMarkerRange {
 public:
  GeckoTextMarkerRange();

  GeckoTextMarkerRange(const GeckoTextMarkerRange& aOther) {
    mLegacy = aOther.mLegacy;
    if (mLegacy) {
      mLegacyTextMarkerRange = aOther.mLegacyTextMarkerRange;
    } else {
      mCachedTextMarkerRange = aOther.mCachedTextMarkerRange;
    }
  }

  explicit GeckoTextMarkerRange(const LegacyTextMarkerRange& aTextMarkerRange)
      : mLegacy(true), mLegacyTextMarkerRange(aTextMarkerRange) {}

  explicit GeckoTextMarkerRange(const CachedTextMarkerRange& aTextMarkerRange)
      : mLegacy(false), mCachedTextMarkerRange(aTextMarkerRange) {}

  GeckoTextMarkerRange(const GeckoTextMarker& aStart,
                       const GeckoTextMarker& aEnd) {
    MOZ_ASSERT(aStart.mLegacy == aEnd.mLegacy);
    mLegacy = aStart.mLegacy;
    if (mLegacy) {
      mLegacyTextMarkerRange = LegacyTextMarkerRange(aStart.mLegacyTextMarker,
                                                     aEnd.mLegacyTextMarker);
    } else {
      mCachedTextMarkerRange = CachedTextMarkerRange(aStart.mCachedTextMarker,
                                                     aEnd.mCachedTextMarker);
    }
  }

  explicit GeckoTextMarkerRange(Accessible* aAccessible);

  static GeckoTextMarkerRange MarkerRangeFromAXTextMarkerRange(
      Accessible* aDoc, AXTextMarkerRangeRef aTextMarkerRange);

  AXTextMarkerRangeRef CreateAXTextMarkerRange();

  bool IsValid() const {
    return mLegacy ? mLegacyTextMarkerRange.IsValid()
                   : mCachedTextMarkerRange.IsValid();
  }

  GeckoTextMarker Start() {
    return mLegacy ? GeckoTextMarker(mLegacyTextMarkerRange.mStart)
                   : GeckoTextMarker(mCachedTextMarkerRange.mRange.Start());
  }

  GeckoTextMarker End() {
    return mLegacy ? GeckoTextMarker(mLegacyTextMarkerRange.mEnd)
                   : GeckoTextMarker(mCachedTextMarkerRange.mRange.End());
  }

  /**
   * Return text enclosed by the range.
   */
  NSString* Text() const {
    return mLegacy ? mLegacyTextMarkerRange.Text()
                   : mCachedTextMarkerRange.Text();
  }

  /**
   * Return the attributed text enclosed by the range.
   */
  NSAttributedString* AttributedText() const {
    return mLegacy ? mLegacyTextMarkerRange.AttributedText()
                   : mCachedTextMarkerRange.AttributedText();
  }

  /**
   * Return length of characters enclosed by the range.
   */
  int32_t Length() const {
    return mLegacy ? mLegacyTextMarkerRange.Length()
                   : mCachedTextMarkerRange.Length();
  }

  /**
   * Return screen bounds of range.
   */
  NSValue* Bounds() const {
    return mLegacy ? mLegacyTextMarkerRange.Bounds()
                   : mCachedTextMarkerRange.Bounds();
  }

  /**
   * Set the current range as the DOM selection.
   */
  void Select() const {
    mLegacy ? mLegacyTextMarkerRange.Select() : mCachedTextMarkerRange.Select();
  }

  /**
   * Crops the range if it overlaps the given accessible element boundaries.
   * Return true if successfully cropped. false if the range does not intersect
   * with the container.
   */
  bool Crop(Accessible* aContainer) {
    return mLegacy ? mLegacyTextMarkerRange.Crop(aContainer)
                   : mCachedTextMarkerRange.Crop(aContainer);
  }

 private:
  bool mLegacy;
  union {
    LegacyTextMarkerRange mLegacyTextMarkerRange;
    CachedTextMarkerRange mCachedTextMarkerRange;
  };
};

}  // namespace a11y
}  // namespace mozilla

#endif
