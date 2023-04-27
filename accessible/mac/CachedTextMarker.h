/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CachedTextMarker_H_
#define _CachedTextMarker_H_

#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include "TextLeafRange.h"

namespace mozilla {
namespace a11y {

class Accessible;
class CachedTextMarkerRange;

class CachedTextMarker final {
 public:
  CachedTextMarker(Accessible* aAcc, int32_t aOffset);

  explicit CachedTextMarker(const TextLeafPoint& aTextLeafPoint)
      : mPoint(aTextLeafPoint) {}

  CachedTextMarker() : mPoint() {}

  CachedTextMarker(Accessible* aDoc, AXTextMarkerRef aTextMarker);

  static CachedTextMarker MarkerFromIndex(Accessible* aRoot, int32_t aIndex);

  AXTextMarkerRef CreateAXTextMarker();

  bool Next();

  bool Previous();

  CachedTextMarkerRange LeftWordRange() const;

  CachedTextMarkerRange RightWordRange() const;

  CachedTextMarkerRange LineRange() const;

  CachedTextMarkerRange LeftLineRange() const;

  CachedTextMarkerRange RightLineRange() const;

  CachedTextMarkerRange ParagraphRange() const;

  CachedTextMarkerRange StyleRange() const;

  Accessible* Leaf();

  bool IsValid() const { return !!mPoint; };

  bool operator<(const CachedTextMarker& aOther) const {
    return mPoint < aOther.mPoint;
  }

  bool operator==(const CachedTextMarker& aOther) const {
    return mPoint == aOther.mPoint;
  }

  TextLeafPoint mPoint;
};

class CachedTextMarkerRange final {
 public:
  CachedTextMarkerRange(const CachedTextMarker& aStart,
                        const CachedTextMarker& aEnd)
      : mRange(aStart.mPoint, aEnd.mPoint) {}

  CachedTextMarkerRange(const TextLeafPoint& aStart, const TextLeafPoint& aEnd)
      : mRange(aStart, aEnd) {}

  CachedTextMarkerRange() {}

  CachedTextMarkerRange(Accessible* aDoc,
                        AXTextMarkerRangeRef aTextMarkerRange);

  explicit CachedTextMarkerRange(Accessible* aAccessible);

  AXTextMarkerRangeRef CreateAXTextMarkerRange();

  bool IsValid() const { return !!mRange.Start() && !!mRange.End(); };

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
  bool Crop(Accessible* aContainer);

  TextLeafRange mRange;
};

}  // namespace a11y
}  // namespace mozilla

#endif
