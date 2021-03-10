// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GLAT_H_
#define OTS_GLAT_H_

#include <vector>

#include "ots.h"
#include "graphite.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGLAT_Basic Interface
// -----------------------------------------------------------------------------

class OpenTypeGLAT_Basic : public Table {
 public:
  explicit OpenTypeGLAT_Basic(Font* font, uint32_t tag)
      : Table(font, tag, tag) { }

  virtual bool Parse(const uint8_t* data, size_t length) = 0;
  virtual bool Serialize(OTSStream* out) = 0;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v1
// -----------------------------------------------------------------------------

class OpenTypeGLAT_v1 : public OpenTypeGLAT_Basic {
 public:
  explicit OpenTypeGLAT_v1(Font* font, uint32_t tag)
      : OpenTypeGLAT_Basic(font, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  struct GlatEntry : public TablePart<OpenTypeGLAT_v1> {
    explicit GlatEntry(OpenTypeGLAT_v1* parent)
        : TablePart<OpenTypeGLAT_v1>(parent) { }
    bool ParsePart(Buffer& table);
    bool SerializePart(OTSStream* out) const;
    uint8_t attNum;
    uint8_t num;
    std::vector<int16_t> attributes;
  };
  uint32_t version;
  std::vector<GlatEntry> entries;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v2
// -----------------------------------------------------------------------------

class OpenTypeGLAT_v2 : public OpenTypeGLAT_Basic {
 public:
  explicit OpenTypeGLAT_v2(Font* font, uint32_t tag)
      : OpenTypeGLAT_Basic(font, tag) { }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
 struct GlatEntry : public TablePart<OpenTypeGLAT_v2> {
   explicit GlatEntry(OpenTypeGLAT_v2* parent)
      : TablePart<OpenTypeGLAT_v2>(parent) { }
   bool ParsePart(Buffer& table);
   bool SerializePart(OTSStream* out) const;
   int16_t attNum;
   int16_t num;
   std::vector<int16_t> attributes;
  };
  uint32_t version;
  std::vector<GlatEntry> entries;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v3
// -----------------------------------------------------------------------------

class OpenTypeGLAT_v3 : public OpenTypeGLAT_Basic {
 public:
  explicit OpenTypeGLAT_v3(Font* font, uint32_t tag)
     : OpenTypeGLAT_Basic(font, tag) { }

  bool Parse(const uint8_t* data, size_t length) {
    return this->Parse(data, length, false);
  }
  bool Serialize(OTSStream* out);

 private:
  bool Parse(const uint8_t* data, size_t length, bool prevent_decompression);
  struct GlyphAttrs : public TablePart<OpenTypeGLAT_v3> {
    explicit GlyphAttrs(OpenTypeGLAT_v3* parent)
      : TablePart<OpenTypeGLAT_v3>(parent), octabox(parent) { }
    bool ParsePart(Buffer& table OTS_UNUSED) { return false; }
    bool ParsePart(Buffer& table, const size_t size);
    bool SerializePart(OTSStream* out) const;
    struct OctaboxMetrics : public TablePart<OpenTypeGLAT_v3> {
      explicit OctaboxMetrics(OpenTypeGLAT_v3* parent)
          : TablePart<OpenTypeGLAT_v3>(parent) { }
      bool ParsePart(Buffer& table);
      bool SerializePart(OTSStream* out) const;
      struct SubboxEntry : public TablePart<OpenTypeGLAT_v3> {
        explicit SubboxEntry(OpenTypeGLAT_v3* parent)
            : TablePart<OpenTypeGLAT_v3>(parent) { }
        bool ParsePart(Buffer& table);
        bool SerializePart(OTSStream* out) const;
        uint8_t left;
        uint8_t right;
        uint8_t bottom;
        uint8_t top;
        uint8_t diag_pos_min;
        uint8_t diag_pos_max;
        uint8_t diag_neg_min;
        uint8_t diag_neg_max;
      };
      uint16_t subbox_bitmap;
      uint8_t diag_neg_min;
      uint8_t diag_neg_max;
      uint8_t diag_pos_min;
      uint8_t diag_pos_max;
      std::vector<SubboxEntry> subboxes;
    };
    struct GlatEntry : public TablePart<OpenTypeGLAT_v3> {
      explicit GlatEntry(OpenTypeGLAT_v3* parent)
          : TablePart<OpenTypeGLAT_v3>(parent) { }
      bool ParsePart(Buffer& table);
      bool SerializePart(OTSStream* out) const;
      int16_t attNum;
      int16_t num;
      std::vector<int16_t> attributes;
     };
    OctaboxMetrics octabox;
    std::vector<GlatEntry> entries;
  };
  uint32_t version;
  uint32_t compHead;  // compression header
  static const uint32_t SCHEME = 0xF8000000;
  static const uint32_t FULL_SIZE = 0x07FFFFFF;
  static const uint32_t RESERVED = 0x07FFFFFE;
  static const uint32_t OCTABOXES = 0x00000001;
  std::vector<GlyphAttrs> entries;
};

// -----------------------------------------------------------------------------
// OpenTypeGLAT
// -----------------------------------------------------------------------------

class OpenTypeGLAT : public Table {
 public:
  explicit OpenTypeGLAT(Font* font, uint32_t tag)
      : Table(font, tag, tag), font(font), tag(tag) { }
  OpenTypeGLAT(const OpenTypeGLAT& other) = delete;
  OpenTypeGLAT& operator=(const OpenTypeGLAT& other) = delete;
  ~OpenTypeGLAT() { delete handler; }

  bool Parse(const uint8_t* data, size_t length);
  bool Serialize(OTSStream* out);

 private:
  Font* font;
  uint32_t tag;
  OpenTypeGLAT_Basic* handler = nullptr;
};

}  // namespace ots

#endif  // OTS_GLAT_H_
