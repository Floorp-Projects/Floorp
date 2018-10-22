/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsViewportInfo.h"
#include "mozilla/Assertions.h"
#include <algorithm>

using namespace mozilla;

void
nsViewportInfo::ConstrainViewportValues()
{
  // Constrain the min/max zoom as specified at:
  // dev.w3.org/csswg/css-device-adapt section 6.2
  mMaxZoom = std::max(mMinZoom, mMaxZoom);

  if (mDefaultZoom > mMaxZoom) {
    mDefaultZoomValid = false;
    mDefaultZoom = mMaxZoom;
  }
  if (mDefaultZoom < mMinZoom) {
    mDefaultZoomValid = false;
    mDefaultZoom = mMinZoom;
  }
}

static const float&
MinOrMax(const float& aA, const float& aB,
         const float& (*aMinOrMax)(const float&,
                                   const float&))
{
  MOZ_ASSERT(aA != nsViewportInfo::ExtendToZoom &&
             aA != nsViewportInfo::DeviceSize &&
             aB != nsViewportInfo::ExtendToZoom &&
             aB != nsViewportInfo::DeviceSize);
  if (aA == nsViewportInfo::Auto) {
    return aB;
  }
  if (aB == nsViewportInfo::Auto) {
    return aA;
  }
  return aMinOrMax(aA, aB);
}

// static
const float&
nsViewportInfo::Min(const float& aA, const float& aB)
{
  return MinOrMax(aA, aB, std::min);
}

//static
const float&
nsViewportInfo::Max(const float& aA, const float& aB)
{
  return MinOrMax(aA, aB, std::max);
}

