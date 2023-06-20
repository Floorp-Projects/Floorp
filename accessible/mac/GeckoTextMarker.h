/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GeckoTextMarker_H_
#define _GeckoTextMarker_H_

#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include "TextLeafRange.h"

namespace mozilla {
namespace a11y {

class Accessible;
class GeckoTextMarkerRange;

class GeckoTextMarker final {
 public:
  GeckoTextMarker(Accessible* aAcc, int32_t aOffset);

  explicit GeckoTextMarker(const TextLeafPoint& aTextLeafPoint)
      : mPoint(aTextLeafPoint) {}

  GeckoTextMarker() : mPoint() {}

  static GeckoTextMarker MarkerFromAXTextMarker(Accessible* aDoc,
                                                AXTextMarkerRef aTextMarker);

  static GeckoTextMarker MarkerFromIndex(Accessible* aRoot, int32_t aIndex);

  AXTextMarkerRef CreateAXTextMarker();

  bool Next();

  bool Previous();

  GeckoTextMarkerRange LeftWordRange() const;

  GeckoTextMarkerRange RightWordRange() const;

  GeckoTextMarkerRange LineRange() const;

  GeckoTextMarkerRange LeftLineRange() const;

  GeckoTextMarkerRange RightLineRange() const;

  GeckoTextMarkerRange ParagraphRange() const;

  GeckoTextMarkerRange StyleRange() const;

  int32_t& Offset() { return mPoint.mOffset; }

  Accessible* Leaf();

  Accessible* Acc() const { return mPoint.mAcc; }

  bool IsValid() const { return !!mPoint; };

  bool operator<(const GeckoTextMarker& aOther) const {
    return mPoint < aOther.mPoint;
  }

  bool operator==(const GeckoTextMarker& aOther) const {
    return mPoint == aOther.mPoint;
  }

  TextLeafPoint mPoint;
};

class GeckoTextMarkerRange final {
 public:
  GeckoTextMarkerRange(const GeckoTextMarker& aStart,
                       const GeckoTextMarker& aEnd)
      : mRange(aStart.mPoint, aEnd.mPoint) {}

  GeckoTextMarkerRange(const TextLeafPoint& aStart, const TextLeafPoint& aEnd)
      : mRange(aStart, aEnd) {}

  GeckoTextMarkerRange() {}

  explicit GeckoTextMarkerRange(Accessible* aAccessible);

  static GeckoTextMarkerRange MarkerRangeFromAXTextMarkerRange(
      Accessible* aDoc, AXTextMarkerRangeRef aTextMarkerRange);

  AXTextMarkerRangeRef CreateAXTextMarkerRange();

  bool IsValid() const { return !!mRange.Start() && !!mRange.End(); };

  GeckoTextMarker Start() { return GeckoTextMarker(mRange.Start()); }

  GeckoTextMarker End() { return GeckoTextMarker(mRange.End()); }

  /**
   * Return text enclosed by the range.
   */
  NSString* Text() const;

  /**
   * Return the attributed text enclosed by the range.
   */
  NSAttributedString* AttributedText() const;

  /**
   * Return length of characters enclosed by the range.
   */
  int32_t Length() const;

  /**
   * Return screen bounds of range.
   */
  NSValue* Bounds() const;

  /**
   * Set the current range as the DOM selection.
   */
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Select() const;

  /**
   * Crops the range if it overlaps the given accessible element boundaries.
   * Return true if successfully cropped. false if the range does not intersect
   * with the container.
   */
  bool Crop(Accessible* aContainer) { return mRange.Crop(aContainer); }

  TextLeafRange mRange;
};

}  // namespace a11y
}  // namespace mozilla

#endif
