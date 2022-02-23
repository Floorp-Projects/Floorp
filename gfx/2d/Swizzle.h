/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SWIZZLE_H_
#define MOZILLA_GFX_SWIZZLE_H_

#include "Point.h"
#include "Rect.h"

namespace mozilla {
namespace image {
struct Orientation;
}

namespace gfx {

/**
 * Premultiplies source and writes it to destination. Source and destination may
 * be the same to premultiply in-place. The source format must have an alpha
 * channel.
 */
GFX2D_API bool PremultiplyData(const uint8_t* aSrc, int32_t aSrcStride,
                               SurfaceFormat aSrcFormat, uint8_t* aDst,
                               int32_t aDstStride, SurfaceFormat aDstFormat,
                               const IntSize& aSize);

/**
 * Unpremultiplies source and writes it to destination. Source and destination
 * may be the same to unpremultiply in-place. Both the source and destination
 * formats must have an alpha channel.
 */
GFX2D_API bool UnpremultiplyData(const uint8_t* aSrc, int32_t aSrcStride,
                                 SurfaceFormat aSrcFormat, uint8_t* aDst,
                                 int32_t aDstStride, SurfaceFormat aDstFormat,
                                 const IntSize& aSize);

/**
 * Swizzles source and writes it to destination. Source and destination may be
 * the same to swizzle in-place.
 */
GFX2D_API bool SwizzleData(const uint8_t* aSrc, int32_t aSrcStride,
                           SurfaceFormat aSrcFormat, uint8_t* aDst,
                           int32_t aDstStride, SurfaceFormat aDstFormat,
                           const IntSize& aSize);

/**
 * Flips rows of source and swizzles it to destination. Source and destination
 * may be the same to swizzle in-place; this will fail if it cannot allocate a
 * temporary buffer.
 */
GFX2D_API bool SwizzleYFlipData(const uint8_t* aSrc, int32_t aSrcStride,
                                SurfaceFormat aSrcFormat, uint8_t* aDst,
                                int32_t aDstStride, SurfaceFormat aDstFormat,
                                const IntSize& aSize);

/**
 * Flips rows of source and premultiplies/swizzles it to destination. Source and
 * destination may be the same to premultiply/swizzle in-place; this will fail
 * if it cannot allocate a temporary buffer.
 */
GFX2D_API bool PremultiplyYFlipData(const uint8_t* aSrc, int32_t aSrcStride,
                                    SurfaceFormat aSrcFormat, uint8_t* aDst,
                                    int32_t aDstStride,
                                    SurfaceFormat aDstFormat,
                                    const IntSize& aSize);

/**
 * Swizzles source and writes it to destination. Source and destination may be
 * the same to swizzle in-place.
 */
typedef void (*SwizzleRowFn)(const uint8_t* aSrc, uint8_t* aDst,
                             int32_t aLength);

/**
 * Get a function pointer to perform premultiplication between two formats.
 */
GFX2D_API SwizzleRowFn PremultiplyRow(SurfaceFormat aSrcFormat,
                                      SurfaceFormat aDstFormat);

/**
 * Get a function pointer to perform unpremultiplication between two formats.
 */
GFX2D_API SwizzleRowFn UnpremultiplyRow(SurfaceFormat aSrcFormat,
                                        SurfaceFormat aDstFormat);

/**
 * Get a function pointer to perform swizzling between two formats.
 */
GFX2D_API SwizzleRowFn SwizzleRow(SurfaceFormat aSrcFormat,
                                  SurfaceFormat aDstFormat);

/**
 * Reorients source and writes it to destination. Returns the dirty rect of
 * what was changed in aDst.
 */
typedef IntRect (*ReorientRowFn)(const uint8_t* aSrc, int32_t aSrcRow,
                                 uint8_t* aDst, const IntSize& aDstSize,
                                 int32_t aDstStride);

/**
 * Get a function pointer to perform reorientation by row.
 */
GFX2D_API ReorientRowFn
ReorientRow(const struct image::Orientation& aOrientation);

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SWIZZLE_H_ */
