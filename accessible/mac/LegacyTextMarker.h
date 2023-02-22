/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _LegacyTextMarker_H_
#define _LegacyTextMarker_H_

#include <ApplicationServices/ApplicationServices.h>
#include <Foundation/Foundation.h>

#include "HyperTextAccessibleWrap.h"
#include "PlatformExtTypes.h"
#include "SDKDeclarations.h"

namespace mozilla {
namespace a11y {

class Accessible;
class LegacyTextMarkerRange;

class LegacyTextMarker final {
 public:
  LegacyTextMarker(Accessible* aContainer, int32_t aOffset)
      : mContainer(aContainer), mOffset(aOffset) {}

  LegacyTextMarker(const LegacyTextMarker& aPoint)
      : mContainer(aPoint.mContainer), mOffset(aPoint.mOffset) {}

  LegacyTextMarker(Accessible* aDoc, AXTextMarkerRef aTextMarker);

  LegacyTextMarker() : mContainer(nullptr), mOffset(0) {}

  static LegacyTextMarker MarkerFromIndex(Accessible* aRoot, int32_t aIndex);

  AXTextMarkerRef CreateAXTextMarker();

  bool Next();

  bool Previous();

  // Return a range with the given type relative to this marker.
  LegacyTextMarkerRange Range(EWhichRange aRangeType);

  Accessible* Leaf();

  bool IsValid() const { return !!mContainer; };

  bool operator<(const LegacyTextMarker& aPoint) const;

  bool operator==(const LegacyTextMarker& aPoint) const {
    return mContainer == aPoint.mContainer && mOffset == aPoint.mOffset;
  }

  Accessible* mContainer;
  int32_t mOffset;

  HyperTextAccessibleWrap* ContainerAsHyperTextWrap() const {
    return (mContainer && mContainer->IsLocal())
               ? static_cast<HyperTextAccessibleWrap*>(
                     mContainer->AsLocal()->AsHyperText())
               : nullptr;
  }

 private:
  bool IsEditableRoot();
};

class LegacyTextMarkerRange final {
 public:
  LegacyTextMarkerRange(const LegacyTextMarker& aStart,
                        const LegacyTextMarker& aEnd)
      : mStart(aStart), mEnd(aEnd) {}

  LegacyTextMarkerRange() {}

  LegacyTextMarkerRange(Accessible* aDoc,
                        AXTextMarkerRangeRef aTextMarkerRange);

  explicit LegacyTextMarkerRange(Accessible* aAccessible);

  AXTextMarkerRangeRef CreateAXTextMarkerRange();

  bool IsValid() const { return !!mStart.mContainer && !!mEnd.mContainer; };

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

  LegacyTextMarker mStart;
  LegacyTextMarker mEnd;
};

// XXX: Temporary for next patch so we don't have to do a massive rename.
typedef LegacyTextMarker GeckoTextMarker;
typedef LegacyTextMarkerRange GeckoTextMarkerRange;

}  // namespace a11y
}  // namespace mozilla

#endif
