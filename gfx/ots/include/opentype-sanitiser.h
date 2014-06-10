// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OPENTYPE_SANITISER_H_
#define OPENTYPE_SANITISER_H_

#if defined(_WIN32) || defined(__CYGWIN__)
  #define OTS_DLL_IMPORT __declspec(dllimport)
  #define OTS_DLL_EXPORT __declspec(dllexport)
#else
  #if __GNUC__ >= 4
    #define OTS_DLL_IMPORT __attribute__((visibility ("default")))
    #define OTS_DLL_EXPORT __attribute__((visibility ("default")))
  #endif
#endif

#ifdef OTS_DLL
  #ifdef OTS_DLL_EXPORTS
    #define OTS_API OTS_DLL_EXPORT
  #else
    #define OTS_API OTS_DLL_IMPORT
  #endif
#else
  #define OTS_API
#endif

#if defined(_WIN32)
#include <stdlib.h>
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define ntohl(x) _byteswap_ulong (x)
#define ntohs(x) _byteswap_ushort (x)
#define htonl(x) _byteswap_ulong (x)
#define htons(x) _byteswap_ushort (x)
#else
#include <arpa/inet.h>
#include <stdint.h>
#endif

#include <algorithm>
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
      uint32_t tmp;
      std::memcpy(&tmp, chksum_buffer_, 4);
      chksum_ += ntohl(tmp);
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

// Signature of the function to be provided by the client in order to report errors.
// The return type is a boolean so that it can be used within an expression,
// but the actual value is ignored. (Suggested convention is to always return 'false'.)
#ifdef __GCC__
#define MSGFUNC_FMT_ATTR __attribute__((format(printf, 2, 3)))
#else
#define MSGFUNC_FMT_ATTR
#endif
typedef bool (*MessageFunc)(void *user_data, const char *format, ...)  MSGFUNC_FMT_ATTR;

enum TableAction {
  TABLE_ACTION_DEFAULT,  // Use OTS's default action for that table
  TABLE_ACTION_SANITIZE, // Sanitize the table, potentially droping it
  TABLE_ACTION_PASSTHRU, // Serialize the table unchanged
  TABLE_ACTION_DROP      // Drop the table
};

// Signature of the function to be provided by the client to decide what action
// to do for a given table.
//   tag: table tag as an integer in big-endian byte order, independent of platform endianness
//   user_data: user defined data that are passed to SetTableActionCallback()
typedef TableAction (*TableActionFunc)(uint32_t tag, void *user_data);

class OTS_API OTSContext {
  public:
    OTSContext()
        : message_func(0),
          message_user_data(0),
          table_action_func(0),
          table_action_user_data(0)
        {}

    ~OTSContext() {}

    // Process a given OpenType file and write out a sanitised version
    //   output: a pointer to an object implementing the OTSStream interface. The
    //     sanitisied output will be written to this. In the even of a failure,
    //     partial output may have been written.
    //   input: the OpenType file
    //   length: the size, in bytes, of |input|
    //   context: optional context that holds various OTS settings like user callbacks
    bool Process(OTSStream *output, const uint8_t *input, size_t length);

    // Set a callback function that will be called when OTS is reporting an error.
    void SetMessageCallback(MessageFunc func, void *user_data) {
      message_func = func;
      message_user_data = user_data;
    }

    // Set a callback function that will be called when OTS needs to decide what to
    // do for a font table.
    void SetTableActionCallback(TableActionFunc func, void *user_data) {
      table_action_func = func;
      table_action_user_data = user_data;
    }

  private:
    MessageFunc      message_func;
    void            *message_user_data;
    TableActionFunc  table_action_func;
    void            *table_action_user_data;
};

// Force to disable debug output even when the library is compiled with
// -DOTS_DEBUG.
void DisableDebugOutput();

// Enable WOFF2 support(experimental).
void EnableWOFF2();

}  // namespace ots

#endif  // OPENTYPE_SANITISER_H_
