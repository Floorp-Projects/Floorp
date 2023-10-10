/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BitWriter.h"

#include <stdint.h>

#include "MediaData.h"
#include "mozilla/MathAlgorithms.h"

namespace mozilla {

constexpr uint8_t golombLen[256] = {
    1,  3,  3,  5,  5,  5,  5,  7,  7,  7,  7,  7,  7,  7,  7,  9,  9,  9,  9,
    9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 17,
};

BitWriter::BitWriter(MediaByteBuffer* aBuffer) : mBuffer(aBuffer) {}

BitWriter::~BitWriter() = default;

void BitWriter::WriteBits(uint64_t aValue, size_t aBits) {
  MOZ_ASSERT(aBits <= sizeof(uint64_t) * 8);

  while (aBits) {
    if (mBitIndex == 0) {
      mBuffer->AppendElement(0);
    }

    const uint8_t clearMask = ~(~0u << (8 - mBitIndex));
    uint8_t mask = 0;

    if (mBitIndex + aBits > 8) {
      // Not enough bits in the current byte to write all the bits
      // required, we'll process what we can and continue with the left over.
      const uint8_t leftOverBits = mBitIndex + aBits - 8;
      const uint64_t leftOver = aValue & (~uint64_t(0) >> (8 - mBitIndex));
      mask = aValue >> leftOverBits;

      mBitIndex = 8;
      aValue = leftOver;
      aBits = leftOverBits;
    } else {
      const uint8_t offset = 8 - mBitIndex - aBits;
      mask = aValue << offset;

      mBitIndex += aBits;
      aBits = 0;
    }

    mBuffer->ElementAt(mPosition) |= mask & clearMask;

    if (mBitIndex == 8) {
      mPosition++;
      mBitIndex = 0;
    }
  }
}

void BitWriter::WriteUE(uint32_t aValue) {
  MOZ_ASSERT(aValue <= (UINT32_MAX - 1));

  if (aValue < 256) {
    WriteBits(aValue + 1, golombLen[aValue]);
  } else {
    const uint32_t e = FloorLog2(aValue + 1);
    WriteBits(aValue + 1, e * 2 + 1);
  }
}

void BitWriter::WriteULEB128(uint64_t aValue) {
  // See https://en.wikipedia.org/wiki/LEB128#Encode_unsigned_integer
  do {
    uint8_t byte = aValue & 0x7F;
    aValue >>= 7;
    WriteBit(aValue != 0);
    WriteBits(byte, 7);
  } while (aValue != 0);
}

void BitWriter::CloseWithRbspTrailing() {
  WriteBit(true);
  WriteBits(0, (8 - mBitIndex) & 7);
}

void BitWriter::AdvanceBytes(uint32_t aByteOffset) {
  MOZ_DIAGNOSTIC_ASSERT(mBitIndex == 0);
  mPosition += aByteOffset;
}

}  // namespace mozilla
