/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BYTE_WRITER_H_
#define BYTE_WRITER_H_

#include "mozilla/EndianUtils.h"
#include "nsTArray.h"

namespace mp4_demuxer {

class ByteWriter
{
public:
  explicit ByteWriter(nsTArray<uint8_t>& aData)
    : mPtr(aData)
  {
  }
  ~ByteWriter()
  {
  }

  MOZ_MUST_USE bool WriteU8(uint8_t aByte)
  {
    return Write(&aByte, 1);
  }

  MOZ_MUST_USE bool WriteU16(uint16_t aShort)
  {
    uint8_t c[2];
    mozilla::BigEndian::writeUint16(&c[0], aShort);
    return Write(&c[0], 2);
  }

  MOZ_MUST_USE bool WriteU32(uint32_t aLong)
  {
    uint8_t c[4];
    mozilla::BigEndian::writeUint32(&c[0], aLong);
    return Write(&c[0], 4);
  }

  MOZ_MUST_USE bool Write32(int32_t aLong)
  {
    uint8_t c[4];
    mozilla::BigEndian::writeInt32(&c[0], aLong);
    return Write(&c[0], 4);
  }

  MOZ_MUST_USE bool WriteU64(uint64_t aLongLong)
  {
    uint8_t c[8];
    mozilla::BigEndian::writeUint64(&c[0], aLongLong);
    return Write(&c[0], 8);
  }

  MOZ_MUST_USE bool Write64(int64_t aLongLong)
  {
    uint8_t c[8];
    mozilla::BigEndian::writeInt64(&c[0], aLongLong);
    return Write(&c[0], 8);
  }

  MOZ_MUST_USE bool Write(const uint8_t* aSrc, size_t aCount)
  {
    return mPtr.AppendElements(aSrc, aCount, mozilla::fallible);
  }

private:
  nsTArray<uint8_t>& mPtr;
};

} // namespace mp4_demuxer

#endif
