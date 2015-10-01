// Copyright (c) 2006-2011 The Chromium Authors. All rights reserved.
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

#ifndef SKIA_EXT_IMAGE_OPERATIONS_H_
#define SKIA_EXT_IMAGE_OPERATIONS_H_

#include "skia/include/core/SkTypes.h"
#include "Types.h"
#include "convolver.h"
#include "skia/include/core/SkRect.h"

class SkBitmap;
struct SkIRect;

namespace skia {

class ImageOperations {
 public:
  enum ResizeMethod {
    //
    // Quality Methods
    //
    // Those enumeration values express a desired quality/speed tradeoff.
    // They are translated into an algorithm-specific method that depends
    // on the capabilities (CPU, GPU) of the underlying platform.
    // It is possible for all three methods to be mapped to the same
    // algorithm on a given platform.

    // Good quality resizing. Fastest resizing with acceptable visual quality.
    // This is typically intended for use during interactive layouts
    // where slower platforms may want to trade image quality for large
    // increase in resizing performance.
    //
    // For example the resizing implementation may devolve to linear
    // filtering if this enables GPU acceleration to be used.
    //
    // Note that the underlying resizing method may be determined
    // on the fly based on the parameters for a given resize call.
    // For example an implementation using a GPU-based linear filter
    // in the common case may still use a higher-quality software-based
    // filter in cases where using the GPU would actually be slower - due
    // to too much latency - or impossible - due to image format or size
    // constraints.
    RESIZE_GOOD,

    // Medium quality resizing. Close to high quality resizing (better
    // than linear interpolation) with potentially some quality being
    // traded-off for additional speed compared to RESIZE_BEST.
    //
    // This is intended, for example, for generation of large thumbnails
    // (hundreds of pixels in each dimension) from large sources, where
    // a linear filter would produce too many artifacts but where
    // a RESIZE_HIGH might be too costly time-wise.
    RESIZE_BETTER,

    // High quality resizing. The algorithm is picked to favor image quality.
    RESIZE_BEST,

    //
    // Algorithm-specific enumerations
    //

    // Box filter. This is a weighted average of all of the pixels touching
    // the destination pixel. For enlargement, this is nearest neighbor.
    //
    // You probably don't want this, it is here for testing since it is easy to
    // compute. Use RESIZE_LANCZOS3 instead.
    RESIZE_BOX,

    // 1-cycle Hamming filter. This is tall is the middle and falls off towards
    // the window edges but without going to 0. This is about 40% faster than
    // a 2-cycle Lanczos.
    RESIZE_HAMMING1,

    // 2-cycle Lanczos filter. This is tall in the middle, goes negative on
    // each side, then returns to zero. Does not provide as good a frequency
    // response as a 3-cycle Lanczos but is roughly 30% faster.
    RESIZE_LANCZOS2,

    // 3-cycle Lanczos filter. This is tall in the middle, goes negative on
    // each side, then oscillates 2 more times. It gives nice sharp edges.
    RESIZE_LANCZOS3,

    // Lanczos filter + subpixel interpolation. If subpixel rendering is not
    // appropriate we automatically fall back to Lanczos.
    RESIZE_SUBPIXEL,

    // enum aliases for first and last methods by algorithm or by quality.
    RESIZE_FIRST_QUALITY_METHOD = RESIZE_GOOD,
    RESIZE_LAST_QUALITY_METHOD = RESIZE_BEST,
    RESIZE_FIRST_ALGORITHM_METHOD = RESIZE_BOX,
    RESIZE_LAST_ALGORITHM_METHOD = RESIZE_SUBPIXEL,
  };

  // Resizes the given source bitmap using the specified resize method, so that
  // the entire image is (dest_size) big. The dest_subset is the rectangle in
  // this destination image that should actually be returned.
  //
  // The output image will be (dest_subset.width(), dest_subset.height()). This
  // will save work if you do not need the entire bitmap.
  //
  // The destination subset must be smaller than the destination image.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         int dest_width, int dest_height,
                         const SkIRect& dest_subset,
                         void* dest_pixels = nullptr);

  // Alternate version for resizing and returning the entire bitmap rather than
  // a subset.
  static SkBitmap Resize(const SkBitmap& source,
                         ResizeMethod method,
                         int dest_width, int dest_height,
                         void* dest_pixels = nullptr);

 private:
  ImageOperations();  // Class for scoping only.

  // Supports all methods except RESIZE_SUBPIXEL.
  static SkBitmap ResizeBasic(const SkBitmap& source,
                              ResizeMethod method,
                              int dest_width, int dest_height,
                              const SkIRect& dest_subset,
                              void* dest_pixels = nullptr);

  // Subpixel renderer.
  static SkBitmap ResizeSubpixel(const SkBitmap& source,
                                 int dest_width, int dest_height,
                                 const SkIRect& dest_subset);
};

// Returns the ceiling/floor as an integer.
inline int CeilInt(float val) {
  return static_cast<int>(ceil(val));
}
inline int FloorInt(float val) {
  return static_cast<int>(floor(val));
}

