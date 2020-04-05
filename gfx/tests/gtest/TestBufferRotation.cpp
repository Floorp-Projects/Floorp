/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "BufferUnrotate.h"

using mozilla::gfx::BufferUnrotate;

static unsigned char* GenerateBuffer(int bytesPerPixel, int width, int height,
                                     int stride, int xBoundary, int yBoundary) {
  unsigned char* buffer = new unsigned char[stride * height];
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int pos = ((yBoundary + y) % height) * stride +
                ((xBoundary + x) % width) * bytesPerPixel;
      for (int i = 0; i < bytesPerPixel; i++) {
        buffer[pos + i] = (x + y + i * 2) % 256;
      }
    }
  }
  return buffer;
}

static bool CheckBuffer(unsigned char* buffer, int bytesPerPixel, int width,
                        int height, int stride) {
  int xBoundary = 0;
  int yBoundary = 0;
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int pos = ((yBoundary + y) % height) * stride +
                ((xBoundary + x) % width) * bytesPerPixel;
      for (int i = 0; i < bytesPerPixel; i++) {
        if (buffer[pos + i] != (x + y + i * 2) % 256) {
          printf("Buffer differs at %i, %i, is %i\n", x, y,
                 (int)buffer[pos + i]);
          return false;
        }
      }
    }
  }
  return true;
}

TEST(Gfx, BufferUnrotateHorizontal)
{
  const int NUM_OF_TESTS = 8;
  int bytesPerPixelList[2] = {2, 4};
  int width[NUM_OF_TESTS] = {100, 100, 99, 99, 100, 100, 99, 99};
  int height[NUM_OF_TESTS] = {100, 99, 100, 99, 100, 99, 100, 99};
  int xBoundary[NUM_OF_TESTS] = {30, 30, 30, 30, 31, 31, 31, 31};
  int yBoundary[NUM_OF_TESTS] = {0, 0, 0, 0};

  for (int bytesPerId = 0; bytesPerId < 2; bytesPerId++) {
    int bytesPerPixel = bytesPerPixelList[bytesPerId];
    int stride = 256 * bytesPerPixel;
    for (int testId = 0; testId < NUM_OF_TESTS; testId++) {
      unsigned char* buffer =
          GenerateBuffer(bytesPerPixel, width[testId], height[testId], stride,
                         xBoundary[testId], yBoundary[testId]);
      BufferUnrotate(buffer, width[testId] * bytesPerPixel, height[testId],
                     stride, xBoundary[testId] * bytesPerPixel,
                     yBoundary[testId]);

      EXPECT_TRUE(CheckBuffer(buffer, bytesPerPixel, width[testId],
                              height[testId], stride));
      delete[] buffer;
    }
  }
}

TEST(Gfx, BufferUnrotateVertical)
{
  const int NUM_OF_TESTS = 8;
  int bytesPerPixelList[2] = {2, 4};
  int width[NUM_OF_TESTS] = {100, 100, 99, 99, 100, 100, 99, 99};
  int height[NUM_OF_TESTS] = {100, 99, 100, 99, 100, 99, 100, 99};
  int xBoundary[NUM_OF_TESTS] = {0, 0, 0, 0};
  int yBoundary[NUM_OF_TESTS] = {30, 30, 30, 30, 31, 31, 31, 31};

  for (int bytesPerId = 0; bytesPerId < 2; bytesPerId++) {
    int bytesPerPixel = bytesPerPixelList[bytesPerId];
    int stride = 256 * bytesPerPixel;
    for (int testId = 0; testId < NUM_OF_TESTS; testId++) {
      unsigned char* buffer =
          GenerateBuffer(bytesPerPixel, width[testId], height[testId], stride,
                         xBoundary[testId], yBoundary[testId]);
      BufferUnrotate(buffer, width[testId] * bytesPerPixel, height[testId],
                     stride, xBoundary[testId] * bytesPerPixel,
                     yBoundary[testId]);

      EXPECT_TRUE(CheckBuffer(buffer, bytesPerPixel, width[testId],
                              height[testId], stride));
      delete[] buffer;
    }
  }
}

TEST(Gfx, BufferUnrotateBoth)
{
  const int NUM_OF_TESTS = 16;
  int bytesPerPixelList[2] = {2, 4};
  int width[NUM_OF_TESTS] = {100, 100, 99, 99, 100, 100, 99, 99,
                             100, 100, 99, 99, 100, 100, 99, 99};
  int height[NUM_OF_TESTS] = {100, 99, 100, 99, 100, 99, 100, 99,
                              100, 99, 100, 99, 100, 99, 100, 99};
  int xBoundary[NUM_OF_TESTS] = {30, 30, 30, 30, 31, 31, 31, 31,
                                 30, 30, 30, 30, 31, 31, 31, 31};
  int yBoundary[NUM_OF_TESTS] = {30, 30, 30, 30, 30, 30, 30, 30,
                                 31, 31, 31, 31, 31, 31, 31, 31};

  for (int bytesPerId = 0; bytesPerId < 2; bytesPerId++) {
    int bytesPerPixel = bytesPerPixelList[bytesPerId];
    int stride = 256 * bytesPerPixel;
    for (int testId = 0; testId < NUM_OF_TESTS; testId++) {
      unsigned char* buffer =
          GenerateBuffer(bytesPerPixel, width[testId], height[testId], stride,
                         xBoundary[testId], yBoundary[testId]);
      BufferUnrotate(buffer, width[testId] * bytesPerPixel, height[testId],
                     stride, xBoundary[testId] * bytesPerPixel,
                     yBoundary[testId]);

      EXPECT_TRUE(CheckBuffer(buffer, bytesPerPixel, width[testId],
                              height[testId], stride));
      delete[] buffer;
    }
  }
}

TEST(Gfx, BufferUnrotateUneven)
{
  const int NUM_OF_TESTS = 16;
  int bytesPerPixelList[2] = {2, 4};
  int width[NUM_OF_TESTS] = {10,  100, 99, 39, 100, 40, 99, 39,
                             100, 50,  39, 99, 74,  60, 99, 39};
  int height[NUM_OF_TESTS] = {100, 39, 10,  99, 10, 99, 40, 99,
                              73,  39, 100, 39, 67, 99, 84, 99};
  int xBoundary[NUM_OF_TESTS] = {0,  0,  30, 30, 99, 31, 0,  31,
                                 30, 30, 30, 30, 31, 31, 31, 38};
  int yBoundary[NUM_OF_TESTS] = {30, 30, 0,  30, 0,  30, 0,  30,
                                 31, 31, 31, 31, 31, 31, 31, 98};

  for (int bytesPerId = 0; bytesPerId < 2; bytesPerId++) {
    int bytesPerPixel = bytesPerPixelList[bytesPerId];
    int stride = 256 * bytesPerPixel;
    for (int testId = 0; testId < NUM_OF_TESTS; testId++) {
      unsigned char* buffer =
          GenerateBuffer(bytesPerPixel, width[testId], height[testId], stride,
                         xBoundary[testId], yBoundary[testId]);
      BufferUnrotate(buffer, width[testId] * bytesPerPixel, height[testId],
                     stride, xBoundary[testId] * bytesPerPixel,
                     yBoundary[testId]);

      EXPECT_TRUE(CheckBuffer(buffer, bytesPerPixel, width[testId],
                              height[testId], stride));
      delete[] buffer;
    }
  }
}
