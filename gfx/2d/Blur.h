/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla gfx.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef MOZILLA_GFX_BLUR_H_
#define MOZILLA_GFX_BLUR_H_

#include "mozilla/gfx/Rect.h"
#include "mozilla/gfx/Point.h"

namespace mozilla {
namespace gfx {

/**
 * Implementation of a triple box blur approximation of a Gaussian blur.
 *
 * A Gaussian blur is good for blurring because, when done independently
 * in the horizontal and vertical directions, it matches the result that
 * would be obtained using a different (rotated) set of axes.  A triple
 * box blur is a very close approximation of a Gaussian.
 *
 * Creates an 8-bit alpha channel context for callers to draw in,
 * spreads the contents of that context, and blurs the contents.
 *
 * A spread N makes each output pixel the maximum value of all source
 * pixels within a square of side length 2N+1 centered on the output pixel.
 *
 * A temporary surface is created in the Init function. The caller then draws
 * any desired content onto the context acquired through GetContext, and lastly
 * calls Paint to apply the blurred content as an alpha mask.
 */
class GFX2D_API AlphaBoxBlur
{
public:

  /** Constructs a box blur and initializes the backing surface.
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
   *   safe to pass NULL here.
   *
   * @param aSkipRect A pointer to a rect, measured in device units, that
   *   represents an area where blurring is unnecessary and shouldn't be done for
   *   speed reasons. It is safe to pass NULL here.
   */
  AlphaBoxBlur(const Rect& aRect,
               const IntSize& aSpreadRadius,
               const IntSize& aBlurRadius,
               const Rect* aDirtyRect,
               const Rect* aSkipRect);

  ~AlphaBoxBlur();

  /**
   * Return the pointer to memory allocated by the constructor for the 8-bit
   * alpha surface you need to be blurred. After you draw to this surface, call
   * Blur(), below, to have its contents blurred.
   */
  unsigned char* GetData();

  /**
   * Return the size, in pixels, of the 8-bit alpha surface backed by the
   * pointer returned by GetData().
   */
  IntSize GetSize();

  /**
   * Return the stride, in bytes, of the 8-bit alpha surface backed by the
   * pointer returned by GetData().
   */
  int32_t GetStride();

  /**
   * Returns the device-space rectangle the 8-bit alpha surface covers.
   */
  IntRect GetRect();

  /**
   * Return a pointer to a dirty rect, as passed in to the constructor, or NULL
   * if none was passed in.
   */
  Rect* GetDirtyRect();

  /**
   * Perform the blur in-place on the surface backed by the pointer returned by
   * GetData().
   */
  void Blur();

  /**
   * Calculates a blur radius that, when used with box blur, approximates a
   * Gaussian blur with the given standard deviation.  The result of this
   * function should be used as the aBlurRadius parameter to AlphaBoxBlur's
   * constructor, above.
   */
  static IntSize CalculateBlurRadius(const Point& aStandardDeviation);

private:

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
   * A pointer to the backing 8-bit alpha surface.
   */
  unsigned char* mData;

  /**
   * The stride of the data contained in mData.
   */
  int32_t mStride;

  /**
   * Whether mDirtyRect contains valid data.
   */
  bool mHasDirtyRect;
};

}
}

#endif /* MOZILLA_GFX_BLUR_H_ */
