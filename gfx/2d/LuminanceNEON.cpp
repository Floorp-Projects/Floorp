/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <arm_neon.h>
#include "LuminanceNEON.h"

using namespace mozilla::gfx;

/**
 * Byte offsets of channels in a native packed gfxColor or cairo image surface.
 */
#ifdef IS_BIG_ENDIAN
#  define GFX_ARGB32_OFFSET_A 0
#  define GFX_ARGB32_OFFSET_R 1
#  define GFX_ARGB32_OFFSET_G 2
#  define GFX_ARGB32_OFFSET_B 3
#else
#  define GFX_ARGB32_OFFSET_A 3
#  define GFX_ARGB32_OFFSET_R 2
#  define GFX_ARGB32_OFFSET_G 1
#  define GFX_ARGB32_OFFSET_B 0
#endif

void ComputesRGBLuminanceMask_NEON(const uint8_t* aSourceData,
                                   int32_t aSourceStride, uint8_t* aDestData,
                                   int32_t aDestStride, const IntSize& aSize,
                                   float aOpacity) {
  int32_t redFactor = 55 * aOpacity;     // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity;  // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity;    // 255 * 0.0721
  const uint8_t* sourcePixel = aSourceData;
  int32_t sourceOffset = aSourceStride - 4 * aSize.width;
  uint8_t* destPixel = aDestData;
  int32_t destOffset = aDestStride - aSize.width;

  sourcePixel = aSourceData;
  int32_t remainderWidth = aSize.width % 8;
  int32_t roundedWidth = aSize.width - remainderWidth;
  uint16x8_t temp;
  uint8x8_t gray;
  uint8x8_t redVector = vdup_n_u8(redFactor);
  uint8x8_t greenVector = vdup_n_u8(greenFactor);
  uint8x8_t blueVector = vdup_n_u8(blueFactor);
  uint8x8_t fullBitVector = vdup_n_u8(255);
  uint8x8_t oneVector = vdup_n_u8(1);
  for (int32_t y = 0; y < aSize.height; y++) {
    // Calculate luminance by neon with 8 pixels per loop
    for (int32_t x = 0; x < roundedWidth; x += 8) {
      uint8x8x4_t argb = vld4_u8(sourcePixel);
      temp = vmull_u8(argb.val[GFX_ARGB32_OFFSET_R],
                      redVector);  // temp = red * redFactor
      temp = vmlal_u8(temp, argb.val[GFX_ARGB32_OFFSET_G],
                      greenVector);  // temp += green * greenFactor
      temp = vmlal_u8(temp, argb.val[GFX_ARGB32_OFFSET_B],
                      blueVector);  // temp += blue * blueFactor
      gray = vshrn_n_u16(temp, 8);  // gray = temp >> 8

      // Check alpha value
      uint8x8_t alphaVector =
          vtst_u8(argb.val[GFX_ARGB32_OFFSET_A], fullBitVector);
      gray = vmul_u8(gray, vand_u8(alphaVector, oneVector));

      // Put the result to the 8 pixels
      vst1_u8(destPixel, gray);
      sourcePixel += 8 * 4;
      destPixel += 8;
    }

    // Calculate the rest pixels of the line by cpu
    for (int32_t x = 0; x < remainderWidth; x++) {
      if (sourcePixel[GFX_ARGB32_OFFSET_A] > 0) {
        *destPixel = (redFactor * sourcePixel[GFX_ARGB32_OFFSET_R] +
                      greenFactor * sourcePixel[GFX_ARGB32_OFFSET_G] +
                      blueFactor * sourcePixel[GFX_ARGB32_OFFSET_B]) >>
                     8;
      } else {
        *destPixel = 0;
      }
      sourcePixel += 4;
      destPixel++;
    }
    sourcePixel += sourceOffset;
    destPixel += destOffset;
  }
}
