// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include "base/gfx/jpeg_codec.h"
#include "testing/gtest/include/gtest/gtest.h"

// out of 100, this indicates how compressed it will be, this should be changed
// with jpeg equality threshold
// static int jpeg_quality = 75;  // FIXME(brettw)
static int jpeg_quality = 100;

// The threshold of average color differences where we consider two images
// equal. This number was picked to be a little above the observed difference
// using the above quality.
static double jpeg_equality_threshold = 1.0;

// Computes the average difference between each value in a and b. A and b
// should be the same size. Used to see if two images are approximately equal
// in the presence of compression.
static double AveragePixelDelta(const std::vector<unsigned char>& a,
                                const std::vector<unsigned char>& b) {
  // if the sizes are different, say the average difference is the maximum
  if (a.size() != b.size())
    return 255.0;
  if (a.size() == 0)
    return 0;  // prevent divide by 0 below

  double acc = 0.0;
  for (size_t i = 0; i < a.size(); i++)
    acc += fabs(static_cast<double>(a[i]) - static_cast<double>(b[i]));

  return acc / static_cast<double>(a.size());
}

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

TEST(JPEGCodec, EncodeDecodeRGB) {
  int w = 20, h = 20;

  // create an image with known values
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // encode, making sure it was compressed some
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(JPEGCodec::Encode(&original[0], JPEGCodec::FORMAT_RGB, w, h,
                                w * 3, jpeg_quality, &encoded));
  EXPECT_GT(original.size(), encoded.size());

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(JPEGCodec::Decode(&encoded[0], encoded.size(),
                                JPEGCodec::FORMAT_RGB, &decoded,
                                &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be approximately equal (compression will have introduced some
  // minor artifacts).
  ASSERT_GE(jpeg_equality_threshold, AveragePixelDelta(original, decoded));
}

TEST(JPEGCodec, EncodeDecodeRGBA) {
  int w = 20, h = 20;

  // create an image with known values, a must be opaque because it will be
  // lost during compression
  std::vector<unsigned char> original;
  original.resize(w * h * 4);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      unsigned char* org_px = &original[(y * w + x) * 4];
      org_px[0] = x * 3;      // r
      org_px[1] = x * 3 + 1;  // g
      org_px[2] = x * 3 + 2;  // b
      org_px[3] = 0xFF;       // a (opaque)
    }
  }

  // encode, making sure it was compressed some
  std::vector<unsigned char> encoded;
  EXPECT_TRUE(JPEGCodec::Encode(&original[0], JPEGCodec::FORMAT_RGBA, w, h,
                                w * 4, jpeg_quality, &encoded));
  EXPECT_GT(original.size(), encoded.size());

  // decode, it should have the same size as the original
  std::vector<unsigned char> decoded;
  int outw, outh;
  EXPECT_TRUE(JPEGCodec::Decode(&encoded[0], encoded.size(),
                                JPEGCodec::FORMAT_RGBA, &decoded,
                                &outw, &outh));
  ASSERT_EQ(w, outw);
  ASSERT_EQ(h, outh);
  ASSERT_EQ(original.size(), decoded.size());

  // Images must be approximately equal (compression will have introduced some
  // minor artifacts).
  ASSERT_GE(jpeg_equality_threshold, AveragePixelDelta(original, decoded));
}

// Test that corrupted data decompression causes failures.
TEST(JPEGCodec, DecodeCorrupted) {
  int w = 20, h = 20;

  // some random data (an uncompressed image)
  std::vector<unsigned char> original;
  MakeRGBImage(w, h, &original);

  // it should fail when given non-JPEG compressed data
  std::vector<unsigned char> output;
  int outw, outh;
  ASSERT_FALSE(JPEGCodec::Decode(&original[0], original.size(),
                                 JPEGCodec::FORMAT_RGB, &output,
                                 &outw, &outh));

  // make some compressed data
  std::vector<unsigned char> compressed;
  ASSERT_TRUE(JPEGCodec::Encode(&original[0], JPEGCodec::FORMAT_RGB, w, h,
                                w * 3, jpeg_quality, &compressed));

  // try decompressing a truncated version
  ASSERT_FALSE(JPEGCodec::Decode(&compressed[0], compressed.size() / 2,
                                 JPEGCodec::FORMAT_RGB, &output,
                                 &outw, &outh));

  // corrupt it and try decompressing that
  for (int i = 10; i < 30; i++)
    compressed[i] = i;
  ASSERT_FALSE(JPEGCodec::Decode(&compressed[0], compressed.size(),
                                 JPEGCodec::FORMAT_RGB, &output,
                                 &outw, &outh));
}
