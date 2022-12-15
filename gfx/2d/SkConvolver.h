/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011-2016 Google Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the gfx/skia/LICENSE file.

#ifndef MOZILLA_GFX_SKCONVOLVER_H_
#define MOZILLA_GFX_SKCONVOLVER_H_

#include "mozilla/Assertions.h"
#include <cfloat>
#include <cmath>
#include <vector>

namespace skia {

class SkBitmapFilter {
 public:
  explicit SkBitmapFilter(float width) : fWidth(width) {}
  virtual ~SkBitmapFilter() = default;

  float width() const { return fWidth; }
  virtual float evaluate(float x) const = 0;

 protected:
  float fWidth;
};

class SkBoxFilter final : public SkBitmapFilter {
 public:
  explicit SkBoxFilter(float width = 0.5f) : SkBitmapFilter(width) {}

  float evaluate(float x) const override {
    return (x >= -fWidth && x < fWidth) ? 1.0f : 0.0f;
  }
};

class SkLanczosFilter final : public SkBitmapFilter {
 public:
  explicit SkLanczosFilter(float width = 3.0f) : SkBitmapFilter(width) {}

  float evaluate(float x) const override {
    if (x <= -fWidth || x >= fWidth) {
      return 0.0f;  // Outside of the window.
    }
    if (x > -FLT_EPSILON && x < FLT_EPSILON) {
      return 1.0f;  // Special case the discontinuity at the origin.
    }
    float xpi = x * float(M_PI);
    return (sinf(xpi) / xpi) *                   // sinc(x)
           sinf(xpi / fWidth) / (xpi / fWidth);  // sinc(x/fWidth)
  }
};

// Represents a filter in one dimension. Each output pixel has one entry in this
// object for the filter values contributing to it. You build up the filter
// list by calling AddFilter for each output pixel (in order).
//
// We do 2-dimensional convolution by first convolving each row by one
// SkConvolutionFilter1D, then convolving each column by another one.
//
// Entries are stored in ConvolutionFixed point, shifted left by kShiftBits.
class SkConvolutionFilter1D {
 public:
  using ConvolutionFixed = short;

  // The number of bits that ConvolutionFixed point values are shifted by.
  enum { kShiftBits = 14 };

  SkConvolutionFilter1D();
  ~SkConvolutionFilter1D();

  // Convert between floating point and our ConvolutionFixed point
  // representation.
  static ConvolutionFixed ToFixed(float f) {
    return static_cast<ConvolutionFixed>(f * (1 << kShiftBits));
  }

  // Returns the maximum pixel span of a filter.
  int maxFilter() const { return fMaxFilter; }

  // Returns the number of filters in this filter. This is the dimension of the
  // output image.
  int numValues() const { return static_cast<int>(fFilters.size()); }

  void reserveAdditional(int filterCount, int filterValueCount) {
    fFilters.reserve(fFilters.size() + filterCount);
    fFilterValues.reserve(fFilterValues.size() + filterValueCount);
  }

  // Appends the given list of scaling values for generating a given output
  // pixel. |filterOffset| is the distance from the edge of the image to where
  // the scaling factors start. The scaling factors apply to the source pixels
  // starting from this position, and going for the next |filterLength| pixels.
  //
  // You will probably want to make sure your input is normalized (that is,
  // all entries in |filterValuesg| sub to one) to prevent affecting the overall
  // brighness of the image.
  //
  // The filterLength must be > 0.
  void AddFilter(int filterOffset, const ConvolutionFixed* filterValues,
                 int filterLength);

  // Retrieves a filter for the given |valueOffset|, a position in the output
  // image in the direction we're convolving. The offset and length of the
  // filter values are put into the corresponding out arguments (see AddFilter
  // above for what these mean), and a pointer to the first scaling factor is
  // returned. There will be |filterLength| values in this array.
  inline const ConvolutionFixed* FilterForValue(int valueOffset,
                                                int* filterOffset,
                                                int* filterLength) const {
    const FilterInstance& filter = fFilters[valueOffset];
    *filterOffset = filter.fOffset;
    *filterLength = filter.fTrimmedLength;
    if (filter.fTrimmedLength == 0) {
      return nullptr;
    }
    return &fFilterValues[filter.fDataLocation];
  }

  bool ComputeFilterValues(const SkBitmapFilter& aBitmapFilter,
                           int32_t aSrcSize, int32_t aDstSize);

 private:
  struct FilterInstance {
    // Offset within filterValues for this instance of the filter.
    int fDataLocation;

    // Distance from the left of the filter to the center. IN PIXELS
    int fOffset;

    // Number of values in this filter instance.
    int fTrimmedLength;

    // Filter length as specified. Note that this may be different from
    // 'trimmed_length' if leading/trailing zeros of the original floating
    // point form were clipped differently on each tail.
    int fLength;
  };

  // Stores the information for each filter added to this class.
  std::vector<FilterInstance> fFilters;

  // We store all the filter values in this flat list, indexed by
  // |FilterInstance.data_location| to avoid the mallocs required for storing
  // each one separately.
  std::vector<ConvolutionFixed> fFilterValues;

  // The maximum size of any filter we've added.
  int fMaxFilter;
};

void convolve_horizontally(const unsigned char* srcData,
                           const SkConvolutionFilter1D& filter,
                           unsigned char* outRow, bool hasAlpha);

void convolve_vertically(
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues,
    int filterLength, unsigned char* const* sourceDataRows, int pixelWidth,
    unsigned char* outRow, bool hasAlpha);

bool BGRAConvolve2D(const unsigned char* sourceData, int sourceByteRowStride,
                    bool sourceHasAlpha, const SkConvolutionFilter1D& filterX,
                    const SkConvolutionFilter1D& filterY,
                    int outputByteRowStride, unsigned char* output);

}  // namespace skia

#endif /* MOZILLA_GFX_SKCONVOLVER_H_ */
