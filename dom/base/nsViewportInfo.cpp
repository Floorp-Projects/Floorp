/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsViewportInfo.h"
#include "mozilla/Assertions.h"
#include <algorithm>

using namespace mozilla;

void nsViewportInfo::ConstrainViewportValues() {
  // Non-positive zoom factors can produce NaN or negative viewport sizes,
  // so we better be sure our constraints will produce positive zoom factors.
  MOZ_ASSERT(mMinZoom > CSSToScreenScale(0.0f), "zoom factor must be positive");
  MOZ_ASSERT(mMaxZoom > CSSToScreenScale(0.0f), "zoom factor must be positive");

  if (mDefaultZoom > mMaxZoom) {
    mDefaultZoomValid = false;
    mDefaultZoom = mMaxZoom;
  }
  if (mDefaultZoom < mMinZoom) {
    mDefaultZoomValid = false;
    mDefaultZoom = mMinZoom;
  }
}

static const float& MinOrMax(const float& aA, const float& aB,
                             const float& (*aMinOrMax)(const float&,
                                                       const float&)) {
  MOZ_ASSERT(
      aA != nsViewportInfo::ExtendToZoom && aA != nsViewportInfo::DeviceSize &&
      aB != nsViewportInfo::ExtendToZoom && aB != nsViewportInfo::DeviceSize);
  if (aA == nsViewportInfo::Auto) {
    return aB;
  }
  if (aB == nsViewportInfo::Auto) {
    return aA;
  }
  return aMinOrMax(aA, aB);
}

// static
const float& nsViewportInfo::Min(const float& aA, const float& aB) {
  return MinOrMax(aA, aB, std::min);
}

// static
const float& nsViewportInfo::Max(const float& aA, const float& aB) {
  return MinOrMax(aA, aB, std::max);
}
