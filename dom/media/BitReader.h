/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BIT_READER_H_
#define BIT_READER_H_

#include "MediaData.h"

namespace mozilla
{

class BitReader
{
public:
  explicit BitReader(const MediaByteBuffer* aBuffer);
  BitReader(const MediaByteBuffer* aBuffer, size_t aBits);
  BitReader(const uint8_t* aBuffer, size_t aBits);
  ~BitReader();
  uint32_t ReadBits(size_t aNum);
  bool ReadBit() { return ReadBits(1) != 0; }
  uint32_t ReadU32() { return ReadBits(32); }
  uint64_t ReadU64();

  // Read the UTF-8 sequence and convert it to its 64-bit UCS-4 encoded form.
  // Return 0xfffffffffffffff if sequence was invalid.
  uint64_t ReadUTF8();
  // Read unsigned integer Exp-Golomb-coded.
  uint32_t ReadUE();
  // Read signed integer Exp-Golomb-coded.
  int32_t ReadSE();

  // Return the number of bits parsed so far;
  size_t BitCount() const;
  // Return the number of bits left.
  size_t BitsLeft() const;

  // Return RBSP bit length.
  static uint32_t GetBitLength(const MediaByteBuffer* aNAL);

private:
  void FillReservoir();
  const uint8_t* mData;
  const size_t mOriginalBitSize;
  size_t mTotalBitsLeft;
  size_t mSize;           // Size left in bytes
  uint32_t mReservoir;    // Left-aligned bits
  size_t mNumBitsLeft;    // Number of bits left in reservoir.
};

} // namespace mozilla

#endif // BIT_READER_H_
