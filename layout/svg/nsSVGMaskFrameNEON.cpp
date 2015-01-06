/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSVGMaskFrameNEON.h"
#include "nsSVGMaskFrame.h"
#include <arm_neon.h>

void
ComputesRGBLuminanceMask_NEON(uint8_t *aData,
                              int32_t aStride,
                              const IntSize &aSize,
                              float aOpacity)
{
  int32_t redFactor = 55 * aOpacity; // 255 * 0.2125 * opacity
  int32_t greenFactor = 183 * aOpacity; // 255 * 0.7154 * opacity
  int32_t blueFactor = 18 * aOpacity; // 255 * 0.0721
  uint8_t *pixel = aData;
  int32_t offset = aStride - 4 * aSize.width;

  // Set the value to zero if the alpha is zero
  for (int32_t y = 0; y < aSize.height; y++) {
    for (int32_t x = 0; x < aSize.width; x++) {
      if (!pixel[GFX_ARGB32_OFFSET_A]) {
        memset(pixel, 0, 4);
      }
      pixel += 4;
    }
    pixel += offset;
  }

  pixel = aData;
  int32_t remainderWidth = aSize.width % 8;
  int32_t roundedWidth = aSize.width - remainderWidth;
  uint16x8_t temp;
  uint8x8_t gray;
  uint8x8x4_t result;
  uint8x8_t redVec = vdup_n_u8(redFactor);
  uint8x8_t greenVec = vdup_n_u8(greenFactor);
  uint8x8_t blueVec = vdup_n_u8(blueFactor);
  for (int32_t y = 0; y < aSize.height; y++) {
    // Calculate luminance by neon with 8 pixels per loop 
    for (int32_t x = 0; x < roundedWidth; x += 8) {
      uint8x8x4_t argb  = vld4_u8(pixel);
      temp = vmull_u8(argb.val[GFX_ARGB32_OFFSET_R], redVec); // temp = red * redFactor
      temp = vmlal_u8(temp, argb.val[GFX_ARGB32_OFFSET_G], greenVec); // temp += green * greenFactor
      temp = vmlal_u8(temp, argb.val[GFX_ARGB32_OFFSET_B], blueVec); // temp += blue * blueFactor
      gray = vshrn_n_u16(temp, 8); // gray = temp >> 8

      // Put the result to the 8 pixels in argb format
      result.val[0] = gray;
      result.val[1] = gray;
      result.val[2] = gray;
      result.val[3] = gray;
      vst4_u8(pixel, result);
      pixel += 8 * 4;
    }

    // Calculate the rest pixels of the line by cpu
    for (int32_t x = 0; x < remainderWidth; x++) {
      pixel[0] = (redFactor * pixel[GFX_ARGB32_OFFSET_R] +
                  greenFactor * pixel[GFX_ARGB32_OFFSET_G] +
                  blueFactor * pixel[GFX_ARGB32_OFFSET_B]) >> 8;
      memset(pixel + 1, pixel[0], 3);
      pixel += 4;
    }
    pixel += offset;
  }
}

