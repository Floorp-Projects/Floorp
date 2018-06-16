/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Derived from Stagefright's ABitReader.

#include "BitReader.h"

namespace mozilla
{

BitReader::BitReader(const mozilla::MediaByteBuffer* aBuffer)
  : BitReader(aBuffer->Elements(), aBuffer->Length() * 8)
{
}

BitReader::BitReader(const mozilla::MediaByteBuffer* aBuffer, size_t aBits)
  : BitReader(aBuffer->Elements(), aBits)
{
}

BitReader::BitReader(const uint8_t* aBuffer, size_t aBits)
  : mData(aBuffer)
  , mOriginalBitSize(aBits)
  , mTotalBitsLeft(aBits)
  , mSize((aBits + 7) / 8)
  , mReservoir(0)
  , mNumBitsLeft(0)
{
}

BitReader::~BitReader() { }

uint32_t
BitReader::ReadBits(size_t aNum)
{
  MOZ_ASSERT(aNum <= 32);
  if (mTotalBitsLeft < aNum) {
    NS_ASSERTION(false, "Reading past end of buffer");
    return 0;
  }
  uint32_t result = 0;
  while (aNum > 0) {
    if (mNumBitsLeft == 0) {
      FillReservoir();
    }

    size_t m = aNum;
    if (m > mNumBitsLeft) {
      m = mNumBitsLeft;
    }

    result = (result << m) | (mReservoir >> (32 - m));
    mReservoir <<= m;
    mNumBitsLeft -= m;
    mTotalBitsLeft -= m;

    aNum -= m;
  }

  return result;
}

// Read unsigned integer Exp-Golomb-coded.
uint32_t
BitReader::ReadUE()
{
  uint32_t i = 0;

  while (ReadBit() == 0 && i < 32) {
    i++;
  }
  if (i == 32) {
    // This can happen if the data is invalid, or if it's
    // short, since ReadBit() will return 0 when it runs
    // off the end of the buffer.
    NS_WARNING("Invalid H.264 data");
    return 0;
  }
  uint32_t r = ReadBits(i);
  r += (1 << i) - 1;
  return r;
}

// Read signed integer Exp-Golomb-coded.
int32_t
BitReader::ReadSE()
{
  int32_t r = ReadUE();
  if (r & 1) {
    return (r+1) / 2;
  } else {
    return -r / 2;
  }
}

uint64_t
BitReader::ReadU64()
{
  uint64_t hi = ReadU32();
  uint32_t lo = ReadU32();
  return (hi << 32) | lo;
}

uint64_t
BitReader::ReadUTF8()
{
  int64_t val = ReadBits(8);
  uint32_t top = (val & 0x80) >> 1;

  if ((val & 0xc0) == 0x80 || val >= 0xFE) {
    // error.
    return -1;
  }
  while (val & top) {
    int tmp = ReadBits(8) - 128;
    if (tmp >> 6) {
      // error.
      return -1;
    }
    val = (val << 6) + tmp;
    top <<= 5;
  }
  val &= (top << 1) - 1;
  return val;
}

size_t
BitReader::BitCount() const
{
  return mOriginalBitSize - mTotalBitsLeft;
}

size_t
BitReader::BitsLeft() const
{
  return mTotalBitsLeft;
}

void
BitReader::FillReservoir()
{
  if (mSize == 0) {
    NS_ASSERTION(false, "Attempting to fill reservoir from past end of data");
    return;
  }

  mReservoir = 0;
  size_t i;
  for (i = 0; mSize > 0 && i < 4; i++) {
    mReservoir = (mReservoir << 8) | *mData;
    mData++;
    mSize--;
  }

  mNumBitsLeft = 8 * i;
  mReservoir <<= 32 - mNumBitsLeft;
}

/* static */ uint32_t
BitReader::GetBitLength(const mozilla::MediaByteBuffer* aNAL)
{
  size_t size = aNAL->Length();

  while (size > 0 && aNAL->ElementAt(size - 1) == 0) {
    size--;
  }

  if (!size) {
    return 0;
  }

  if (size > UINT32_MAX / 8) {
    // We can't represent it, we'll use as much as we can.
    return UINT32_MAX;
  }

  uint8_t v = aNAL->ElementAt(size - 1);
  size *= 8;

  // Remove the stop bit and following trailing zeros.
  if (v) {
    // Count the consecutive zero bits (trailing) on the right by binary search.
    // Adapted from Matt Whitlock algorithm to only bother with 8 bits integers.
    uint32_t c;
    if (v & 1) {
      // Special case for odd v (assumed to happen half of the time).
      c = 0;
    } else {
      c = 1;
      if ((v & 0xf) == 0) {
        v >>= 4;
        c += 4;
      }
      if ((v & 0x3) == 0) {
        v >>= 2;
        c += 2;
      }
      c -= v & 0x1;
    }
    size -= c + 1;
  }
  return size;
}

} // namespace mozilla
