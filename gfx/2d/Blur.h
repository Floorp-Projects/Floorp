/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_BLUR_H_
#define MOZILLA_GFX_BLUR_H_

#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/CheckedInt.h"

namespace mozilla {
namespace gfx {

#ifdef _MSC_VER
#pragma warning( disable : 4251 )
#endif

/**
 * Implementation of a triple box blur approximation of a Gaussian blur.
 *
 * A Gaussian blur is good for blurring because, when done independently
 * in the horizontal and vertical directions, it matches the result that
 * would be obtained using a different (rotated) set of axes.  A triple
 * box blur is a very close approximation of a Gaussian.
 *
 * This is a "service" class; the constructors set up all the information
 * based on the values and compute the minimum size for an 8-bit alpha
 * channel context.
 * The callers are responsible for creating and managing the backing surface
 * and passing the pointer to the data to the Blur() method.  This class does
 * not retain the pointer to the data outside of the Blur() call.
 *
 * A spread N makes each output pixel the maximum value of all source
 * pixels within a square of side length 2N+1 centered on the output pixel.
 */
class GFX2D_API AlphaBoxBlur
{
public:

  /** Constructs a box blur and computes the backing surface size.
   *
   * @param aRect The coordinates of the surface to create in device units.
   *
   * @param aBlurRadius The blur radius in pixels.  This is the radius of the
   *   entire (triple) kernel function.  Each individual box blur has radius
   *   approximately 1/3 this value, or diameter approximately 2/3 this value.
   *   This parameter should nearly always be computed using CalculateBlurRadius,
   *   below.
   *
   * @param aDirtyRect A pointer to a dirty rect, measured in device units, if
   *   available.  This will be used for optimizing the blur operation. It is
   *   safe to pass nullptr here.
   *
   * @param aSkipRect A pointer to a rect, measured in device units, that
   *   represents an area where blurring is unnecessary and shouldn't be done for
   *   speed reasons. It is safe to pass nullptr here.
   */
  AlphaBoxBlur(const Rect& aRect,
               const IntSize& aSpreadRadius,
               const IntSize& aBlurRadius,
               const Rect* aDirtyRect,
               const Rect* aSkipRect);

  AlphaBoxBlur(const Rect& aRect,
               int32_t aStride,
               float aSigmaX,
               float aSigmaY);

  ~AlphaBoxBlur();

  /**
   * Return the size, in pixels, of the 8-bit alpha surface we'd use.
   */
  IntSize GetSize();

  /**
   * Return the stride, in bytes, of the 8-bit alpha surface we'd use.
   */
  int32_t GetStride();

  /**
   * Returns the device-space rectangle the 8-bit alpha surface covers.
   */
  IntRect GetRect();

  /**
   * Return a pointer to a dirty rect, as passed in to the constructor, or nullptr
   * if none was passed in.
   */
  Rect* GetDirtyRect();

  /**
   * Return the minimum buffer size that should be given to Blur() method.  If
   * zero, the class is not properly setup for blurring.  Note that this
   * includes the extra three bytes on top of the stride*width, where something
   * like gfxImageSurface::GetDataSize() would report without it, even if it 
   * happens to have the extra bytes.
   */
  size_t GetSurfaceAllocationSize() const;

  /**
   * Perform the blur in-place on the surface backed by specified 8-bit
   * alpha surface data. The size must be at least that returned by
   * GetSurfaceAllocationSize() or bad things will happen.
   */
  void Blur(uint8_t* aData);

  /**
   * Calculates a blur radius that, when used with box blur, approximates a
   * Gaussian blur with the given standard deviation.  The result of this
   * function should be used as the aBlurRadius parameter to AlphaBoxBlur's
   * constructor, above.
   */
  static IntSize CalculateBlurRadius(const Point& aStandardDeviation);

private:

  void BoxBlur_C(uint8_t* aData,
                 int32_t aLeftLobe, int32_t aRightLobe, int32_t aTopLobe,
                 int32_t aBottomLobe, uint32_t *aIntegralImage, size_t aIntegralImageStride);
  void BoxBlur_SSE2(uint8_t* aData,
                    int32_t aLeftLobe, int32_t aRightLobe, int32_t aTopLobe,
                    int32_t aBottomLobe, uint32_t *aIntegralImage, size_t aIntegralImageStride);
#ifdef BUILD_ARM_NEON
  void BoxBlur_NEON(uint8_t* aData,
                    int32_t aLeftLobe, int32_t aRightLobe, int32_t aTopLobe,
                    int32_t aBottomLobe, uint32_t *aIntegralImage, size_t aIntegralImageStride);
#endif

  static CheckedInt<int32_t> RoundUpToMultipleOf4(int32_t aVal);

  /**
   * A rect indicating the area where blurring is unnecessary, and the blur
   * algorithm should skip over it.
   */
  IntRect mSkipRect;

  /**
   * The device-space rectangle the the backing 8-bit alpha surface covers.
   */
  IntRect mRect;

  /**
   * A copy of the dirty rect passed to the constructor. This will only be valid if
   * mHasDirtyRect is true.
   */
  Rect mDirtyRect;

  /**
   * The spread radius, in pixels.
   */
  IntSize mSpreadRadius;

  /**
   * The blur radius, in pixels.
   */
  IntSize mBlurRadius;

  /**
   * The stride of the data passed to Blur()
   */
  int32_t mStride;

  /**
   * The minimum size of the buffer needed for the Blur() operation.
   */
  size_t mSurfaceAllocationSize;

  /**
   * Whether mDirtyRect contains valid data.
   */
  bool mHasDirtyRect;
};

}
}

#endif /* MOZILLA_GFX_BLUR_H_ */
