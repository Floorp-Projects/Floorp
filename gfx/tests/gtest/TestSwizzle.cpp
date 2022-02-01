/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/gfx/Swizzle.h"
#include "Orientation.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::image;

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

TEST(Moz2D, PremultiplyRow)
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

  SwizzleRowFn func =
      PremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  func = PremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  func = PremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::A8R8G8B8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_argb));
}

TEST(Moz2D, PremultiplyYFlipData)
{
  const uint8_t stride = 2 * 4;
  const uint8_t in_bgra[6 * 4] = {
      255, 255, 0,   255,  // row 1: verify 255 alpha leaves RGB unchanged
      0,   0,   255, 255,
      0,   255, 255, 0,  // row 2: verify 0 alpha zeroes out RGB
      0,   0,   0,   0,
      255, 0,   0,   128,  // row 3: verify that 255 RGB maps to alpha
      255, 255, 255, 128,
  };
  const uint8_t in_bgra_2[4 * 4] = {
      255, 255, 0,   255,  // row 1: verify 255 alpha leaves RGB unchanged
      0,   0,   255, 255,
      0,   255, 255, 0,  // row 2: verify 0 alpha zeroes out RGB
      0,   0,   0,   0,
  };
  const uint8_t in_bgra_3[2 * 4] = {
      255, 0,   0,   128,  // row 1: verify that 255 RGB maps to alpha
      255, 255, 255, 128,
  };
  uint8_t out[6 * 4];
  uint8_t out_2[4 * 4];
  uint8_t out_3[2 * 4];
  const uint8_t check_bgra[6 * 4] = {
      128, 0, 0, 128, 128, 128, 128, 128, 0, 0, 0,   0,
      0,   0, 0, 0,   255, 255, 0,   255, 0, 0, 255, 255,
  };
  const uint8_t check_bgra_2[4 * 4] = {
      0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 0, 255, 0, 0, 255, 255,
  };
  const uint8_t check_bgra_3[2 * 4] = {
      128, 0, 0, 128, 128, 128, 128, 128,
  };
  // check swizzled output
  const uint8_t check_rgba[6 * 4] = {
      0, 0, 128, 128, 128, 128, 128, 128, 0,   0, 0, 0,
      0, 0, 0,   0,   0,   255, 255, 255, 255, 0, 0, 255,
  };

  // Premultiply.
  PremultiplyYFlipData(in_bgra, stride, SurfaceFormat::B8G8R8A8, out, stride,
                       SurfaceFormat::B8G8R8A8, IntSize(2, 3));
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  // Premultiply in-place with middle row.
  memcpy(out, in_bgra, sizeof(out));
  PremultiplyYFlipData(out, stride, SurfaceFormat::B8G8R8A8, out, stride,
                       SurfaceFormat::B8G8R8A8, IntSize(2, 3));
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  // Premultiply in-place without middle row.
  memcpy(out_2, in_bgra_2, sizeof(out_2));
  PremultiplyYFlipData(out_2, stride, SurfaceFormat::B8G8R8A8, out_2, stride,
                       SurfaceFormat::B8G8R8A8, IntSize(2, 2));
  EXPECT_TRUE(ArrayEqual(out_2, check_bgra_2));

  // Premultiply in-place only middle row.
  memcpy(out_3, in_bgra_3, sizeof(out_3));
  PremultiplyYFlipData(out_3, stride, SurfaceFormat::B8G8R8A8, out_3, stride,
                       SurfaceFormat::B8G8R8A8, IntSize(2, 1));
  EXPECT_TRUE(ArrayEqual(out_3, check_bgra_3));

  // Premultiply and swizzle with middle row.
  PremultiplyYFlipData(in_bgra, stride, SurfaceFormat::B8G8R8A8, out, stride,
                       SurfaceFormat::R8G8B8A8, IntSize(2, 3));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));
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

TEST(Moz2D, UnpremultiplyRow)
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

  SwizzleRowFn func =
      UnpremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_bgra));

  func = UnpremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  func = UnpremultiplyRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::A8R8G8B8);
  func(in_bgra, out, 5);
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

  const uint8_t* uint32_argb;

#if MOZ_BIG_ENDIAN()
  EXPECT_EQ(SurfaceFormat::A8R8G8B8_UINT32, SurfaceFormat::A8R8G8B8);
  uint32_argb = check_argb;
#else
  EXPECT_EQ(SurfaceFormat::A8R8G8B8_UINT32, SurfaceFormat::B8G8R8A8);
  uint32_argb = check_bgra;
