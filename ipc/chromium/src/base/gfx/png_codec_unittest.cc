// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/gfx/png_encoder.h"
#include "base/gfx/png_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

static void MakeRGBImage(int w, int h, std::vector<unsigned char>* dat) {
  dat->resize(w * h * 3);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned char* org_px = &(*dat)[(y * w + x) * 3];
      org_px[0] = x * 3;      // r
      org_px[1] = x * 3 + 1;  // g
      org_px[2] = x * 3 + 2;  // b
    }
  }
}

// Set use_transparency to write data into the alpha channel, otherwise it will
// be filled with 0xff. With the alpha channel stripped, this should yield the
// same image as MakeRGBImage above, so the code below can make reference
// images for conversion testing.
static void MakeRGBAImage(int w, int h, bool use_transparency,
                          std::vector<unsigned char>* dat) {
  dat->resize(w * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned char* org_px = &(*dat)[(y * w + x) * 4];
      org_px[0] = x * 3;      // r
      org_px[1] = x * 3 + 1;  // g
      org_px[2] = x * 3 + 2;  // b
      if (use_transparency)
        org_px[3] = x*3 + 3;  // a
      else
        org_px[3] = 0xFF;     // a (opaque)
    }
  }
}

TEST(PNGCodec, EncodeDecodeRGB) {
  const int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // encode
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGB, w, h,
                               w * 3, false, &encoded));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGB, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be equal
  ASSERT_TRUE(original == decoded);
}

TEST(PNGCodec, EncodeDecodeRGBA) {
  const int w = 20, h = 20;

  // create an image with known values, a must be opaque because it will be
  // lost during encoding
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // encode
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGBA, w, h,
                               w * 4, false, &encoded));

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGBA, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be exactly equal
  ASSERT_TRUE(original == decoded);
}

// Test that corrupted data decompression causes failures.
TEST(PNGCodec, DecodeCorrupted) {
  int w = 20, h = 20;

  // Make some random data (an uncompressed image).
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // It should fail when given non-JPEG compressed data.
  std::vector<unsigned char> output;
  int outw, outh;
  EXPECT_FALSE(PNGDecoder::Decode(&original[0], original.size(),
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));

  // Make some compressed data.
  std::vector<unsigned char> compressed;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_RGB, w, h,
                               w * 3, false, &compressed));

  // Try decompressing a truncated version.
  EXPECT_FALSE(PNGDecoder::Decode(&compressed[0], compressed.size() / 2,
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));

  // Corrupt it and try decompressing that.
  for (int i = 10; i < 30; i++)
    compressed[i] = i;
  EXPECT_FALSE(PNGDecoder::Decode(&compressed[0], compressed.size(),
                                PNGDecoder::FORMAT_RGB, &output,
                                &outw, &outh));
}

TEST(PNGCodec, EncodeDecodeBGRA) {
  const int w = 20, h = 20;

  // Create an image with known values, alpha must be opaque because it will be
  // lost during encoding.
  std::vector<unsigned char> original;
  MakeRGBAImage(w, h, true, &original);

  // Encode.
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original[0], PNGEncoder::FORMAT_BGRA, w, h,
                               w * 4, false, &encoded));

  // Decode, it should have the same size as the original.
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_BGRA, &decoded,
                               &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be exactly equal.
  ASSERT_TRUE(original == decoded);
}

TEST(PNGCodec, StripAddAlpha) {
  const int w = 20, h = 20;

  // These should be the same except one has a 0xff alpha channel.
  std::vector<unsigned char> original_rgb;
  MakeRGBImage(w, h, &original_rgb);
  std::vector<unsigned char> original_rgba;
  MakeRGBAImage(w, h, false, &original_rgba);

  // Encode RGBA data as RGB.
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(PNGEncoder::Encode(&original_rgba[0],
                                 PNGEncoder::FORMAT_RGBA,
                                 w, h,
                                 w * 4, true, &encoded));

  // Decode the RGB to RGBA.
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGBA, &decoded,
                               &outw, &outh));

  // Decoded and reference should be the same (opaque alpha).
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original_rgba.size(), decoded.size());
  ASSERT_TRUE(original_rgba == decoded);

  // Encode RGBA to RGBA.
  EXPECT_TRUE(PNGEncoder::Encode(&original_rgba[0],
                                 PNGEncoder::FORMAT_RGBA,
                                 w, h,
                                 w * 4, false, &encoded));

  // Decode the RGBA to RGB.
  EXPECT_TRUE(PNGDecoder::Decode(&encoded[0], encoded.size(),
                               PNGDecoder::FORMAT_RGB, &decoded,
                               &outw, &outh));

  // It should be the same as our non-alpha-channel reference.
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original_rgb.size(), decoded.size());
  ASSERT_TRUE(original_rgb == decoded);
}
