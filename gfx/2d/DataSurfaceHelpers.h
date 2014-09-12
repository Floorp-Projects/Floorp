/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _MOZILLA_GFX_DATASURFACEHELPERS_H
#define _MOZILLA_GFX_DATASURFACEHELPERS_H

#include "2D.h"

namespace mozilla {
namespace gfx {

void
ConvertBGRXToBGRA(uint8_t* aData, const IntSize &aSize, int32_t aStride);

/**
 * Copy the pixel data from aSrc and pack it into aDst. aSrcSize, aSrcStride
 * and aBytesPerPixel give the size, stride and bytes per pixel for aSrc's
 * surface. Callers are responsible for making sure that aDst is big enough to
 * contain |aSrcSize.width * aSrcSize.height * aBytesPerPixel| bytes.
 */
void
CopySurfaceDataToPackedArray(uint8_t* aSrc, uint8_t* aDst, IntSize aSrcSize,
                             int32_t aSrcStride, int32_t aBytesPerPixel);

/**
 * Convert aSurface to a packed buffer in BGRA format. The pixel data is
 * returned in a buffer allocated with new uint8_t[]. The caller then has
 * ownership of the buffer and is responsible for delete[]'ing it.
 */
uint8_t*
SurfaceToPackedBGRA(DataSourceSurface *aSurface);

/**
 * Convert aSurface to a packed buffer in BGR format. The pixel data is
 * returned in a buffer allocated with new uint8_t[]. The caller then has
 * ownership of the buffer and is responsible for delete[]'ing it.
 *
 * This function is currently only intended for use with surfaces of format
 * SurfaceFormat::B8G8R8X8 since the X components of the pixel data (if any)
 * are simply dropped (no attempt is made to un-pre-multiply alpha from the
 * color components).
 */
uint8_t*
SurfaceToPackedBGR(DataSourceSurface *aSurface);

/**
 * Clears all the bytes in a DataSourceSurface's data array to zero (so to
 * transparent black for SurfaceFormat::B8G8R8A8, for example).
 * Note that DataSourceSurfaces can be initialized to zero, which is
 * more efficient than zeroing the surface after initialization.
 */
void
ClearDataSourceSurface(DataSourceSurface *aSurface);

/**
 * Multiplies aStride and aHeight and makes sure the result is limited to
 * something sane. To keep things consistent, this should always be used
 * wherever we allocate a buffer based on surface stride and height.
 *
 * @param aExtra Optional argument to specify an additional number of trailing
 *   bytes (useful for creating intermediate surfaces for filters, for
 *   example).
 *
 * @return The result of the multiplication if it is acceptable, or else zero.
 */
size_t
BufferSizeFromStrideAndHeight(int32_t aStride,
                              int32_t aHeight,
                              int32_t aExtraBytes = 0);

}
}

#endif // _MOZILLA_GFX_DATASURFACEHELPERS_H
