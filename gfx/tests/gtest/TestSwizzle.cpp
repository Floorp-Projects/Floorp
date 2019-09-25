/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/Swizzle.h"

using namespace mozilla;
using namespace mozilla::gfx;

TEST(Moz2D, PremultiplyData)
{
  const uint8_t in_bgra[5 * 4] = {
      255, 255, 0,   255,  // verify 255 alpha leaves RGB unchanged
      0,   0,   255, 255,
      0,   255, 255, 0,  // verify 0 alpha zeroes out RGB
      0,   0,   0,   0,
      255, 0,   0,   128,  // verify that 255 RGB maps to alpha
  };
  uint8_t out[5 * 4];
  const uint8_t check_bgra[5 * 4] = {
      255, 255, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 128,
  };
  // check swizzled output
  const uint8_t check_rgba[5 * 4] = {
      0, 255, 255, 255, 255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 128,
  };
  const uint8_t check_argb[5 * 4] = {
      255, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0, 128,
  };

  PremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                  sizeof(in_bgra), SurfaceFormat::B8G8R8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  PremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                  sizeof(in_bgra), SurfaceFormat::R8G8B8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  PremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                  sizeof(in_bgra), SurfaceFormat::A8R8G8B8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_argb));
}

TEST(Moz2D, UnpremultiplyData)
{
  const uint8_t in_bgra[5 * 4] = {
      255, 255, 0,   255,              // verify 255 alpha leaves RGB unchanged
      0,   0,   255, 255, 0, 0, 0, 0,  // verify 0 alpha leaves RGB at 0
      0,   0,   0,   64,   // verify 0 RGB stays 0 with non-zero alpha
      128, 0,   0,   128,  // verify that RGB == alpha maps to 255

  };
  uint8_t out[5 * 4];
  const uint8_t check_bgra[5 * 4] = {
      255, 255, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0, 0, 0, 0, 64, 255, 0, 0, 128,
  };
  // check swizzled output
  const uint8_t check_rgba[5 * 4] = {
      0, 255, 255, 255, 255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 255, 128,
  };
  const uint8_t check_argb[5 * 4] = {
      255, 0, 255, 255, 255, 255, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 128, 0, 0, 255,
  };

  UnpremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                    sizeof(in_bgra), SurfaceFormat::B8G8R8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  UnpremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                    sizeof(in_bgra), SurfaceFormat::R8G8B8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  UnpremultiplyData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
                    sizeof(in_bgra), SurfaceFormat::A8R8G8B8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_argb));
}

TEST(Moz2D, SwizzleData)
{
  const uint8_t in_bgra[5 * 4] = {
      253, 254, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0, 1, 2, 3, 64, 127, 0, 9, 128,

  };
  uint8_t out[5 * 4];
  // check copy
  const uint8_t check_bgra[5 * 4] = {
      253, 254, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0, 1, 2, 3, 64, 127, 0, 9, 128,
  };
  // check swaps
  const uint8_t check_rgba[5 * 4] = {
      0, 254, 253, 255, 255, 0, 0, 255, 0, 0, 0, 0, 3, 2, 1, 64, 9, 0, 127, 128,
  };
  const uint8_t check_argb[5 * 4] = {
      255, 0, 254, 253, 255, 255, 0, 0, 0, 0, 0, 0, 64, 3, 2, 1, 128, 9, 0, 127,
  };
  // check opaquifying
  const uint8_t check_rgbx[5 * 4] = {
      0, 254, 253, 255, 255, 0,   0, 255, 0,   0,
      0, 255, 3,   2,   1,   255, 9, 0,   127, 255,
  };
  // check packing
  uint8_t out24[5 * 3];
  const uint8_t check_bgr[5 * 3] = {253, 254, 0, 0, 0,   255, 0, 0,
                                    0,   1,   2, 3, 127, 0,   9};
  const uint8_t check_rgb[5 * 3] = {
      0, 254, 253, 255, 0, 0, 0, 0, 0, 3, 2, 1, 9, 0, 127,
  };
  uint8_t out8[5];
  const uint8_t check_a[5] = {255, 255, 0, 64, 128};
  uint16_t out16[5];
#define PACK_RGB565(b, g, r) \
  (((b & 0xF8) >> 3) | ((g & 0xFC) << 3) | ((r & 0xF8) << 8))
  const uint16_t check_16[5] = {
      PACK_RGB565(253, 254, 0), PACK_RGB565(0, 0, 255), PACK_RGB565(0, 0, 0),
      PACK_RGB565(1, 2, 3),     PACK_RGB565(127, 0, 9),
  };

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
              sizeof(out), SurfaceFormat::B8G8R8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
              sizeof(out), SurfaceFormat::R8G8B8A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
              sizeof(out), SurfaceFormat::A8R8G8B8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_argb));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out,
              sizeof(out), SurfaceFormat::R8G8B8X8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out, check_rgbx));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out24,
              sizeof(out24), SurfaceFormat::B8G8R8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out24, check_bgr));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out24,
              sizeof(out24), SurfaceFormat::R8G8B8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out24, check_rgb));

  SwizzleData(in_bgra, sizeof(in_bgra), SurfaceFormat::B8G8R8A8, out8,
              sizeof(out8), SurfaceFormat::A8, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out8, check_a));

  SwizzleData(SurfaceFormat::A8R8G8B8_UINT32 == SurfaceFormat::A8R8G8B8
                  ? check_argb
                  : check_bgra,
              sizeof(in_bgra), SurfaceFormat::A8R8G8B8_UINT32,
              reinterpret_cast<uint8_t*>(out16), sizeof(out16),
              SurfaceFormat::R5G6B5_UINT16, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out16, check_16));
}
