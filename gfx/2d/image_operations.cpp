// Copyright (c) 2006-2012 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in
//    the documentation and/or other materials provided with the
//    distribution.
//  * Neither the name of Google, Inc. nor the names of its contributors
//    may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
// OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.

#include "base/basictypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "image_operations.h"

#include "base/stack_container.h"
#include "convolver.h"
#include "skia/include/core/SkColorPriv.h"
#include "skia/include/core/SkBitmap.h"
#include "skia/include/core/SkRect.h"
#include "skia/include/core/SkFontHost.h"

namespace skia {

namespace resize {

// TODO(egouriou): Take advantage of periods in the convolution.
// Practical resizing filters are periodic outside of the border area.
// For Lanczos, a scaling by a (reduced) factor of p/q (q pixels in the
// source become p pixels in the destination) will have a period of p.
// A nice consequence is a period of 1 when downscaling by an integral
// factor. Downscaling from typical display resolutions is also bound
// to produce interesting periods as those are chosen to have multiple
// small factors.
// Small periods reduce computational load and improve cache usage if
// the coefficients can be shared. For periods of 1 we can consider
// loading the factors only once outside the borders.
void ComputeFilters(ImageOperations::ResizeMethod method,
                    int src_size, int dst_size,
                    int dest_subset_lo, int dest_subset_size,
                    ConvolutionFilter1D* output) {
  // method_ will only ever refer to an "algorithm method".
  SkASSERT((ImageOperations::RESIZE_FIRST_ALGORITHM_METHOD <= method) &&
           (method <= ImageOperations::RESIZE_LAST_ALGORITHM_METHOD));

  float scale = static_cast<float>(dst_size) / static_cast<float>(src_size);
 
  int dest_subset_hi = dest_subset_lo + dest_subset_size;  // [lo, hi)

  // When we're doing a magnification, the scale will be larger than one. This
  // means the destination pixels are much smaller than the source pixels, and
  // that the range covered by the filter won't necessarily cover any source
  // pixel boundaries. Therefore, we use these clamped values (max of 1) for
  // some computations.
  float clamped_scale = std::min(1.0f, scale);

  float src_support = GetFilterSupport(method, clamped_scale) / clamped_scale;

  // Speed up the divisions below by turning them into multiplies.
  float inv_scale = 1.0f / scale;

  StackVector<float, 64> filter_values;
  StackVector<int16_t, 64> fixed_filter_values;

  // Loop over all pixels in the output range. We will generate one set of
  // filter values for each one. Those values will tell us how to blend the
  // source pixels to compute the destination pixel.
  for (int dest_subset_i = dest_subset_lo; dest_subset_i < dest_subset_hi;
       dest_subset_i++) {
    // Reset the arrays. We don't declare them inside so they can re-use the
    // same malloc-ed buffer.
    filter_values->clear();
    fixed_filter_values->clear();

    // This is the pixel in the source directly under the pixel in the dest.
    // Note that we base computations on the "center" of the pixels. To see
    // why, observe that the destination pixel at coordinates (0, 0) in a 5.0x
    // downscale should "cover" the pixels around the pixel with *its center*
    // at coordinates (2.5, 2.5) in the source, not those around (0, 0).
    // Hence we need to scale coordinates (0.5, 0.5), not (0, 0).
    float src_pixel = (static_cast<float>(dest_subset_i) + 0.5f) * inv_scale;

    // Compute the (inclusive) range of source pixels the filter covers.
    int src_begin = std::max(0, FloorInt(src_pixel - src_support));
    int src_end = std::min(src_size - 1, CeilInt(src_pixel + src_support));

    // Compute the unnormalized filter value at each location of the source
    // it covers.
    float filter_sum = 0.0f;  // Sub of the filter values for normalizing.
    for (int cur_filter_pixel = src_begin; cur_filter_pixel <= src_end;
         cur_filter_pixel++) {
      // Distance from the center of the filter, this is the filter coordinate
      // in source space. We also need to consider the center of the pixel
      // when comparing distance against 'src_pixel'. In the 5x downscale
      // example used above the distance from the center of the filter to
      // the pixel with coordinates (2, 2) should be 0, because its center
      // is at (2.5, 2.5).
      float src_filter_dist =
           ((static_cast<float>(cur_filter_pixel) + 0.5f) - src_pixel);

      // Since the filter really exists in dest space, map it there.
      float dest_filter_dist = src_filter_dist * clamped_scale;

      // Compute the filter value at that location.
      float filter_value = ComputeFilter(method, dest_filter_dist);
      filter_values->push_back(filter_value);

      filter_sum += filter_value;
    }

    // The filter must be normalized so that we don't affect the brightness of
    // the image. Convert to normalized fixed point.
    int16_t fixed_sum = 0;
    for (size_t i = 0; i < filter_values->size(); i++) {
      int16_t cur_fixed = output->FloatToFixed(filter_values[i] / filter_sum);
      fixed_sum += cur_fixed;
      fixed_filter_values->push_back(cur_fixed);
    }

    // The conversion to fixed point will leave some rounding errors, which
    // we add back in to avoid affecting the brightness of the image. We
    // arbitrarily add this to the center of the filter array (this won't always
    // be the center of the filter function since it could get clipped on the
    // edges, but it doesn't matter enough to worry about that case).
    int16_t leftovers = output->FloatToFixed(1.0f) - fixed_sum;
    fixed_filter_values[fixed_filter_values->size() / 2] += leftovers;

    // Now it's ready to go.
    output->AddFilter(src_begin, &fixed_filter_values[0],
                      static_cast<int>(fixed_filter_values->size()));
  }

  output->PaddingForSIMD(8);
}

} // namespace resize

ImageOperations::ResizeMethod ResizeMethodToAlgorithmMethod(
    ImageOperations::ResizeMethod method) {
  // Convert any "Quality Method" into an "Algorithm Method"
  if (method >= ImageOperations::RESIZE_FIRST_ALGORITHM_METHOD &&
      method <= ImageOperations::RESIZE_LAST_ALGORITHM_METHOD) {
    return method;
  }
  // The call to ImageOperationsGtv::Resize() above took care of
  // GPU-acceleration in the cases where it is possible. So now we just
  // pick the appropriate software method for each resize quality.
  switch (method) {
    // Users of RESIZE_GOOD are willing to trade a lot of quality to
    // get speed, allowing the use of linear resampling to get hardware
    // acceleration (SRB). Hence any of our "good" software filters
    // will be acceptable, and we use the fastest one, Hamming-1.
    case ImageOperations::RESIZE_GOOD:
      // Users of RESIZE_BETTER are willing to trade some quality in order
      // to improve performance, but are guaranteed not to devolve to a linear
      // resampling. In visual tests we see that Hamming-1 is not as good as
      // Lanczos-2, however it is about 40% faster and Lanczos-2 itself is
      // about 30% faster than Lanczos-3. The use of Hamming-1 has been deemed
      // an acceptable trade-off between quality and speed.
    case ImageOperations::RESIZE_BETTER:
      return ImageOperations::RESIZE_HAMMING1;
    default:
      return ImageOperations::RESIZE_LANCZOS3;
  }
}

// Resize ----------------------------------------------------------------------

// static
SkBitmap ImageOperations::Resize(const SkBitmap& source,
                                 ResizeMethod method,
                                 int dest_width, int dest_height,
                                 const SkIRect& dest_subset,
                                 void* dest_pixels /* = nullptr */) {
  if (method == ImageOperations::RESIZE_SUBPIXEL)
    return ResizeSubpixel(source, dest_width, dest_height, dest_subset);
  else
    return ResizeBasic(source, method, dest_width, dest_height, dest_subset,
                       dest_pixels);
}

// static
SkBitmap ImageOperations::ResizeSubpixel(const SkBitmap& source,
                                         int dest_width, int dest_height,
                                         const SkIRect& dest_subset) {
  // Currently only works on Linux/BSD because these are the only platforms
  // where SkFontHost::GetSubpixelOrder is defined.
#if defined(XP_UNIX)
  // Understand the display.
  const SkFontHost::LCDOrder order = SkFontHost::GetSubpixelOrder();
  const SkFontHost::LCDOrientation orientation =
      SkFontHost::GetSubpixelOrientation();

  // Decide on which dimension, if any, to deploy subpixel rendering.
  int w = 1;
  int h = 1;
  switch (orientation) {
    case SkFontHost::kHorizontal_LCDOrientation:
      w = dest_width < source.width() ? 3 : 1;
      break;
    case SkFontHost::kVertical_LCDOrientation:
      h = dest_height < source.height() ? 3 : 1;
      break;
  }

  // Resize the image.
  const int width = dest_width * w;
  const int height = dest_height * h;
  SkIRect subset = { dest_subset.fLeft, dest_subset.fTop,
                     dest_subset.fLeft + dest_subset.width() * w,
                     dest_subset.fTop + dest_subset.height() * h };
  SkBitmap img = ResizeBasic(source, ImageOperations::RESIZE_LANCZOS3, width,
                             height, subset);
  const int row_words = img.rowBytes() / 4;
  if (w == 1 && h == 1)
    return img;

  // Render into subpixels.
  SkBitmap result;
  SkImageInfo info = SkImageInfo::Make(dest_subset.width(),
                                       dest_subset.height(),
                                       kBGRA_8888_SkColorType,
                                       kPremul_SkAlphaType);


  result.allocPixels(info);
  if (!result.readyToDraw())
    return img;

  SkAutoLockPixels locker(img);
  if (!img.readyToDraw())
    return img;

  uint32_t* src_row = img.getAddr32(0, 0);
  uint32_t* dst_row = result.getAddr32(0, 0);
  for (int y = 0; y < dest_subset.height(); y++) {
    uint32_t* src = src_row;
    uint32_t* dst = dst_row;
    for (int x = 0; x < dest_subset.width(); x++, src += w, dst++) {
      uint8_t r = 0, g = 0, b = 0, a = 0;
      switch (order) {
        case SkFontHost::kRGB_LCDOrder:
          switch (orientation) {
            case SkFontHost::kHorizontal_LCDOrientation:
              r = SkGetPackedR32(src[0]);
              g = SkGetPackedG32(src[1]);
              b = SkGetPackedB32(src[2]);
              a = SkGetPackedA32(src[1]);
              break;
            case SkFontHost::kVertical_LCDOrientation:
              r = SkGetPackedR32(src[0 * row_words]);
              g = SkGetPackedG32(src[1 * row_words]);
              b = SkGetPackedB32(src[2 * row_words]);
              a = SkGetPackedA32(src[1 * row_words]);
              break;
          }
          break;
        case SkFontHost::kBGR_LCDOrder:
          switch (orientation) {
            case SkFontHost::kHorizontal_LCDOrientation:
              b = SkGetPackedB32(src[0]);
              g = SkGetPackedG32(src[1]);
              r = SkGetPackedR32(src[2]);
              a = SkGetPackedA32(src[1]);
              break;
            case SkFontHost::kVertical_LCDOrientation:
              b = SkGetPackedB32(src[0 * row_words]);
              g = SkGetPackedG32(src[1 * row_words]);
              r = SkGetPackedR32(src[2 * row_words]);
              a = SkGetPackedA32(src[1 * row_words]);
              break;
          }
          break;
        case SkFontHost::kNONE_LCDOrder:
          break;
      }
      // Premultiplied alpha is very fragile.
      a = a > r ? a : r;
      a = a > g ? a : g;
      a = a > b ? a : b;
      *dst = SkPackARGB32(a, r, g, b);
    }
    src_row += h * row_words;
    dst_row += result.rowBytes() / 4;
  }
  result.setAlphaType(img.alphaType());
  return result;
#else
  return SkBitmap();
#endif  // OS_POSIX && !OS_MACOSX && !defined(OS_ANDROID)
}

// static
SkBitmap ImageOperations::ResizeBasic(const SkBitmap& source,
                                      ResizeMethod method,
                                      int dest_width, int dest_height,
                                      const SkIRect& dest_subset,
                                      void* dest_pixels /* = nullptr */) {
  // Ensure that the ResizeMethod enumeration is sound.
  SkASSERT(((RESIZE_FIRST_QUALITY_METHOD <= method) &&
            (method <= RESIZE_LAST_QUALITY_METHOD)) ||
           ((RESIZE_FIRST_ALGORITHM_METHOD <= method) &&
            (method <= RESIZE_LAST_ALGORITHM_METHOD)));

  // If the size of source or destination is 0, i.e. 0x0, 0xN or Nx0, just
  // return empty.
  if (source.width() < 1 || source.height() < 1 ||
      dest_width < 1 || dest_height < 1)
    return SkBitmap();

  method = ResizeMethodToAlgorithmMethod(method);
  // Check that we deal with an "algorithm methods" from this point onward.
  SkASSERT((ImageOperations::RESIZE_FIRST_ALGORITHM_METHOD <= method) &&
           (method <= ImageOperations::RESIZE_LAST_ALGORITHM_METHOD));

  SkAutoLockPixels locker(source);
  if (!source.readyToDraw())
      return SkBitmap();

  ConvolutionFilter1D x_filter;
  ConvolutionFilter1D y_filter;

  resize::ComputeFilters(method, source.width(), dest_width, dest_subset.fLeft, dest_subset.width(), &x_filter);
  resize::ComputeFilters(method, source.height(), dest_height, dest_subset.fTop, dest_subset.height(), &y_filter);

  // Get a source bitmap encompassing this touched area. We construct the
  // offsets and row strides such that it looks like a new bitmap, while
  // referring to the old data.
  const uint8_t* source_subset =
      reinterpret_cast<const uint8_t*>(source.getPixels());

  // Convolve into the result.
  SkBitmap result;
  SkImageInfo info = SkImageInfo::Make(dest_subset.width(),
                                       dest_subset.height(),
                                       kBGRA_8888_SkColorType,
                                       kPremul_SkAlphaType);

  if (dest_pixels) {
    result.installPixels(info, dest_pixels, info.minRowBytes());
  } else {
    result.allocPixels(info);
  }

  if (!result.readyToDraw())
    return SkBitmap();

  BGRAConvolve2D(source_subset, static_cast<int>(source.rowBytes()),
                 !source.isOpaque(), x_filter, y_filter,
                 static_cast<int>(result.rowBytes()),
                 static_cast<unsigned char*>(result.getPixels()));

  // Preserve the "opaque" flag for use as an optimization later.
  result.setAlphaType(source.alphaType());

  return result;
}

// static
SkBitmap ImageOperations::Resize(const SkBitmap& source,
                                 ResizeMethod method,
                                 int dest_width, int dest_height,
                                 void* dest_pixels /* = nullptr */) {
  SkIRect dest_subset = { 0, 0, dest_width, dest_height };
  return Resize(source, method, dest_width, dest_height, dest_subset,
                dest_pixels);
}

} // namespace skia
