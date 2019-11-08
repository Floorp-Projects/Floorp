/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_COLOR_H
#define GFX_COLOR_H

#include "mozilla/Attributes.h"  // for MOZ_ALWAYS_INLINE
#include "mozilla/gfx/Types.h"   // for mozilla::gfx::SurfaceFormatBit

/**
 * Fast approximate division by 255. It has the property that
 * for all 0 <= n <= 255*255, GFX_DIVIDE_BY_255(n) == n/255.
 * But it only uses two adds and two shifts instead of an
 * integer division (which is expensive on many processors).
 *
 * equivalent to ((v)/255)
 */
#define GFX_DIVIDE_BY_255(v) \
  (((((unsigned)(v)) << 8) + ((unsigned)(v)) + 255) >> 16)

/**
 * Fast premultiply
 *
 * equivalent to (((c)*(a))/255)
 */
uint8_t MOZ_ALWAYS_INLINE gfxPreMultiply(uint8_t c, uint8_t a) {
  return GFX_DIVIDE_BY_255((c) * (a));
}

/**
 * Pack the 4 8-bit channels (A,R,G,B)
 * into a 32-bit packed NON-premultiplied pixel.
 */
uint32_t MOZ_ALWAYS_INLINE gfxPackedPixelNoPreMultiply(uint8_t a, uint8_t r,
                                                       uint8_t g, uint8_t b) {
  return (((a) << mozilla::gfx::SurfaceFormatBit::OS_A) |
          ((r) << mozilla::gfx::SurfaceFormatBit::OS_R) |
          ((g) << mozilla::gfx::SurfaceFormatBit::OS_G) |
          ((b) << mozilla::gfx::SurfaceFormatBit::OS_B));
}

/**
 * Pack the 4 8-bit channels (A,R,G,B)
 * into a 32-bit packed premultiplied pixel.
 */
uint32_t MOZ_ALWAYS_INLINE gfxPackedPixel(uint8_t a, uint8_t r, uint8_t g,
                                          uint8_t b) {
  if (a == 0x00)
    return 0x00000000;
  else if (a == 0xFF) {
    return gfxPackedPixelNoPreMultiply(a, r, g, b);
  } else {
    return ((a) << mozilla::gfx::SurfaceFormatBit::OS_A) |
           (gfxPreMultiply(r, a) << mozilla::gfx::SurfaceFormatBit::OS_R) |
           (gfxPreMultiply(g, a) << mozilla::gfx::SurfaceFormatBit::OS_G) |
           (gfxPreMultiply(b, a) << mozilla::gfx::SurfaceFormatBit::OS_B);
  }
}

#endif /* _GFX_COLOR_H */
