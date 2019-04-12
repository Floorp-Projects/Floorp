/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_CONVOLUTION_FILTER_H_
#define MOZILLA_GFX_CONVOLUTION_FILTER_H_

#include "mozilla/UniquePtr.h"

class SkConvolutionFilter1D;

namespace mozilla {
namespace gfx {

class ConvolutionFilter final {
 public:
  ConvolutionFilter();
  ~ConvolutionFilter();

  int32_t MaxFilter() const;
  int32_t NumValues() const;

  bool GetFilterOffsetAndLength(int32_t aRowIndex, int32_t* aResultOffset,
                                int32_t* aResultLength);

  void ConvolveHorizontally(const uint8_t* aSrc, uint8_t* aDst, bool aHasAlpha);
  void ConvolveVertically(uint8_t* const* aSrc, uint8_t* aDst,
                          int32_t aRowIndex, int32_t aRowSize, bool aHasAlpha);

  enum class ResizeMethod { BOX, TRIANGLE, LANCZOS3, HAMMING, MITCHELL };

  bool ComputeResizeFilter(ResizeMethod aResizeMethod, int32_t aSrcSize,
                           int32_t aDstSize);

  static inline size_t PadBytesForSIMD(size_t aBytes) {
    return (aBytes + 31) & ~31;
  }

 private:
  UniquePtr<SkConvolutionFilter1D> mFilter;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_CONVOLUTION_FILTER_H_ */