#endif

  SwizzleData(uint32_argb, sizeof(in_bgra), SurfaceFormat::A8R8G8B8_UINT32,
              reinterpret_cast<uint8_t*>(out16), sizeof(out16),
              SurfaceFormat::R5G6B5_UINT16, IntSize(5, 1));
  EXPECT_TRUE(ArrayEqual(out16, check_16));
}

TEST(Moz2D, SwizzleYFlipData)
{
  const uint8_t stride = 2 * 4;
  const uint8_t in_bgra[6 * 4] = {
      255, 255, 0,   255,                      // row 1
      0,   0,   255, 255, 0,   255, 255, 0,    // row 2
      0,   0,   0,   0,   255, 0,   0,   128,  // row 3
      255, 255, 255, 128,
  };
  const uint8_t in_bgra_2[4 * 4] = {
      255, 255, 0,   255,                  // row 1
      0,   0,   255, 255, 0, 255, 255, 0,  // row 2
      0,   0,   0,   0,
  };
  const uint8_t in_bgra_3[2 * 4] = {
      255, 0,   0,   128,  // row 1
      255, 255, 255, 128,
  };
  uint8_t out[6 * 4];
  uint8_t out_2[4 * 4];
  uint8_t out_3[2 * 4];
  const uint8_t check_rgba[6 * 4] = {
      0, 0, 255, 128, 255, 255, 255, 128, 255, 255, 0, 0,
      0, 0, 0,   0,   0,   255, 255, 255, 255, 0,   0, 255,
  };
  const uint8_t check_rgba_2[4 * 4] = {
      255, 255, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 255,
  };
  const uint8_t check_rgba_3[2 * 4] = {
      0, 0, 255, 128, 255, 255, 255, 128,
  };

  // Swizzle.
  SwizzleYFlipData(in_bgra, stride, SurfaceFormat::B8G8R8A8, out, stride,
                   SurfaceFormat::R8G8B8A8, IntSize(2, 3));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  // Swizzle in-place with middle row.
  memcpy(out, in_bgra, sizeof(out));
  SwizzleYFlipData(out, stride, SurfaceFormat::B8G8R8A8, out, stride,
                   SurfaceFormat::R8G8B8A8, IntSize(2, 3));
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  // Swizzle in-place without middle row.
  memcpy(out_2, in_bgra_2, sizeof(out_2));
  SwizzleYFlipData(out_2, stride, SurfaceFormat::B8G8R8A8, out_2, stride,
                   SurfaceFormat::R8G8B8A8, IntSize(2, 2));
  EXPECT_TRUE(ArrayEqual(out_2, check_rgba_2));

  // Swizzle in-place only middle row.
  memcpy(out_3, in_bgra_3, sizeof(out_3));
  SwizzleYFlipData(out_3, stride, SurfaceFormat::B8G8R8A8, out_3, stride,
                   SurfaceFormat::R8G8B8A8, IntSize(2, 1));
  EXPECT_TRUE(ArrayEqual(out_3, check_rgba_3));
}

