// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OPENTYPE_SANITISER_H_
#define OPENTYPE_SANITISER_H_

#if defined(_WIN32)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#include <winsock2.h>  // for htons/ntohs
#else
#include <arpa/inet.h>
#include <stdint.h>
#endif

#include <algorithm>  // for std::min
#include <cassert>
#include <cstddef>
#include <cstring>

namespace ots {

// -----------------------------------------------------------------------------
// This is an interface for an abstract stream class which is used for writing
// the serialised results out.
// -----------------------------------------------------------------------------
class OTSStream {
 public:
  OTSStream() {
    ResetChecksum();
  }

  virtual ~OTSStream() {}

  // This should be implemented to perform the actual write.
  virtual bool WriteRaw(const void *data, size_t length) = 0;

  bool Write(const void *data, size_t length) {
    if (!length) return false;

    const size_t orig_length = length;
    size_t offset = 0;
    if (chksum_buffer_offset_) {
      const size_t l =
        std::min(length, static_cast<size_t>(4) - chksum_buffer_offset_);
      std::memcpy(chksum_buffer_ + chksum_buffer_offset_, data, l);
      chksum_buffer_offset_ += l;
      offset += l;
      length -= l;
    }

    if (chksum_buffer_offset_ == 4) {
      // TODO(yusukes): This cast breaks the strict-aliasing rule.
      chksum_ += ntohl(*reinterpret_cast<const uint32_t*>(chksum_buffer_));
      chksum_buffer_offset_ = 0;
    }

    while (length >= 4) {
      uint32_t tmp;
      std::memcpy(&tmp, reinterpret_cast<const uint8_t *>(data) + offset,
        sizeof(uint32_t));
      chksum_ += ntohl(tmp);
      length -= 4;
      offset += 4;
    }

    if (length) {
      if (chksum_buffer_offset_ != 0) return false;  // not reached
      if (length > 4) return false;  // not reached
      std::memcpy(chksum_buffer_,
             reinterpret_cast<const uint8_t*>(data) + offset, length);
      chksum_buffer_offset_ = length;
    }

    return WriteRaw(data, orig_length);
  }

  virtual bool Seek(off_t position) = 0;
  virtual off_t Tell() const = 0;

  virtual bool Pad(size_t bytes) {
    static const uint32_t kZero = 0;
    while (bytes >= 4) {
      if (!WriteTag(kZero)) return false;
      bytes -= 4;
    }
    while (bytes) {
      static const uint8_t kZerob = 0;
      if (!Write(&kZerob, 1)) return false;
      bytes--;
    }
    return true;
  }

  bool WriteU8(uint8_t v) {
    return Write(&v, sizeof(v));
  }

  bool WriteU16(uint16_t v) {
    v = htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteS16(int16_t v) {
    v = htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteU24(uint32_t v) {
    v = htonl(v);
    return Write(reinterpret_cast<uint8_t*>(&v)+1, 3);
  }

  bool WriteU32(uint32_t v) {
    v = htonl(v);
    return Write(&v, sizeof(v));
  }

  bool WriteS32(int32_t v) {
    v = htonl(v);
    return Write(&v, sizeof(v));
  }

  bool WriteR64(uint64_t v) {
    return Write(&v, sizeof(v));
  }

  bool WriteTag(uint32_t v) {
    return Write(&v, sizeof(v));
  }

  void ResetChecksum() {
    chksum_ = 0;
    chksum_buffer_offset_ = 0;
  }

  uint32_t chksum() const {
    assert(chksum_buffer_offset_ == 0);
    return chksum_;
  }

  struct ChecksumState {
    uint32_t chksum;
    uint8_t chksum_buffer[4];
    unsigned chksum_buffer_offset;
  };

  ChecksumState SaveChecksumState() const {
    ChecksumState s;
    s.chksum = chksum_;
    s.chksum_buffer_offset = chksum_buffer_offset_;
    std::memcpy(s.chksum_buffer, chksum_buffer_, 4);

    return s;
  }

  void RestoreChecksum(const ChecksumState &s) {
    assert(chksum_buffer_offset_ == 0);
    chksum_ += s.chksum;
    chksum_buffer_offset_ = s.chksum_buffer_offset;
    std::memcpy(chksum_buffer_, s.chksum_buffer, 4);
  }

 protected:
  uint32_t chksum_;
  uint8_t chksum_buffer_[4];
  unsigned chksum_buffer_offset_;
};

// -----------------------------------------------------------------------------
// Process a given OpenType file and write out a sanitised version
//   output: a pointer to an object implementing the OTSStream interface. The
//     sanitisied output will be written to this. In the even of a failure,
//     partial output may have been written.
//   input: the OpenType file
//   length: the size, in bytes, of |input|
// -----------------------------------------------------------------------------
bool Process(OTSStream *output, const uint8_t *input, size_t length);

// Force to disable debug output even when the library is compiled with
// -DOTS_DEBUG.
void DisableDebugOutput();

}  // namespace ots

#endif  // OPENTYPE_SANITISER_H_
