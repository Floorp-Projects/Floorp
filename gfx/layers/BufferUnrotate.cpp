/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm> // min & max
#include <cstdlib>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void BufferUnrotate(uint8_t* aBuffer, int aByteWidth, int aHeight,
                    int aByteStride, int aXBoundary, int aYBoundary)
{
  if (aXBoundary != 0) {
    uint8_t* line = new uint8_t[aByteWidth];
    uint32_t smallStart = 0;
    uint32_t smallLen = aXBoundary;
    uint32_t smallDest = aByteWidth - aXBoundary;
    uint32_t largeStart = aXBoundary;
    uint32_t largeLen = aByteWidth - aXBoundary;
    uint32_t largeDest = 0;
    if (aXBoundary > aByteWidth / 2) {
      smallStart = aXBoundary;
      smallLen = aByteWidth - aXBoundary;
      smallDest = 0;
      largeStart = 0;
      largeLen = aXBoundary;
      largeDest = smallLen;
    }

    for (int y = 0; y < aHeight; y++) {
      int yOffset = y * aByteStride;
      memcpy(line, &aBuffer[yOffset + smallStart], smallLen);
      memmove(&aBuffer[yOffset + largeDest], &aBuffer[yOffset + largeStart], largeLen);
      memcpy(&aBuffer[yOffset + smallDest], line, smallLen);
    }

    delete[] line;
  }

  if (aYBoundary != 0) {
    uint32_t smallestHeight = std::min(aHeight - aYBoundary, aYBoundary);
    uint32_t largestHeight = std::max(aHeight - aYBoundary, aYBoundary);
    uint32_t smallOffset = 0;
    uint32_t largeOffset = aYBoundary * aByteStride;
    uint32_t largeDestOffset = 0;
    uint32_t smallDestOffset = largestHeight * aByteStride;
    if (aYBoundary > aHeight / 2) {
      smallOffset = aYBoundary * aByteStride;
      largeOffset = 0;
      largeDestOffset = smallestHeight * aByteStride;
      smallDestOffset = 0;
    }

    uint8_t* smallestSide = new uint8_t[aByteStride * smallestHeight];
    memcpy(smallestSide, &aBuffer[smallOffset], aByteStride * smallestHeight);
    memmove(&aBuffer[largeDestOffset], &aBuffer[largeOffset], aByteStride * largestHeight);
    memcpy(&aBuffer[smallDestOffset], smallestSide, aByteStride * smallestHeight);
    delete[] smallestSide;
  }
}

