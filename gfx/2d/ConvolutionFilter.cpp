/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConvolutionFilter.h"
#include "skia/src/core/SkBitmapFilter.h"
#include "skia/src/core/SkConvolver.h"
#include "skia/src/core/SkOpts.h"
#include <algorithm>
#include <cmath>
#include "mozilla/Vector.h"

namespace mozilla::gfx {

ConvolutionFilter::ConvolutionFilter()
    : mFilter(MakeUnique<SkConvolutionFilter1D>()) {}

ConvolutionFilter::~ConvolutionFilter() = default;

int32_t ConvolutionFilter::MaxFilter() const { return mFilter->maxFilter(); }

int32_t ConvolutionFilter::NumValues() const { return mFilter->numValues(); }

bool ConvolutionFilter::GetFilterOffsetAndLength(int32_t aRowIndex,
                                                 int32_t* aResultOffset,
                                                 int32_t* aResultLength) {
  if (aRowIndex >= mFilter->numValues()) {
    return false;
  }
  mFilter->FilterForValue(aRowIndex, aResultOffset, aResultLength);
  return true;
}

void ConvolutionFilter::ConvolveHorizontally(const uint8_t* aSrc, uint8_t* aDst,
                                             bool aHasAlpha) {
  SkOpts::convolve_horizontally(aSrc, *mFilter, aDst, aHasAlpha);
}

void ConvolutionFilter::ConvolveVertically(uint8_t* const* aSrc, uint8_t* aDst,
                                           int32_t aRowIndex, int32_t aRowSize,
                                           bool aHasAlpha) {
  MOZ_ASSERT(aRowIndex < mFilter->numValues());

  int32_t filterOffset;
  int32_t filterLength;
  auto filterValues =
      mFilter->FilterForValue(aRowIndex, &filterOffset, &filterLength);
  SkOpts::convolve_vertically(filterValues, filterLength, aSrc, aRowSize, aDst,
                              aHasAlpha);
}

/* ConvolutionFilter::ComputeResizeFactor is derived from Skia's
 * SkBitmapScaler/SkResizeFilter::computeFactors. It is governed by Skia's
 * BSD-style license (see gfx/skia/LICENSE) and the following copyright:
 * Copyright (c) 2015 Google Inc.
 */
bool ConvolutionFilter::ComputeResizeFilter(ResizeMethod aResizeMethod,
                                            int32_t aSrcSize,
                                            int32_t aDstSize) {
  typedef SkConvolutionFilter1D::ConvolutionFixed Fixed;

  if (aSrcSize < 0 || aDstSize < 0) {
    return false;
  }

  UniquePtr<SkBitmapFilter> bitmapFilter;
  switch (aResizeMethod) {
    case ResizeMethod::BOX:
      bitmapFilter = MakeUnique<SkBoxFilter>();
      break;
    case ResizeMethod::TRIANGLE:
      bitmapFilter = MakeUnique<SkTriangleFilter>();
      break;
    case ResizeMethod::LANCZOS3:
      bitmapFilter = MakeUnique<SkLanczosFilter>();
      break;
    case ResizeMethod::HAMMING:
      bitmapFilter = MakeUnique<SkHammingFilter>();
      break;
    case ResizeMethod::MITCHELL:
      bitmapFilter = MakeUnique<SkMitchellFilter>();
      break;
    default:
      return false;
  }

  // When we're doing a magnification, the scale will be larger than one. This
  // means the destination pixels are much smaller than the source pixels, and
  // that the range covered by the filter won't necessarily cover any source
  // pixel boundaries. Therefore, we use these clamped values (max of 1) for
  // some computations.
  float scale = float(aDstSize) / float(aSrcSize);
  float clampedScale = std::min(1.0f, scale);
  // This is how many source pixels from the center we need to count
  // to support the filtering function.
  float srcSupport = bitmapFilter->width() / clampedScale;
  float invScale = 1.0f / scale;

  Vector<float, 64> filterValues;
  Vector<Fixed, 64> fixedFilterValues;

  // Loop over all pixels in the output range. We will generate one set of
  // filter values for each one. Those values will tell us how to blend the
  // source pixels to compute the destination pixel.

  // This value is computed based on how SkTDArray::resizeStorageToAtLeast works
  // in order to ensure that it does not overflow or assert. That functions
  // computes
  //   n+4 + (n+4)/4
  // and we want to to fit in a 32 bit signed int. Equating that to 2^31-1 and
  // solving n gives n = (2^31-6)*4/5 = 1717986913.6
  const int32_t maxToPassToReserveAdditional = 1717986913;

  int32_t filterValueCount = int32_t(ceil(aDstSize * srcSupport * 2));
  if (aDstSize > maxToPassToReserveAdditional || filterValueCount < 0 ||
      filterValueCount > maxToPassToReserveAdditional) {
    return false;
  }
  mFilter->reserveAdditional(aDstSize, filterValueCount);
  for (int32_t destI = 0; destI < aDstSize; destI++) {
    // This is the pixel in the source directly under the pixel in the dest.
    // Note that we base computations on the "center" of the pixels. To see
    // why, observe that the destination pixel at coordinates (0, 0) in a 5.0x
    // downscale should "cover" the pixels around the pixel with *its center*
    // at coordinates (2.5, 2.5) in the source, not those around (0, 0).
    // Hence we need to scale coordinates (0.5, 0.5), not (0, 0).
    float srcPixel = (static_cast<float>(destI) + 0.5f) * invScale;

    // Compute the (inclusive) range of source pixels the filter covers.
    float srcBegin = std::max(0.0f, floorf(srcPixel - srcSupport));
    float srcEnd = std::min(aSrcSize - 1.0f, ceilf(srcPixel + srcSupport));

    // Compute the unnormalized filter value at each location of the source
    // it covers.

    // Sum of the filter values for normalizing.
    // Distance from the center of the filter, this is the filter coordinate
    // in source space. We also need to consider the center of the pixel
    // when comparing distance against 'srcPixel'. In the 5x downscale
    // example used above the distance from the center of the filter to
    // the pixel with coordinates (2, 2) should be 0, because its center
    // is at (2.5, 2.5).
    float destFilterDist = (srcBegin + 0.5f - srcPixel) * clampedScale;
    int32_t filterCount = int32_t(srcEnd - srcBegin) + 1;
    if (filterCount <= 0 || !filterValues.resize(filterCount) ||
        !fixedFilterValues.resize(filterCount)) {
      return false;
    }
    float filterSum = bitmapFilter->evaluate_n(
        destFilterDist, clampedScale, filterCount, filterValues.begin());

    // The filter must be normalized so that we don't affect the brightness of
    // the image. Convert to normalized fixed point.
    Fixed fixedSum = 0;
    float invFilterSum = 1.0f / filterSum;
    for (int32_t fixedI = 0; fixedI < filterCount; fixedI++) {
      Fixed curFixed = SkConvolutionFilter1D::FloatToFixed(
          filterValues[fixedI] * invFilterSum);
      fixedSum += curFixed;
      fixedFilterValues[fixedI] = curFixed;
    }

    // The conversion to fixed point will leave some rounding errors, which
    // we add back in to avoid affecting the brightness of the image. We
    // arbitrarily add this to the center of the filter array (this won't always
    // be the center of the filter function since it could get clipped on the
    // edges, but it doesn't matter enough to worry about that case).
    Fixed leftovers = SkConvolutionFilter1D::FloatToFixed(1) - fixedSum;
    fixedFilterValues[filterCount / 2] += leftovers;

    mFilter->AddFilter(int32_t(srcBegin), fixedFilterValues.begin(),
                       filterCount);
  }

  return mFilter->maxFilter() > 0 && mFilter->numValues() == aDstSize;
}

}  // namespace mozilla::gfx
