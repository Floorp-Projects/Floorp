// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_H_
#define OTS_H_

// Not needed in the gecko build
// #include "config.h"

#include <stddef.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>

#include "opentype-sanitiser.h"

// arraysize borrowed from base/basictypes.h
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];
#define arraysize(array) (sizeof(ArraySizeHelper(array)))

namespace ots {

#if !defined(OTS_DEBUG)
#define OTS_FAILURE() false
#else
#define OTS_FAILURE() \
  (\
    std::fprintf(stderr, "ERROR at %s:%d (%s)\n", \
                 __FILE__, __LINE__, __FUNCTION__) \
    && false\
  )
#endif

// All OTS_FAILURE_* macros ultimately evaluate to 'false', just like the original
// message-less OTS_FAILURE(), so that the current parser will return 'false' as
// its result (indicating a failure).

#if !defined(OTS_DEBUG)
#define OTS_MESSAGE_(level,otf_,...) \
  (otf_)->context->Message(level,__VA_ARGS__)
#else
#define OTS_MESSAGE_(level,otf_,...) \
  OTS_FAILURE(), \
  (otf_)->context->Message(level,__VA_ARGS__)
#endif

// Generate a simple message
#define OTS_FAILURE_MSG_(otf_,...) \
  (OTS_MESSAGE_(0,otf_,__VA_ARGS__), false)

#define OTS_WARNING_MSG_(otf_,...) \
  OTS_MESSAGE_(1,otf_,__VA_ARGS__)

// Convenience macros for use in files that only handle a single table tag,
// defined as TABLE_NAME at the top of the file; the 'file' variable is
// expected to be the current FontFile pointer.
#define OTS_FAILURE_MSG(...) OTS_FAILURE_MSG_(font->file, TABLE_NAME ": " __VA_ARGS__)

#define OTS_WARNING(...) OTS_WARNING_MSG_(font->file, TABLE_NAME ": " __VA_ARGS__)

// -----------------------------------------------------------------------------
// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
// -----------------------------------------------------------------------------
class Buffer {
 public:
  Buffer(const uint8_t *buf, size_t len)
      : buffer_(buf),
        length_(len),
        offset_(0) { }

  bool Skip(size_t n_bytes) {
    return Read(NULL, n_bytes);
  }

