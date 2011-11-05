// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_H_
#define OTS_H_

#include <stddef.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "opentype-sanitiser.h"

namespace ots {

#if defined(_MSC_VER) || !defined(OTS_DEBUG)
#define OTS_FAILURE() false
#else
#define OTS_FAILURE() ots::Failure(__FILE__, __LINE__, __PRETTY_FUNCTION__)
bool Failure(const char *f, int l, const char *fn);
#endif

#if defined(_MSC_VER)
// MSVC supports C99 style variadic macros.
#define OTS_WARNING(format, ...)
#else
// GCC
#if defined(OTS_DEBUG)
#define OTS_WARNING(format, args...) \
    ots::Warning(__FILE__, __LINE__, format, ##args)
void Warning(const char *f, int l, const char *format, ...)
     __attribute__((format(printf, 3, 4)));
#else
#define OTS_WARNING(format, args...)
#endif
#endif

// Define OTS_NO_TRANSCODE_HINTS (i.e., g++ -DOTS_NO_TRANSCODE_HINTS) if you
// want to omit TrueType hinting instructions and variables in glyf, fpgm, prep,
// and cvt tables.
#if defined(OTS_NO_TRANSCODE_HINTS)
const bool g_transcode_hints = false;
#else
const bool g_transcode_hints = true;
#endif

// -----------------------------------------------------------------------------
// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
// -----------------------------------------------------------------------------
class Buffer {
 public:
  Buffer(const uint8_t *buffer, size_t len)
      : buffer_(buffer),
        length_(len),
        offset_(0) { }

  bool Skip(size_t n_bytes) {
    return Read(NULL, n_bytes);
  }

  bool Read(uint8_t *buffer, size_t n_bytes) {
    if (n_bytes > 1024 * 1024 * 1024) {
      return OTS_FAILURE();
    }
    if ((offset_ + n_bytes > length_) ||
        (offset_ > length_ - n_bytes)) {
      return OTS_FAILURE();
    }
    if (buffer) {
      std::memcpy(buffer, buffer_ + offset_, n_bytes);
    }
    offset_ += n_bytes;
    return true;
  }

  inline bool ReadU8(uint8_t *value) {
    if (offset_ + 1 > length_) {
      return OTS_FAILURE();
    }
    *value = buffer_[offset_];
    ++offset_;
    return true;
  }

  bool ReadU16(uint16_t *value) {
    if (offset_ + 2 > length_) {
      return OTS_FAILURE();
    }
    std::memcpy(value, buffer_ + offset_, sizeof(uint16_t));
    *value = ntohs(*value);
    offset_ += 2;
    return true;
  }

  bool ReadS16(int16_t *value) {
    return ReadU16(reinterpret_cast<uint16_t*>(value));
  }

  bool ReadU24(uint32_t *value) {
    if (offset_ + 3 > length_) {
      return OTS_FAILURE();
    }
    *value = static_cast<uint32_t>(buffer_[offset_]) << 16 |
        static_cast<uint32_t>(buffer_[offset_ + 1]) << 8 |
        static_cast<uint32_t>(buffer_[offset_ + 2]);
    offset_ += 3;
    return true;
  }

  bool ReadU32(uint32_t *value) {
    if (offset_ + 4 > length_) {
      return OTS_FAILURE();
    }
    std::memcpy(value, buffer_ + offset_, sizeof(uint32_t));
    *value = ntohl(*value);
    offset_ += 4;
    return true;
  }

  bool ReadS32(int32_t *value) {
    return ReadU32(reinterpret_cast<uint32_t*>(value));
  }

  bool ReadTag(uint32_t *value) {
    if (offset_ + 4 > length_) {
      return OTS_FAILURE();
    }
    std::memcpy(value, buffer_ + offset_, sizeof(uint32_t));
    offset_ += 4;
    return true;
  }

  bool ReadR64(uint64_t *value) {
    if (offset_ + 8 > length_) {
      return OTS_FAILURE();
    }
    std::memcpy(value, buffer_ + offset_, sizeof(uint64_t));
    offset_ += 8;
    return true;
  }

  const uint8_t *buffer() const { return buffer_; }
  size_t offset() const { return offset_; }
  size_t length() const { return length_; }

  void set_offset(size_t newoffset) { offset_ = newoffset; }

 private:
  const uint8_t * const buffer_;
  const size_t length_;
  size_t offset_;
};

#define FOR_EACH_TABLE_TYPE \
  F(cff, CFF) \
  F(cmap, CMAP) \
  F(cvt, CVT) \
  F(fpgm, FPGM) \
  F(gasp, GASP) \
  F(gdef, GDEF) \
  F(glyf, GLYF) \
  F(gpos, GPOS) \
  F(gsub, GSUB) \
  F(hdmx, HDMX) \
  F(head, HEAD) \
  F(hhea, HHEA) \
  F(hmtx, HMTX) \
  F(kern, KERN) \
  F(loca, LOCA) \
  F(ltsh, LTSH) \
  F(maxp, MAXP) \
  F(name, NAME) \
  F(os2, OS2) \
  F(post, POST) \
  F(prep, PREP) \
  F(vdmx, VDMX) \
  F(vorg, VORG) \
  F(vhea, VHEA) \
  F(vmtx, VMTX)

#define F(name, capname) struct OpenType##capname;
FOR_EACH_TABLE_TYPE
#undef F

struct OpenTypeFile {
  OpenTypeFile() {
#define F(name, capname) name = NULL;
    FOR_EACH_TABLE_TYPE
#undef F
  }

  uint32_t version;
  uint16_t num_tables;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;

#define F(name, capname) OpenType##capname *name;
FOR_EACH_TABLE_TYPE
#undef F
};

#define F(name, capname) \
bool ots_##name##_parse(OpenTypeFile *f, const uint8_t *d, size_t l); \
bool ots_##name##_should_serialise(OpenTypeFile *f); \
bool ots_##name##_serialise(OTSStream *s, OpenTypeFile *f); \
void ots_##name##_free(OpenTypeFile *f);
// TODO(yusukes): change these function names to follow Chromium coding rule.
FOR_EACH_TABLE_TYPE
#undef F

}  // namespace ots

#endif  // OTS_H_