// Filter function computation -------------------------------------------------

// Evaluates the box filter, which goes from -0.5 to +0.5.
inline float EvalBox(float x) {
  return (x >= -0.5f && x < 0.5f) ? 1.0f : 0.0f;
}

// Evaluates the Lanczos filter of the given filter size window for the given
// position.
//
// |filter_size| is the width of the filter (the "window"), outside of which
// the value of the function is 0. Inside of the window, the value is the
// normalized sinc function:
//   lanczos(x) = sinc(x) * sinc(x / filter_size);
// where
//   sinc(x) = sin(pi*x) / (pi*x);
inline float EvalLanczos(int filter_size, float x) {
  if (x <= -filter_size || x >= filter_size)
    return 0.0f;  // Outside of the window.
  if (x > -std::numeric_limits<float>::epsilon() &&
      x < std::numeric_limits<float>::epsilon())
    return 1.0f;  // Special case the discontinuity at the origin.
  float xpi = x * static_cast<float>(M_PI);
  return (sinf(xpi) / xpi) *  // sinc(x)
          sinf(xpi / filter_size) / (xpi / filter_size);  // sinc(x/filter_size)
}

// Evaluates the Hamming filter of the given filter size window for the given
// position.
//
// The filter covers [-filter_size, +filter_size]. Outside of this window
// the value of the function is 0. Inside of the window, the value is sinus
// cardinal multiplied by a recentered Hamming function. The traditional
// Hamming formula for a window of size N and n ranging in [0, N-1] is:
//   hamming(n) = 0.54 - 0.46 * cos(2 * pi * n / (N-1)))
// In our case we want the function centered for x == 0 and at its minimum
// on both ends of the window (x == +/- filter_size), hence the adjusted
// formula:
//   hamming(x) = (0.54 -
//                 0.46 * cos(2 * pi * (x - filter_size)/ (2 * filter_size)))
//              = 0.54 - 0.46 * cos(pi * x / filter_size - pi)
//              = 0.54 + 0.46 * cos(pi * x / filter_size)
inline float EvalHamming(int filter_size, float x) {
  if (x <= -filter_size || x >= filter_size)
    return 0.0f;  // Outside of the window.
  if (x > -std::numeric_limits<float>::epsilon() &&
      x < std::numeric_limits<float>::epsilon())
    return 1.0f;  // Special case the sinc discontinuity at the origin.
  const float xpi = x * static_cast<float>(M_PI);

  return ((sinf(xpi) / xpi) *  // sinc(x)
          (0.54f + 0.46f * cosf(xpi / filter_size)));  // hamming(x)
}

// ResizeFilter ----------------------------------------------------------------

// Encapsulates computation and storage of the filters required for one complete
// resize operation.

namespace resize {

  // Returns the number of pixels that the filer spans, in filter space (the
  // destination image).
  inline float GetFilterSupport(ImageOperations::ResizeMethod method,
                                float scale) {
    switch (method) {
      case ImageOperations::RESIZE_BOX:
        // The box filter just scales with the image scaling.
        return 0.5f;  // Only want one side of the filter = /2.
      case ImageOperations::RESIZE_HAMMING1:
        // The Hamming filter takes as much space in the source image in
        // each direction as the size of the window = 1 for Hamming1.
        return 1.0f;
      case ImageOperations::RESIZE_LANCZOS2:
        // The Lanczos filter takes as much space in the source image in
        // each direction as the size of the window = 2 for Lanczos2.
        return 2.0f;
      case ImageOperations::RESIZE_LANCZOS3:
        // The Lanczos filter takes as much space in the source image in
        // each direction as the size of the window = 3 for Lanczos3.
        return 3.0f;
      default:
        return 1.0f;
    }
  }

  // Computes one set of filters either horizontally or vertically. The caller
  // will specify the "min" and "max" rather than the bottom/top and
  // right/bottom so that the same code can be re-used in each dimension.
  //
  // |src_depend_lo| and |src_depend_size| gives the range for the source
  // depend rectangle (horizontally or vertically at the caller's discretion
  // -- see above for what this means).
  //
  // Likewise, the range of destination values to compute and the scale factor
  // for the transform is also specified.
  void ComputeFilters(ImageOperations::ResizeMethod method,
                      int src_size, int dst_size,
                      int dest_subset_lo, int dest_subset_size,
                      ConvolutionFilter1D* output);

  // Computes the filter value given the coordinate in filter space.
  inline float ComputeFilter(ImageOperations::ResizeMethod method, float pos) {
    switch (method) {
      case ImageOperations::RESIZE_BOX:
        return EvalBox(pos);
      case ImageOperations::RESIZE_HAMMING1:
        return EvalHamming(1, pos);
      case ImageOperations::RESIZE_LANCZOS2:
        return EvalLanczos(2, pos);
      case ImageOperations::RESIZE_LANCZOS3:
        return EvalLanczos(3, pos);
      default:
        return 0;
    }
  }
}

}  // namespace skia

#endif  // SKIA_EXT_IMAGE_OPERATIONS_H_
