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

class GeckoTextMarker final {
 public:
  GeckoTextMarker(const AccessibleOrProxy& aContainer, int32_t aOffset)
      : mContainer(aContainer), mOffset(aOffset) {}

  GeckoTextMarker(const GeckoTextMarker& aPoint)
      : mContainer(aPoint.mContainer), mOffset(aPoint.mOffset) {}

  GeckoTextMarker(AccessibleOrProxy aDoc, AXTextMarkerRef aTextMarker);

  GeckoTextMarker() : mContainer(nullptr), mOffset(0) {}

  id CreateAXTextMarker();

  bool operator<(const GeckoTextMarker& aPoint) const;

  AccessibleOrProxy mContainer;
  int32_t mOffset;
};

class GeckoTextMarkerRange final {
 public:
  GeckoTextMarkerRange(const GeckoTextMarker& aStart,
                       const GeckoTextMarker& aEnd)
      : mStart(aStart), mEnd(aEnd) {}

  GeckoTextMarkerRange(AccessibleOrProxy aDoc,
                       AXTextMarkerRangeRef aTextMarkerRange);

  id CreateAXTextMarkerRange();

  bool IsValid() const {
    return !mStart.mContainer.IsNull() && !mEnd.mContainer.IsNull();
  };

  GeckoTextMarker mStart;
  GeckoTextMarker mEnd;
};

}  // namespace a11y
}  // namespace mozilla

#endif
