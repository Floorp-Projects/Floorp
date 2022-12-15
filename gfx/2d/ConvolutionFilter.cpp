/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ConvolutionFilter.h"
#include "HelpersSkia.h"
#include "SkConvolver.h"
#include "skia/include/core/SkBitmap.h"

namespace mozilla::gfx {

ConvolutionFilter::ConvolutionFilter()
    : mFilter(MakeUnique<skia::SkConvolutionFilter1D>()) {}

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
  skia::convolve_horizontally(aSrc, *mFilter, aDst, aHasAlpha);
}

void ConvolutionFilter::ConvolveVertically(uint8_t* const* aSrc, uint8_t* aDst,
                                           int32_t aRowIndex, int32_t aRowSize,
                                           bool aHasAlpha) {
  MOZ_ASSERT(aRowIndex < mFilter->numValues());

  int32_t filterOffset;
  int32_t filterLength;
  auto filterValues =
      mFilter->FilterForValue(aRowIndex, &filterOffset, &filterLength);
  skia::convolve_vertically(filterValues, filterLength, aSrc, aRowSize, aDst,
                            aHasAlpha);
}

bool ConvolutionFilter::ComputeResizeFilter(ResizeMethod aResizeMethod,
                                            int32_t aSrcSize,
                                            int32_t aDstSize) {
  if (aSrcSize < 0 || aDstSize < 0) {
    return false;
  }

  switch (aResizeMethod) {
    case ResizeMethod::BOX:
      return mFilter->ComputeFilterValues(skia::SkBoxFilter(), aSrcSize,
                                          aDstSize);
    case ResizeMethod::LANCZOS3:
      return mFilter->ComputeFilterValues(skia::SkLanczosFilter(), aSrcSize,
                                          aDstSize);
    default:
      return false;
  }
}

bool Scale(uint8_t* srcData, int32_t srcWidth, int32_t srcHeight,
           int32_t srcStride, uint8_t* dstData, int32_t dstWidth,
           int32_t dstHeight, int32_t dstStride, SurfaceFormat format) {
  if (!srcData || !dstData || srcWidth < 1 || srcHeight < 1 || dstWidth < 1 ||
      dstHeight < 1) {
    return false;
  }

  SkPixmap srcPixmap(MakeSkiaImageInfo(IntSize(srcWidth, srcHeight), format),
                     srcData, srcStride);

  // Rescaler is compatible with N32 only. Convert to N32 if needed.
  SkBitmap tmpBitmap;
  if (srcPixmap.colorType() != kN32_SkColorType) {
    if (!tmpBitmap.tryAllocPixels(
            SkImageInfo::MakeN32Premul(srcWidth, srcHeight)) ||
        !tmpBitmap.writePixels(srcPixmap) ||
        !tmpBitmap.peekPixels(&srcPixmap)) {
      return false;
    }
  }

  ConvolutionFilter xFilter;
  ConvolutionFilter yFilter;
  ConvolutionFilter* xOrYFilter = &xFilter;
  bool isSquare = srcWidth == srcHeight && dstWidth == dstHeight;
  if (!xFilter.ComputeResizeFilter(ConvolutionFilter::ResizeMethod::LANCZOS3,
                                   srcWidth, dstWidth)) {
    return false;
  }
  if (!isSquare) {
    if (!yFilter.ComputeResizeFilter(ConvolutionFilter::ResizeMethod::LANCZOS3,
                                     srcHeight, dstHeight)) {
      return false;
    }
    xOrYFilter = &yFilter;
  }

  return skia::BGRAConvolve2D(
      static_cast<const uint8_t*>(srcPixmap.addr()), int(srcPixmap.rowBytes()),
      !srcPixmap.isOpaque(), xFilter.GetSkiaFilter(),
      xOrYFilter->GetSkiaFilter(), int(dstStride), dstData);
}

}  // namespace mozilla::gfx
