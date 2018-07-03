/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BIT_WRITER_H_
#define BIT_WRITER_H_

#include "mozilla/RefPtr.h"

namespace mozilla
{

class MediaByteBuffer;

class BitWriter
{
public:
  explicit BitWriter(MediaByteBuffer* aBuffer);
  virtual ~BitWriter();
  void WriteBits(uint64_t aValue, size_t aBits);
  void WriteBit(bool aValue) { WriteBits(aValue, 1); }
  void WriteU8(uint8_t aValue) { WriteBits(aValue, 8); }
  void WriteU32(uint32_t aValue) { WriteBits(aValue, 32); }
  void WriteU64(uint64_t aValue) { WriteBits(aValue, 64); }

  // Write unsigned integer into Exp-Golomb-coded. 2^32-2 at most
  void WriteUE(uint32_t aValue);

  // Write RBSP trailing bits.
  void CloseWithRbspTrailing();

  // Return the number of bits written so far;
  size_t BitCount() const { return mPosition * 8 + mBitIndex; }

private:
  RefPtr<MediaByteBuffer> mBuffer;
  size_t mPosition = 0;
  uint8_t mBitIndex = 0;
};

} // namespace mozilla

#endif // BIT_WRITER_H_
