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

namespace mozilla {
namespace a11y {

class Accessible;
class GeckoTextMarkerRange;

class GeckoTextMarker {
 public:
  GeckoTextMarker();

  GeckoTextMarker(const GeckoTextMarker& aOther) {
    mLegacyTextMarker = aOther.mLegacyTextMarker;
  }

  explicit GeckoTextMarker(const LegacyTextMarker& aTextMarker)
      : mLegacyTextMarker(aTextMarker) {}

  GeckoTextMarker(Accessible* aContainer, int32_t aOffset);

  static GeckoTextMarker MarkerFromAXTextMarker(Accessible* aDoc,
                                                AXTextMarkerRef aTextMarker);

  static GeckoTextMarker MarkerFromIndex(Accessible* aRoot, int32_t aIndex);

  AXTextMarkerRef CreateAXTextMarker();

  bool Next() { return mLegacyTextMarker.Next(); }

  bool Previous() { return mLegacyTextMarker.Previous(); }

  GeckoTextMarkerRange LeftWordRange() const;

  GeckoTextMarkerRange RightWordRange() const;

  GeckoTextMarkerRange LeftLineRange() const;

  GeckoTextMarkerRange RightLineRange() const;

  GeckoTextMarkerRange ParagraphRange() const;

  GeckoTextMarkerRange StyleRange() const;

  Accessible* Leaf() { return mLegacyTextMarker.Leaf(); }

  int32_t& Offset() { return mLegacyTextMarker.mOffset; }

  Accessible* Acc() const { return mLegacyTextMarker.mContainer; }

  bool IsValid() const { return mLegacyTextMarker.IsValid(); }

  bool operator<(const GeckoTextMarker& aPoint) const {
    return mLegacyTextMarker < aPoint.mLegacyTextMarker;
  }

  bool operator==(const GeckoTextMarker& aPoint) const {
    return mLegacyTextMarker == aPoint.mLegacyTextMarker;
  }

 private:
  LegacyTextMarker mLegacyTextMarker;

  friend class GeckoTextMarkerRange;
};

class GeckoTextMarkerRange {
 public:
  GeckoTextMarkerRange();

  GeckoTextMarkerRange(const GeckoTextMarkerRange& aOther) {
    mLegacyTextMarkerRange = aOther.mLegacyTextMarkerRange;
  }

  explicit GeckoTextMarkerRange(const LegacyTextMarkerRange& aTextMarkerRange)
      : mLegacyTextMarkerRange(aTextMarkerRange) {}

  GeckoTextMarkerRange(const GeckoTextMarker& aStart,
                       const GeckoTextMarker& aEnd) {
    mLegacyTextMarkerRange =
        LegacyTextMarkerRange(aStart.mLegacyTextMarker, aEnd.mLegacyTextMarker);
  }

  explicit GeckoTextMarkerRange(Accessible* aAccessible);

  static GeckoTextMarkerRange MarkerRangeFromAXTextMarkerRange(
      Accessible* aDoc, AXTextMarkerRangeRef aTextMarkerRange);

  AXTextMarkerRangeRef CreateAXTextMarkerRange();

  bool IsValid() const { return mLegacyTextMarkerRange.IsValid(); }

  GeckoTextMarker Start() {
    return GeckoTextMarker(mLegacyTextMarkerRange.mStart);
  }

  GeckoTextMarker End() { return GeckoTextMarker(mLegacyTextMarkerRange.mEnd); }

  /**
   * Return text enclosed by the range.
   */
  NSString* Text() const { return mLegacyTextMarkerRange.Text(); }

  /**
   * Return the attributed text enclosed by the range.
   */
  NSAttributedString* AttributedText() const {
    return mLegacyTextMarkerRange.AttributedText();
  }

  /**
   * Return length of characters enclosed by the range.
   */
  int32_t Length() const { return mLegacyTextMarkerRange.Length(); }

  /**
   * Return screen bounds of range.
   */
  NSValue* Bounds() const { return mLegacyTextMarkerRange.Bounds(); }

  /**
   * Set the current range as the DOM selection.
   */
  void Select() const { mLegacyTextMarkerRange.Select(); }

  /**
   * Crops the range if it overlaps the given accessible element boundaries.
   * Return true if successfully cropped. false if the range does not intersect
   * with the container.
   */
  bool Crop(Accessible* aContainer) {
    return mLegacyTextMarkerRange.Crop(aContainer);
  }

 private:
  LegacyTextMarkerRange mLegacyTextMarkerRange;
};

}  // namespace a11y
}  // namespace mozilla

#endif
