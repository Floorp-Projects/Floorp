/* clang-format off */
/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* clang-format on */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _GeckoTextMarker_H_
#define _GeckoTextMarker_H_

typedef CFTypeRef AXTextMarkerRef;
typedef CFTypeRef AXTextMarkerRangeRef;

namespace mozilla {
namespace a11y {

class AccessibleOrProxy;
class GeckoTextMarkerRange;

class GeckoTextMarker final {
 public:
  GeckoTextMarker(const AccessibleOrProxy& aContainer, int32_t aOffset)
      : mContainer(aContainer), mOffset(aOffset) {}

  GeckoTextMarker(const GeckoTextMarker& aPoint)
      : mContainer(aPoint.mContainer), mOffset(aPoint.mOffset) {}

  GeckoTextMarker(AccessibleOrProxy aDoc, AXTextMarkerRef aTextMarker);

  GeckoTextMarker() : mContainer(nullptr), mOffset(0) {}

  static GeckoTextMarker MarkerFromIndex(const AccessibleOrProxy& aRoot,
                                         int32_t aIndex);

  id CreateAXTextMarker();

  bool Next();

  bool Previous();

  // Return a range with the given type relative to this marker.
  GeckoTextMarkerRange Range(EWhichRange aRangeType);

  AccessibleOrProxy Leaf();

  bool IsValid() const { return !mContainer.IsNull(); };

  bool operator<(const GeckoTextMarker& aPoint) const;

  bool operator==(const GeckoTextMarker& aPoint) const {
    return mContainer == aPoint.mContainer && mOffset == aPoint.mOffset;
  }

  AccessibleOrProxy mContainer;
  int32_t mOffset;

  HyperTextAccessibleWrap* ContainerAsHyperTextWrap() const {
    return (!mContainer.IsNull() && mContainer.IsAccessible())
               ? static_cast<HyperTextAccessibleWrap*>(
                     mContainer.AsAccessible()->AsHyperText())
               : nullptr;
  }

 private:
  bool IsEditableRoot();
};

class GeckoTextMarkerRange final {
 public:
  GeckoTextMarkerRange(const GeckoTextMarker& aStart,
                       const GeckoTextMarker& aEnd)
      : mStart(aStart), mEnd(aEnd) {}

  GeckoTextMarkerRange() {}

  GeckoTextMarkerRange(AccessibleOrProxy aDoc,
                       AXTextMarkerRangeRef aTextMarkerRange);

  explicit GeckoTextMarkerRange(const AccessibleOrProxy& aAccessible);

  id CreateAXTextMarkerRange();

  bool IsValid() const {
    return !mStart.mContainer.IsNull() && !mEnd.mContainer.IsNull();
  };

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
  bool Crop(const AccessibleOrProxy& aContainer);

  GeckoTextMarker mStart;
  GeckoTextMarker mEnd;
};

}  // namespace a11y
}  // namespace mozilla

#endif
