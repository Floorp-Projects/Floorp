/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

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
 * returned in a buffer allocated with new uint8_t[].
 */
uint8_t*
SurfaceToPackedBGRA(DataSourceSurface *aSurface);

/**
 * Convert aSurface to a packed buffer in BGR format. The pixel data is
 * returned in a buffer allocated with new uint8_t[].
 *
 * This function is currently only intended for use with surfaces of format
 * SurfaceFormat::B8G8R8X8 since the X components of the pixel data are simply
 * dropped (no attempt is made to un-pre-multiply alpha from the color
 * components).
 */
uint8_t*
SurfaceToPackedBGR(DataSourceSurface *aSurface);

}
}
