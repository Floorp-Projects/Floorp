/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BYTE_WRITER_H_
#define BYTE_WRITER_H_

#include "mozilla/EndianUtils.h"
#include "nsTArray.h"

namespace mozilla {

template <typename Endianess>
class ByteWriter {
 public:
  explicit ByteWriter(nsTArray<uint8_t>& aData) : mPtr(aData) {}
  ~ByteWriter() = default;

  [[nodiscard]] bool WriteU8(uint8_t aByte) { return Write(&aByte, 1); }

  [[nodiscard]] bool WriteU16(uint16_t aShort) {
    uint8_t c[2];
    Endianess::writeUint16(&c[0], aShort);
    return Write(&c[0], 2);
  }

  [[nodiscard]] bool WriteU32(uint32_t aLong) {
    uint8_t c[4];
    Endianess::writeUint32(&c[0], aLong);
    return Write(&c[0], 4);
  }

  [[nodiscard]] bool Write32(int32_t aLong) {
    uint8_t c[4];
    Endianess::writeInt32(&c[0], aLong);
    return Write(&c[0], 4);
  }

  [[nodiscard]] bool WriteU64(uint64_t aLongLong) {
    uint8_t c[8];
    Endianess::writeUint64(&c[0], aLongLong);
    return Write(&c[0], 8);
  }

  [[nodiscard]] bool Write64(int64_t aLongLong) {
    uint8_t c[8];
    Endianess::writeInt64(&c[0], aLongLong);
    return Write(&c[0], 8);
  }

  [[nodiscard]] bool Write(const uint8_t* aSrc, size_t aCount) {
    return mPtr.AppendElements(aSrc, aCount, mozilla::fallible);
  }

 private:
  nsTArray<uint8_t>& mPtr;
};

}  // namespace mozilla

#endif