TEST(Moz2D, SwizzleRow)
{
  const uint8_t in_bgra[5 * 4] = {
      253, 254, 0, 255, 0, 0, 255, 255, 0, 0, 0, 0, 1, 2, 3, 64, 127, 0, 9, 128,

  };
  uint8_t out[5 * 4];
  // check swaps
  const uint8_t check_rgba[5 * 4] = {
      0, 254, 253, 255, 255, 0, 0, 255, 0, 0, 0, 0, 3, 2, 1, 64, 9, 0, 127, 128,
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
  // check unpacking
  uint8_t out_unpack[16 * 4];
  const uint8_t in_rgb[16 * 3] = {
      0,  254, 253, 255, 0,  0,  0,  0,  0,  3,  2,  1,  9,  0,  127, 4,
      5,  6,   9,   8,   7,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,  20,
      21, 22,  23,  24,  25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,  36,
  };
  const uint8_t check_unpack_rgbx[16 * 4] = {
      0,  254, 253, 255, 255, 0,  0,  255, 0,  0,  0,  255, 3,  2,  1,  255,
      9,  0,   127, 255, 4,   5,  6,  255, 9,  8,  7,  255, 10, 11, 12, 255,
      13, 14,  15,  255, 16,  17, 18, 255, 19, 20, 21, 255, 22, 23, 24, 255,
      25, 26,  27,  255, 28,  29, 30, 255, 31, 32, 33, 255, 34, 35, 36, 255,
  };
  const uint8_t check_unpack_bgrx[16 * 4] = {
      253, 254, 0,  255, 0,  0,  255, 255, 0,  0,  0,  255, 1,  2,  3,  255,
      127, 0,   9,  255, 6,  5,  4,   255, 7,  8,  9,  255, 12, 11, 10, 255,
      15,  14,  13, 255, 18, 17, 16,  255, 21, 20, 19, 255, 24, 23, 22, 255,
      27,  26,  25, 255, 30, 29, 28,  255, 33, 32, 31, 255, 36, 35, 34, 255,
  };
  const uint8_t check_unpack_xrgb[16 * 4] = {
      255, 0,  254, 253, 255, 255, 0,  0,  255, 0,  0,  0,  255, 3,  2,  1,
      255, 9,  0,   127, 255, 4,   5,  6,  255, 9,  8,  7,  255, 10, 11, 12,
      255, 13, 14,  15,  255, 16,  17, 18, 255, 19, 20, 21, 255, 22, 23, 24,
      255, 25, 26,  27,  255, 28,  29, 30, 255, 31, 32, 33, 255, 34, 35, 36,
  };

  SwizzleRowFn func =
      SwizzleRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_rgba));

  func = SwizzleRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8X8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, check_rgbx));

  func = SwizzleRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8A8);
  func(in_bgra, out, 5);
  EXPECT_TRUE(ArrayEqual(out, in_bgra));

  func = SwizzleRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::B8G8R8);
  func(in_bgra, out24, 5);
  EXPECT_TRUE(ArrayEqual(out24, check_bgr));

  func = SwizzleRow(SurfaceFormat::B8G8R8A8, SurfaceFormat::R8G8B8);
  func(in_bgra, out24, 5);
  EXPECT_TRUE(ArrayEqual(out24, check_rgb));

  func = SwizzleRow(SurfaceFormat::R8G8B8, SurfaceFormat::B8G8R8X8);
  func(in_rgb, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_bgrx));

  memset(out_unpack, 0xE5, sizeof(out_unpack));
  memcpy(out_unpack, in_rgb, sizeof(in_rgb));
  func(out_unpack, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_bgrx));

  func = SwizzleRow(SurfaceFormat::R8G8B8, SurfaceFormat::R8G8B8X8);
  func(in_rgb, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_rgbx));

  memset(out_unpack, 0xE5, sizeof(out_unpack));
  memcpy(out_unpack, in_rgb, sizeof(in_rgb));
  func(out_unpack, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_rgbx));

  func = SwizzleRow(SurfaceFormat::R8G8B8, SurfaceFormat::X8R8G8B8);
  func(in_rgb, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_xrgb));

  memset(out_unpack, 0xE5, sizeof(out_unpack));
  memcpy(out_unpack, in_rgb, sizeof(in_rgb));
  func(out_unpack, out_unpack, 16);
  EXPECT_TRUE(ArrayEqual(out_unpack, check_unpack_xrgb));
}

