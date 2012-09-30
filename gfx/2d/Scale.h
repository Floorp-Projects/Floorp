/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALE_H_
#define MOZILLA_GFX_SCALE_H_

#include "Types.h"

namespace mozilla {
namespace gfx {

/**
 * Scale an image using a high-quality filter.
 *
 * Synchronously scales an image and writes the output to the destination in
 * 32-bit format. The destination must be pre-allocated by the caller.
 *
 * Returns true if scaling was successful, and false otherwise. Currently, this
 * function is implemented using Skia. If Skia is not enabled when building,
 * calling this function will always return false.
 *
 * IMPLEMTATION NOTES:
 * This API is not currently easily hardware acceleratable. A better API might
 * take a SourceSurface and return a SourceSurface; the Direct2D backend, for
 * example, could simply set a status bit on a copy of the image, and use
 * Direct2D's high-quality scaler at draw time.
 */
GFX2D_API bool Scale(uint8_t* srcData, int32_t srcWidth, int32_t srcHeight, int32_t srcStride,
                     uint8_t* dstData, int32_t dstWidth, int32_t dstHeight, int32_t dstStride,
                     SurfaceFormat format);

}
}

#endif /* MOZILLA_GFX_BLUR_H_ */
