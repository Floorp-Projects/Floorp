// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
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
#define ots_ntohl(x) _byteswap_ulong (x)
#define ots_ntohs(x) _byteswap_ushort (x)
#define ots_htonl(x) _byteswap_ulong (x)
#define ots_htons(x) _byteswap_ushort (x)
#else
#include <arpa/inet.h>
#include <stdint.h>
#define ots_ntohl(x) ntohl (x)
#define ots_ntohs(x) ntohs (x)
#define ots_htonl(x) htonl (x)
#define ots_htons(x) htons (x)
#endif

#include <sys/types.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>

#define OTS_TAG(c1,c2,c3,c4) ((uint32_t)((((uint8_t)(c1))<<24)|(((uint8_t)(c2))<<16)|(((uint8_t)(c3))<<8)|((uint8_t)(c4))))
#define OTS_UNTAG(tag)       ((char)((tag)>>24)), ((char)((tag)>>16)), ((char)((tag)>>8)), ((char)(tag))

#if defined(__GNUC__) && (__GNUC__ >= 4) || (__clang__)
#define OTS_UNUSED __attribute__((unused))
#elif defined(_MSC_VER)
#define OTS_UNUSED __pragma(warning(suppress: 4100 4101))
#else
#define OTS_UNUSED
#endif

namespace ots {

// -----------------------------------------------------------------------------
// This is an interface for an abstract stream class which is used for writing
// the serialised results out.
// -----------------------------------------------------------------------------
class OTSStream {
 public:
  OTSStream() : chksum_(0) {}

  virtual ~OTSStream() {}

  // This should be implemented to perform the actual write.
  virtual bool WriteRaw(const void *data, size_t length) = 0;

  bool Write(const void *data, size_t length) {
    if (!length) return false;

    const size_t orig_length = length;
    size_t offset = 0;

    size_t chksum_offset = Tell() & 3;
    if (chksum_offset) {
      const size_t l = std::min(length, static_cast<size_t>(4) - chksum_offset);
      uint32_t tmp = 0;
      std::memcpy(reinterpret_cast<uint8_t *>(&tmp) + chksum_offset, data, l);
      chksum_ += ots_ntohl(tmp);
      length -= l;
      offset += l;
    }

    while (length >= 4) {
      uint32_t tmp;
      std::memcpy(&tmp, reinterpret_cast<const uint8_t *>(data) + offset,
        sizeof(uint32_t));
      chksum_ += ots_ntohl(tmp);
      length -= 4;
      offset += 4;
    }

    if (length) {
      if (length > 4) return false;  // not reached
      uint32_t tmp = 0;
      std::memcpy(&tmp,
                  reinterpret_cast<const uint8_t*>(data) + offset, length);
      chksum_ += ots_ntohl(tmp);
    }

    return WriteRaw(data, orig_length);
  }

  virtual bool Seek(off_t position) = 0;
  virtual off_t Tell() const = 0;

  virtual bool Pad(size_t bytes) {
    static const uint32_t kZero = 0;
    while (bytes >= 4) {
      if (!Write(&kZero, 4)) return false;
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
    v = ots_htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteS16(int16_t v) {
    v = ots_htons(v);
    return Write(&v, sizeof(v));
  }

  bool WriteU24(uint32_t v) {
    v = ots_htonl(v);
    return Write(reinterpret_cast<uint8_t*>(&v)+1, 3);
  }

  bool WriteU32(uint32_t v) {
    v = ots_htonl(v);
    return Write(&v, sizeof(v));
  }

  bool WriteS32(int32_t v) {
    v = ots_htonl(v);
    return Write(&v, sizeof(v));
  }

  bool WriteR64(uint64_t v) {
    return Write(&v, sizeof(v));
  }

  void ResetChecksum() {
    assert((Tell() & 3) == 0);
    chksum_ = 0;
  }

  uint32_t chksum() const {
    return chksum_;
  }

 protected:
  uint32_t chksum_;
};

#ifdef __GCC__
#define MSGFUNC_FMT_ATTR __attribute__((format(printf, 2, 3)))
#else
#define MSGFUNC_FMT_ATTR
#endif

enum TableAction {
  TABLE_ACTION_DEFAULT,  // Use OTS's default action for that table
  TABLE_ACTION_SANITIZE, // Sanitize the table, potentially dropping it
  TABLE_ACTION_PASSTHRU, // Serialize the table unchanged
  TABLE_ACTION_DROP      // Drop the table
};

class OTS_API OTSContext {
  public:
    OTSContext() {}
    virtual ~OTSContext() {}

    // Process a given OpenType file and write out a sanitized version
    //   output: a pointer to an object implementing the OTSStream interface. The
    //     sanitisied output will be written to this. In the even of a failure,
    //     partial output may have been written.
    //   input: the OpenType file
    //   length: the size, in bytes, of |input|
    //   index: if the input is a font collection and index is specified, then
    //     the corresponding font will be returned, otherwise the whole
    //     collection. Ignored for non-collection fonts.
    bool Process(OTSStream *output, const uint8_t *input, size_t length, uint32_t index = -1);

    // This function will be called when OTS is reporting an error.
    //   level: the severity of the generated message:
    //     0: error messages in case OTS fails to sanitize the font.
    //     1: warning messages about issue OTS fixed in the sanitized font.
    virtual void Message(int level OTS_UNUSED, const char *format OTS_UNUSED, ...) MSGFUNC_FMT_ATTR {}

    // This function will be called when OTS needs to decide what to do for a
    // font table.
    //   tag: table tag formed with OTS_TAG() macro
    virtual TableAction GetTableAction(uint32_t tag OTS_UNUSED) { return ots::TABLE_ACTION_DEFAULT; }
};

}  // namespace ots

#endif  // OPENTYPE_SANITISER_H_