TEST(Moz2D, ReorientRow)
{
  // Input is a 3x4 image.
  const uint8_t in_row0[3 * 4] = {
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
  };
  const uint8_t in_row1[3 * 4] = {
      12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
  };
  const uint8_t in_row2[3 * 4] = {
      24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
  };
  const uint8_t in_row3[3 * 4] = {
      36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  };

  // Output is either a 3x4 image or 4x3 image.
  uint8_t out[3 * 4 * 4];
  IntSize outSize(3, 4);
  IntSize outSizeSwap(4, 3);
  int32_t outStride = 3 * 4;
  int32_t outStrideSwap = 4 * 4;
  IntRect dirty;

  auto func = ReorientRow(Orientation());
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));

  // clang-format off
  const uint8_t check_identity[3 * 4 * 4] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
      12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
      24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
      36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_identity));

  func = ReorientRow(Orientation(Angle::D90));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));

  // clang-format off
  const uint8_t check_d90[3 * 4 * 4] = {
      36, 37, 38, 39, 24, 25, 26, 27, 12, 13, 14, 15, 0,  1,  2,  3,
      40, 41, 42, 43, 28, 29, 30, 31, 16, 17, 18, 19, 4,  5,  6,  7,
      44, 45, 46, 47, 32, 33, 34, 35, 20, 21, 22, 23, 8,  9,  10, 11,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d90));

  func = ReorientRow(Orientation(Angle::D180));
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));

  // clang-format off
  const uint8_t check_d180[3 * 4 * 4] = {
      44, 45, 46, 47, 40, 41, 42, 43, 36, 37, 38, 39,
      32, 33, 34, 35, 28, 29, 30, 31, 24, 25, 26, 27,
      20, 21, 22, 23, 16, 17, 18, 19, 12, 13, 14, 15,
      8,  9,  10, 11, 4,  5,  6,  7,  0,  1,  2,  3,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d180));

  func = ReorientRow(Orientation(Angle::D270));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));

  // clang-format off
  const uint8_t check_d270[3 * 4 * 4] = {
      8,  9,  10, 11, 20, 21, 22, 23, 32, 33, 34, 35, 44, 45, 46, 47,
      4,  5,  6,  7,  16, 17, 18, 19, 28, 29, 30, 31, 40, 41, 42, 43,
      0,  1,  2,  3,  12, 13, 14, 15, 24, 25, 26, 27, 36, 37, 38, 39,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d270));

  func = ReorientRow(Orientation(Angle::D0, Flip::Horizontal));
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));

  // clang-format off
  const uint8_t check_d0_flip[3 * 4 * 4] = {
      8,  9,  10, 11, 4,  5,  6,  7,  0,  1,  2,  3,
      20, 21, 22, 23, 16, 17, 18, 19, 12, 13, 14, 15,
      32, 33, 34, 35, 28, 29, 30, 31, 24, 25, 26, 27,
      44, 45, 46, 47, 40, 41, 42, 43, 36, 37, 38, 39,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d0_flip));

  func = ReorientRow(Orientation(Angle::D90, Flip::Horizontal));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));

  // clang-format off
  const uint8_t check_d90_flip[3 * 4 * 4] = {
      0,  1,  2,  3,  12, 13, 14, 15, 24, 25, 26, 27, 36, 37, 38, 39,
      4,  5,  6,  7,  16, 17, 18, 19, 28, 29, 30, 31, 40, 41, 42, 43,
      8,  9,  10, 11, 20, 21, 22, 23, 32, 33, 34, 35, 44, 45, 46, 47,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d90_flip));

  func = ReorientRow(Orientation(Angle::D180, Flip::Horizontal));
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));

  // clang-format off
  const uint8_t check_d180_flip[3 * 4 * 4] = {
      36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
      24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
      12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d180_flip));

  func = ReorientRow(Orientation(Angle::D270, Flip::Horizontal));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));

  // clang-format off
  const uint8_t check_d270_flip[3 * 4 * 4] = {
      44, 45, 46, 47, 32, 33, 34, 35, 20, 21, 22, 23, 8,  9,  10, 11,
      40, 41, 42, 43, 28, 29, 30, 31, 16, 17, 18, 19, 4,  5,  6,  7,
      36, 37, 38, 39, 24, 25, 26, 27, 12, 13, 14, 15, 0,  1,  2,  3,
  };
  // clang-format on
  EXPECT_TRUE(ArrayEqual(out, check_d270_flip));

  func = ReorientRow(
      Orientation(Angle::D0, Flip::Horizontal, /* aFlipFirst */ true));
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));

  // No rotation, so flipping before and after are the same.
  EXPECT_TRUE(ArrayEqual(out, check_d0_flip));

  func = ReorientRow(
      Orientation(Angle::D90, Flip::Horizontal, /* aFlipFirst */ true));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));

  // Flip, rotate 90 degrees is the same as rotate 270 degrees, flip.
  EXPECT_TRUE(ArrayEqual(out, check_d270_flip));

  func = ReorientRow(
      Orientation(Angle::D180, Flip::Horizontal, /* aFlipFirst */ true));
  dirty = func(in_row0, 0, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 3, 3, 1));
  dirty = func(in_row1, 1, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 2, 3, 1));
  dirty = func(in_row2, 2, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 1, 3, 1));
  dirty = func(in_row3, 3, out, outSize, outStride);
  EXPECT_EQ(dirty, IntRect(0, 0, 3, 1));

  // Flip, rotate 180 degrees is the same as rotate 180 degrees, flip.
  EXPECT_TRUE(ArrayEqual(out, check_d180_flip));

  func = ReorientRow(
      Orientation(Angle::D270, Flip::Horizontal, /* aFlipFirst */ true));
  dirty = func(in_row0, 0, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(0, 0, 1, 3));
  dirty = func(in_row1, 1, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(1, 0, 1, 3));
  dirty = func(in_row2, 2, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(2, 0, 1, 3));
  dirty = func(in_row3, 3, out, outSizeSwap, outStrideSwap);
  EXPECT_EQ(dirty, IntRect(3, 0, 1, 3));

  // Flip, rotate 270 degrees is the same as rotate 90 degrees, flip.
  EXPECT_TRUE(ArrayEqual(out, check_d90_flip));
}
