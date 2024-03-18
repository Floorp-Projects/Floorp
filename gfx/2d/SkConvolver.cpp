/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2011-2016 Google Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the gfx/skia/LICENSE file.

#include "SkConvolver.h"

#ifdef USE_SSE2
#  include "mozilla/SSE.h"
#endif

#ifdef USE_NEON
#  include "mozilla/arm.h"
#endif

namespace skia {

// Converts the argument to an 8-bit unsigned value by clamping to the range
// 0-255.
static inline unsigned char ClampTo8(int a) {
  if (static_cast<unsigned>(a) < 256) {
    return a;  // Avoid the extra check in the common case.
  }
  if (a < 0) {
    return 0;
  }
  return 255;
}

// Convolves horizontally along a single row. The row data is given in
// |srcData| and continues for the numValues() of the filter.
template <bool hasAlpha>
void ConvolveHorizontally(const unsigned char* srcData,
                          const SkConvolutionFilter1D& filter,
                          unsigned char* outRow) {
  // Loop over each pixel on this row in the output image.
  int numValues = filter.numValues();
  for (int outX = 0; outX < numValues; outX++) {
    // Get the filter that determines the current output pixel.
    int filterOffset, filterLength;
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues =
        filter.FilterForValue(outX, &filterOffset, &filterLength);

    // Compute the first pixel in this row that the filter affects. It will
    // touch |filterLength| pixels (4 bytes each) after this.
    const unsigned char* rowToFilter = &srcData[filterOffset * 4];

    // Apply the filter to the row to get the destination pixel in |accum|.
    int accum[4] = {0};
    for (int filterX = 0; filterX < filterLength; filterX++) {
      SkConvolutionFilter1D::ConvolutionFixed curFilter = filterValues[filterX];
      accum[0] += curFilter * rowToFilter[filterX * 4 + 0];
      accum[1] += curFilter * rowToFilter[filterX * 4 + 1];
      accum[2] += curFilter * rowToFilter[filterX * 4 + 2];
      if (hasAlpha) {
        accum[3] += curFilter * rowToFilter[filterX * 4 + 3];
      }
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of fractional part.
    accum[0] >>= SkConvolutionFilter1D::kShiftBits;
    accum[1] >>= SkConvolutionFilter1D::kShiftBits;
    accum[2] >>= SkConvolutionFilter1D::kShiftBits;

    if (hasAlpha) {
      accum[3] >>= SkConvolutionFilter1D::kShiftBits;
    }

    // Store the new pixel.
    outRow[outX * 4 + 0] = ClampTo8(accum[0]);
    outRow[outX * 4 + 1] = ClampTo8(accum[1]);
    outRow[outX * 4 + 2] = ClampTo8(accum[2]);
    if (hasAlpha) {
      outRow[outX * 4 + 3] = ClampTo8(accum[3]);
    }
  }
}

// Does vertical convolution to produce one output row. The filter values and
// length are given in the first two parameters. These are applied to each
// of the rows pointed to in the |sourceDataRows| array, with each row
// being |pixelWidth| wide.
//
// The output must have room for |pixelWidth * 4| bytes.
template <bool hasAlpha>
void ConvolveVertically(
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues,
    int filterLength, unsigned char* const* sourceDataRows, int pixelWidth,
    unsigned char* outRow) {
  // We go through each column in the output and do a vertical convolution,
  // generating one output pixel each time.
  for (int outX = 0; outX < pixelWidth; outX++) {
    // Compute the number of bytes over in each row that the current column
    // we're convolving starts at. The pixel will cover the next 4 bytes.
    int byteOffset = outX * 4;

    // Apply the filter to one column of pixels.
    int accum[4] = {0};
    for (int filterY = 0; filterY < filterLength; filterY++) {
      SkConvolutionFilter1D::ConvolutionFixed curFilter = filterValues[filterY];
      accum[0] += curFilter * sourceDataRows[filterY][byteOffset + 0];
      accum[1] += curFilter * sourceDataRows[filterY][byteOffset + 1];
      accum[2] += curFilter * sourceDataRows[filterY][byteOffset + 2];
      if (hasAlpha) {
        accum[3] += curFilter * sourceDataRows[filterY][byteOffset + 3];
      }
    }

    // Bring this value back in range. All of the filter scaling factors
    // are in fixed point with kShiftBits bits of precision.
    accum[0] >>= SkConvolutionFilter1D::kShiftBits;
    accum[1] >>= SkConvolutionFilter1D::kShiftBits;
    accum[2] >>= SkConvolutionFilter1D::kShiftBits;
    if (hasAlpha) {
      accum[3] >>= SkConvolutionFilter1D::kShiftBits;
    }

    // Store the new pixel.
    outRow[byteOffset + 0] = ClampTo8(accum[0]);
    outRow[byteOffset + 1] = ClampTo8(accum[1]);
    outRow[byteOffset + 2] = ClampTo8(accum[2]);

    if (hasAlpha) {
      unsigned char alpha = ClampTo8(accum[3]);

      // Make sure the alpha channel doesn't come out smaller than any of the
      // color channels. We use premultipled alpha channels, so this should
      // never happen, but rounding errors will cause this from time to time.
      // These "impossible" colors will cause overflows (and hence random pixel
      // values) when the resulting bitmap is drawn to the screen.
      //
      // We only need to do this when generating the final output row (here).
      int maxColorChannel =
          std::max(outRow[byteOffset + 0],
                   std::max(outRow[byteOffset + 1], outRow[byteOffset + 2]));
      if (alpha < maxColorChannel) {
        outRow[byteOffset + 3] = maxColorChannel;
      } else {
        outRow[byteOffset + 3] = alpha;
      }
    } else {
      // No alpha channel, the image is opaque.
      outRow[byteOffset + 3] = 0xff;
    }
  }
}

#ifdef USE_SSE2
void convolve_vertically_avx2(const int16_t* filter, int filterLen,
                              uint8_t* const* srcRows, int width, uint8_t* out,
                              bool hasAlpha);
void convolve_horizontally_sse2(const unsigned char* srcData,
                                const SkConvolutionFilter1D& filter,
                                unsigned char* outRow, bool hasAlpha);
void convolve_vertically_sse2(const int16_t* filter, int filterLen,
                              uint8_t* const* srcRows, int width, uint8_t* out,
                              bool hasAlpha);
#elif defined(USE_NEON)
void convolve_horizontally_neon(const unsigned char* srcData,
                                const SkConvolutionFilter1D& filter,
                                unsigned char* outRow, bool hasAlpha);
void convolve_vertically_neon(const int16_t* filter, int filterLen,
                              uint8_t* const* srcRows, int width, uint8_t* out,
                              bool hasAlpha);
#endif

void convolve_horizontally(const unsigned char* srcData,
                           const SkConvolutionFilter1D& filter,
                           unsigned char* outRow, bool hasAlpha) {
#ifdef USE_SSE2
  if (mozilla::supports_sse2()) {
    convolve_horizontally_sse2(srcData, filter, outRow, hasAlpha);
    return;
  }
#elif defined(USE_NEON)
  if (mozilla::supports_neon()) {
    convolve_horizontally_neon(srcData, filter, outRow, hasAlpha);
    return;
  }
#endif
  if (hasAlpha) {
    ConvolveHorizontally<true>(srcData, filter, outRow);
  } else {
    ConvolveHorizontally<false>(srcData, filter, outRow);
  }
}

void convolve_vertically(
    const SkConvolutionFilter1D::ConvolutionFixed* filterValues,
    int filterLength, unsigned char* const* sourceDataRows, int pixelWidth,
    unsigned char* outRow, bool hasAlpha) {
#ifdef USE_SSE2
  if (mozilla::supports_avx2()) {
    convolve_vertically_avx2(filterValues, filterLength, sourceDataRows,
                             pixelWidth, outRow, hasAlpha);
    return;
  }
  if (mozilla::supports_sse2()) {
    convolve_vertically_sse2(filterValues, filterLength, sourceDataRows,
                             pixelWidth, outRow, hasAlpha);
    return;
  }
#elif defined(USE_NEON)
  if (mozilla::supports_neon()) {
    convolve_vertically_neon(filterValues, filterLength, sourceDataRows,
                             pixelWidth, outRow, hasAlpha);
    return;
  }
#endif
  if (hasAlpha) {
    ConvolveVertically<true>(filterValues, filterLength, sourceDataRows,
                             pixelWidth, outRow);
  } else {
    ConvolveVertically<false>(filterValues, filterLength, sourceDataRows,
                              pixelWidth, outRow);
  }
}

// Stores a list of rows in a circular buffer. The usage is you write into it
// by calling AdvanceRow. It will keep track of which row in the buffer it
// should use next, and the total number of rows added.
class CircularRowBuffer {
 public:
  // The number of pixels in each row is given in |sourceRowPixelWidth|.
  // The maximum number of rows needed in the buffer is |maxYFilterSize|
  // (we only need to store enough rows for the biggest filter).
  //
  // We use the |firstInputRow| to compute the coordinates of all of the
  // following rows returned by Advance().
  CircularRowBuffer(int destRowPixelWidth, int maxYFilterSize,
                    int firstInputRow)
      : fRowByteWidth(destRowPixelWidth * 4),
        fNumRows(maxYFilterSize),
        fNextRow(0),
        fNextRowCoordinate(firstInputRow) {}

  bool AllocBuffer() {
    return fBuffer.resize(fRowByteWidth * fNumRows) &&
           fRowAddresses.resize(fNumRows);
  }

  // Moves to the next row in the buffer, returning a pointer to the beginning
  // of it.
  unsigned char* advanceRow() {
    unsigned char* row = &fBuffer[fNextRow * fRowByteWidth];
    fNextRowCoordinate++;

    // Set the pointer to the next row to use, wrapping around if necessary.
    fNextRow++;
    if (fNextRow == fNumRows) {
      fNextRow = 0;
    }
    return row;
  }

  // Returns a pointer to an "unrolled" array of rows. These rows will start
  // at the y coordinate placed into |*firstRowIndex| and will continue in
  // order for the maximum number of rows in this circular buffer.
  //
  // The |firstRowIndex_| may be negative. This means the circular buffer
  // starts before the top of the image (it hasn't been filled yet).
  unsigned char* const* GetRowAddresses(int* firstRowIndex) {
    // Example for a 4-element circular buffer holding coords 6-9.
    //   Row 0   Coord 8
    //   Row 1   Coord 9
    //   Row 2   Coord 6  <- fNextRow = 2, fNextRowCoordinate = 10.
    //   Row 3   Coord 7
    //
    // The "next" row is also the first (lowest) coordinate. This computation
    // may yield a negative value, but that's OK, the math will work out
    // since the user of this buffer will compute the offset relative
    // to the firstRowIndex and the negative rows will never be used.
    *firstRowIndex = fNextRowCoordinate - fNumRows;

    int curRow = fNextRow;
    for (int i = 0; i < fNumRows; i++) {
      fRowAddresses[i] = &fBuffer[curRow * fRowByteWidth];

      // Advance to the next row, wrapping if necessary.
      curRow++;
      if (curRow == fNumRows) {
        curRow = 0;
      }
    }
    return &fRowAddresses[0];
  }

 private:
  // The buffer storing the rows. They are packed, each one fRowByteWidth.
  mozilla::Vector<unsigned char> fBuffer;

  // Number of bytes per row in the |buffer|.
  int fRowByteWidth;

  // The number of rows available in the buffer.
  int fNumRows;

  // The next row index we should write into. This wraps around as the
  // circular buffer is used.
  int fNextRow;

  // The y coordinate of the |fNextRow|. This is incremented each time a
  // new row is appended and does not wrap.
  int fNextRowCoordinate;

  // Buffer used by GetRowAddresses().
  mozilla::Vector<unsigned char*> fRowAddresses;
};

SkConvolutionFilter1D::SkConvolutionFilter1D() : fMaxFilter(0) {}

SkConvolutionFilter1D::~SkConvolutionFilter1D() = default;

bool SkConvolutionFilter1D::AddFilter(int filterOffset,
                                      const ConvolutionFixed* filterValues,
                                      int filterLength) {
  // It is common for leading/trailing filter values to be zeros. In such
  // cases it is beneficial to only store the central factors.
  // For a scaling to 1/4th in each dimension using a Lanczos-2 filter on
  // a 1080p image this optimization gives a ~10% speed improvement.
  int filterSize = filterLength;
  int firstNonZero = 0;
  while (firstNonZero < filterLength && filterValues[firstNonZero] == 0) {
    firstNonZero++;
  }

  if (firstNonZero < filterLength) {
    // Here we have at least one non-zero factor.
    int lastNonZero = filterLength - 1;
    while (lastNonZero >= 0 && filterValues[lastNonZero] == 0) {
      lastNonZero--;
    }

    filterOffset += firstNonZero;
    filterLength = lastNonZero + 1 - firstNonZero;
    MOZ_ASSERT(filterLength > 0);

    if (!fFilterValues.append(&filterValues[firstNonZero], filterLength)) {
      return false;
    }
  } else {
    // Here all the factors were zeroes.
    filterLength = 0;
  }

  FilterInstance instance = {
      // We pushed filterLength elements onto fFilterValues
      int(fFilterValues.length()) - filterLength, filterOffset, filterLength,
      filterSize};
  if (!fFilters.append(instance)) {
    if (filterLength > 0) {
      fFilterValues.shrinkBy(filterLength);
    }
    return false;
  }

  fMaxFilter = std::max(fMaxFilter, filterLength);
  return true;
}

bool SkConvolutionFilter1D::ComputeFilterValues(
    const SkBitmapFilter& aBitmapFilter, int32_t aSrcSize, int32_t aDstSize) {
  // When we're doing a magnification, the scale will be larger than one. This
  // means the destination pixels are much smaller than the source pixels, and
  // that the range covered by the filter won't necessarily cover any source
  // pixel boundaries. Therefore, we use these clamped values (max of 1) for
  // some computations.
  float scale = float(aDstSize) / float(aSrcSize);
  float clampedScale = std::min(1.0f, scale);
  // This is how many source pixels from the center we need to count
  // to support the filtering function.
  float srcSupport = aBitmapFilter.width() / clampedScale;
  float invScale = 1.0f / scale;

  mozilla::Vector<float, 64> filterValues;
  mozilla::Vector<ConvolutionFixed, 64> fixedFilterValues;

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

  int32_t filterValueCount = int32_t(ceilf(aDstSize * srcSupport * 2));
  if (aDstSize > maxToPassToReserveAdditional || filterValueCount < 0 ||
      filterValueCount > maxToPassToReserveAdditional ||
      !reserveAdditional(aDstSize, filterValueCount)) {
    return false;
  }
  size_t oldFiltersLength = fFilters.length();
  size_t oldFilterValuesLength = fFilterValues.length();
  int oldMaxFilter = fMaxFilter;
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
    int32_t filterCount = int32_t(srcEnd - srcBegin) + 1;
    if (filterCount <= 0 || !filterValues.resize(filterCount) ||
        !fixedFilterValues.resize(filterCount)) {
      return false;
    }

    float destFilterDist = (srcBegin + 0.5f - srcPixel) * clampedScale;
    float filterSum = 0.0f;
    for (int32_t index = 0; index < filterCount; index++) {
      float filterValue = aBitmapFilter.evaluate(destFilterDist);
      filterValues[index] = filterValue;
      filterSum += filterValue;
      destFilterDist += clampedScale;
    }

    // The filter must be normalized so that we don't affect the brightness of
    // the image. Convert to normalized fixed point.
    ConvolutionFixed fixedSum = 0;
    float invFilterSum = 1.0f / filterSum;
    for (int32_t fixedI = 0; fixedI < filterCount; fixedI++) {
      ConvolutionFixed curFixed = ToFixed(filterValues[fixedI] * invFilterSum);
      fixedSum += curFixed;
      fixedFilterValues[fixedI] = curFixed;
    }

    // The conversion to fixed point will leave some rounding errors, which
    // we add back in to avoid affecting the brightness of the image. We
    // arbitrarily add this to the center of the filter array (this won't always
    // be the center of the filter function since it could get clipped on the
    // edges, but it doesn't matter enough to worry about that case).
    ConvolutionFixed leftovers = ToFixed(1) - fixedSum;
    fixedFilterValues[filterCount / 2] += leftovers;

    if (!AddFilter(int32_t(srcBegin), fixedFilterValues.begin(), filterCount)) {
      fFilters.shrinkTo(oldFiltersLength);
      fFilterValues.shrinkTo(oldFilterValuesLength);
      fMaxFilter = oldMaxFilter;
      return false;
    }
  }

  return maxFilter() > 0 && numValues() == aDstSize;
}

// Does a two-dimensional convolution on the given source image.
//
// It is assumed the source pixel offsets referenced in the input filters
// reference only valid pixels, so the source image size is not required. Each
// row of the source image starts |sourceByteRowStride| after the previous
// one (this allows you to have rows with some padding at the end).
//
// The result will be put into the given output buffer. The destination image
// size will be xfilter.numValues() * yfilter.numValues() pixels. It will be
// in rows of exactly xfilter.numValues() * 4 bytes.
//
// |sourceHasAlpha| is a hint that allows us to avoid doing computations on
// the alpha channel if the image is opaque. If you don't know, set this to
// true and it will work properly, but setting this to false will be a few
// percent faster if you know the image is opaque.
//
// The layout in memory is assumed to be 4-bytes per pixel in B-G-R-A order
// (this is ARGB when loaded into 32-bit words on a little-endian machine).
/**
 *  Returns false if it was unable to perform the convolution/rescale. in which
 * case the output buffer is assumed to be undefined.
 */
bool BGRAConvolve2D(const unsigned char* sourceData, int sourceByteRowStride,
                    bool sourceHasAlpha, const SkConvolutionFilter1D& filterX,
                    const SkConvolutionFilter1D& filterY,
                    int outputByteRowStride, unsigned char* output) {
  int maxYFilterSize = filterY.maxFilter();

  // The next row in the input that we will generate a horizontally
  // convolved row for. If the filter doesn't start at the beginning of the
  // image (this is the case when we are only resizing a subset), then we
  // don't want to generate any output rows before that. Compute the starting
  // row for convolution as the first pixel for the first vertical filter.
  int filterOffset = 0, filterLength = 0;
  const SkConvolutionFilter1D::ConvolutionFixed* filterValues =
      filterY.FilterForValue(0, &filterOffset, &filterLength);
  int nextXRow = filterOffset;

  // We loop over each row in the input doing a horizontal convolution. This
  // will result in a horizontally convolved image. We write the results into
  // a circular buffer of convolved rows and do vertical convolution as rows
  // are available. This prevents us from having to store the entire
  // intermediate image and helps cache coherency.
  // We will need four extra rows to allow horizontal convolution could be done
  // simultaneously. We also pad each row in row buffer to be aligned-up to
  // 32 bytes.
  // TODO(jiesun): We do not use aligned load from row buffer in vertical
  // convolution pass yet. Somehow Windows does not like it.
  int rowBufferWidth = (filterX.numValues() + 31) & ~0x1F;
  int rowBufferHeight = maxYFilterSize;

  // check for too-big allocation requests : crbug.com/528628
  {
    int64_t size = int64_t(rowBufferWidth) * int64_t(rowBufferHeight);
    // need some limit, to avoid over-committing success from malloc, but then
    // crashing when we try to actually use the memory.
    // 100meg seems big enough to allow "normal" zoom factors and image sizes
    // through while avoiding the crash seen by the bug (crbug.com/528628)
    if (size > 100 * 1024 * 1024) {
      //            printf_stderr("BGRAConvolve2D: tmp allocation [%lld] too
      //            big\n", size);
      return false;
    }
  }

  CircularRowBuffer rowBuffer(rowBufferWidth, rowBufferHeight, filterOffset);
  if (!rowBuffer.AllocBuffer()) {
    return false;
  }

  // Loop over every possible output row, processing just enough horizontal
  // convolutions to run each subsequent vertical convolution.
  MOZ_ASSERT(outputByteRowStride >= filterX.numValues() * 4);
  int numOutputRows = filterY.numValues();

  // We need to check which is the last line to convolve before we advance 4
  // lines in one iteration.
  int lastFilterOffset, lastFilterLength;
  filterY.FilterForValue(numOutputRows - 1, &lastFilterOffset,
                         &lastFilterLength);

  for (int outY = 0; outY < numOutputRows; outY++) {
    filterValues = filterY.FilterForValue(outY, &filterOffset, &filterLength);

    // Generate output rows until we have enough to run the current filter.
    while (nextXRow < filterOffset + filterLength) {
      convolve_horizontally(
          &sourceData[(uint64_t)nextXRow * sourceByteRowStride], filterX,
          rowBuffer.advanceRow(), sourceHasAlpha);
      nextXRow++;
    }

    // Compute where in the output image this row of final data will go.
    unsigned char* curOutputRow = &output[(uint64_t)outY * outputByteRowStride];

    // Get the list of rows that the circular buffer has, in order.
    int firstRowInCircularBuffer;
    unsigned char* const* rowsToConvolve =
        rowBuffer.GetRowAddresses(&firstRowInCircularBuffer);

    // Now compute the start of the subset of those rows that the filter needs.
    unsigned char* const* firstRowForFilter =
        &rowsToConvolve[filterOffset - firstRowInCircularBuffer];

    convolve_vertically(filterValues, filterLength, firstRowForFilter,
                        filterX.numValues(), curOutputRow, sourceHasAlpha);
  }
  return true;
}

}  // namespace skia
