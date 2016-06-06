/*
 * Copyright 2015, Mozilla Foundation and contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __BigEndian_h__
#define __BigEndian_h__

#include <stdint.h>

namespace mozilla {

class BigEndian {
public:

  static uint32_t readUint32(const void* aPtr) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(aPtr);
    return uint32_t(p[0]) << 24 |
           uint32_t(p[1]) << 16 |
           uint32_t(p[2]) << 8 |
           uint32_t(p[3]);
  }

  static uint16_t readUint16(const void* aPtr) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(aPtr);
    return uint32_t(p[0]) << 8 |
           uint32_t(p[1]);
  }

  static uint64_t readUint64(const void* aPtr) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(aPtr);
    return uint64_t(p[0]) << 56 |
           uint64_t(p[1]) << 48 |
           uint64_t(p[2]) << 40 |
           uint64_t(p[3]) << 32 |
           uint64_t(p[4]) << 24 |
           uint64_t(p[5]) << 16 |
           uint64_t(p[6]) << 8 |
           uint64_t(p[7]);
  }

  static void writeUint64(void* aPtr, uint64_t aValue) {
    uint8_t* p = reinterpret_cast<uint8_t*>(aPtr);
    p[0] = uint8_t(aValue >> 56) & 0xff;
    p[1] = uint8_t(aValue >> 48) & 0xff;
    p[2] = uint8_t(aValue >> 40) & 0xff;
    p[3] = uint8_t(aValue >> 32) & 0xff;
    p[4] = uint8_t(aValue >> 24) & 0xff;
    p[5] = uint8_t(aValue >> 16) & 0xff;
    p[6] = uint8_t(aValue >> 8) & 0xff;
    p[7] = uint8_t(aValue) & 0xff;
  }
};

} // namespace mozilla

#endif // __BigEndian_h__