  bool Read(uint8_t *buf, size_t n_bytes) {
    if (n_bytes > 1024 * 1024 * 1024) {
      return OTS_FAILURE();
    }
    if ((offset_ + n_bytes > length_) ||
        (offset_ > length_ - n_bytes)) {
      return OTS_FAILURE();
    }
    if (buf) {
      std::memcpy(buf, buffer_ + offset_, n_bytes);
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
  size_t remaining() const { return length_ - offset_; }

  void set_offset(size_t newoffset) { offset_ = newoffset; }

 private:
  const uint8_t * const buffer_;
  const size_t length_;
  size_t offset_;
};

// Round a value up to the nearest multiple of 4. Don't round the value in the
// case that rounding up overflows.
template<typename T> T Round4(T value) {
  if (std::numeric_limits<T>::max() - value < 3) {
    return value;
  }
  return (value + 3) & ~3;
}

template<typename T> T Round2(T value) {
  if (value == std::numeric_limits<T>::max()) {
    return value;
  }
  return (value + 1) & ~1;
}

bool IsValidVersionTag(uint32_t tag);

#define OTS_TAG_CFF  OTS_TAG('C','F','F',' ')
#define OTS_TAG_CMAP OTS_TAG('c','m','a','p')
#define OTS_TAG_CVT  OTS_TAG('c','v','t',' ')
#define OTS_TAG_FPGM OTS_TAG('f','p','g','m')
#define OTS_TAG_GASP OTS_TAG('g','a','s','p')
#define OTS_TAG_GDEF OTS_TAG('G','D','E','F')
#define OTS_TAG_GLYF OTS_TAG('g','l','y','f')
#define OTS_TAG_GPOS OTS_TAG('G','P','O','S')
#define OTS_TAG_GSUB OTS_TAG('G','S','U','B')
#define OTS_TAG_HDMX OTS_TAG('h','d','m','x')
#define OTS_TAG_HEAD OTS_TAG('h','e','a','d')
#define OTS_TAG_HHEA OTS_TAG('h','h','e','a')
#define OTS_TAG_HMTX OTS_TAG('h','m','t','x')
#define OTS_TAG_KERN OTS_TAG('k','e','r','n')
#define OTS_TAG_LOCA OTS_TAG('l','o','c','a')
#define OTS_TAG_LTSH OTS_TAG('L','T','S','H')
#define OTS_TAG_MATH OTS_TAG('M','A','T','H')
#define OTS_TAG_MAXP OTS_TAG('m','a','x','p')
#define OTS_TAG_NAME OTS_TAG('n','a','m','e')
#define OTS_TAG_OS2  OTS_TAG('O','S','/','2')
#define OTS_TAG_POST OTS_TAG('p','o','s','t')
#define OTS_TAG_PREP OTS_TAG('p','r','e','p')
#define OTS_TAG_VDMX OTS_TAG('V','D','M','X')
#define OTS_TAG_VHEA OTS_TAG('v','h','e','a')
#define OTS_TAG_VMTX OTS_TAG('v','m','t','x')
#define OTS_TAG_VORG OTS_TAG('V','O','R','G')

struct Font;
struct FontFile;
struct TableEntry;
struct Arena;

class Table {
 public:
  explicit Table(Font *font, uint32_t tag, uint32_t type)
      : m_tag(tag),
        m_type(type),
        m_font(font),
        m_shouldSerialize(true) {
  }

  virtual ~Table() { }

  virtual bool Parse(const uint8_t *data, size_t length) = 0;
  virtual bool Serialize(OTSStream *out) = 0;
  virtual bool ShouldSerialize();

  // Return the tag (table type) this Table was parsed as, to support
  // "poor man's RTTI" so that we know if we can safely down-cast to
  // a specific Table subclass. The m_type field is initialized to the
  // appropriate tag when a subclass is constructed, or to zero for
  // TablePassthru (indicating unparsed data).
  uint32_t Type() { return m_type; }

  Font* GetFont() { return m_font; }

  bool Error(const char *format, ...);
  bool Warning(const char *format, ...);
  bool Drop(const char *format, ...);

 private:
  void Message(int level, const char *format, va_list va);

  uint32_t m_tag;
  uint32_t m_type;
  Font *m_font;
  bool m_shouldSerialize;
};

class TablePassthru : public Table {
 public:
  explicit TablePassthru(Font *font, uint32_t tag)
      : Table(font, tag, 0),
        m_data(NULL),
        m_length(0) {
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  const uint8_t *m_data;
  size_t m_length;
};

struct Font {
  explicit Font(FontFile *f)
      : file(f),
        version(0),
        num_tables(0),
        search_range(0),
        entry_selector(0),
        range_shift(0) {
  }

  bool ParseTable(const TableEntry& tableinfo, const uint8_t* data,
                  Arena &arena);
  Table* GetTable(uint32_t tag) const;

  // This checks that the returned Table is actually of the correct subclass
  // for |tag|, so it can safely be downcast to the corresponding OpenTypeXXXX;
  // if not (i.e. if the table was treated as Passthru), it will return NULL.
  Table* GetTypedTable(uint32_t tag) const;

  FontFile *file;

  uint32_t version;
  uint16_t num_tables;
  uint16_t search_range;
  uint16_t entry_selector;
  uint16_t range_shift;

 private:
  std::map<uint32_t, Table*> m_tables;
};

struct TableEntry {
  uint32_t tag;
  uint32_t offset;
  uint32_t length;
  uint32_t uncompressed_length;
  uint32_t chksum;

  bool operator<(const TableEntry& other) const {
    return tag < other.tag;
  }
};

struct FontFile {
  ~FontFile();

  OTSContext *context;
  std::map<TableEntry, Table*> tables;
  std::map<uint32_t, TableEntry> table_entries;
};

}  // namespace ots

#endif  // OTS_H_
