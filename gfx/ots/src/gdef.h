// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GDEF_H_
#define OTS_GDEF_H_

#include "ots.h"

namespace ots {

class OpenTypeGDEF : public Table {
 public:
  explicit OpenTypeGDEF(Font *font, uint32_t tag)
      : Table(font, tag, tag),
        version_2(false),
        has_glyph_class_def(false),
        has_mark_attachment_class_def(false),
        has_mark_glyph_sets_def(false),
        num_mark_glyph_sets(0),
        m_data(NULL),
        m_length(0),
        m_num_glyphs(0) {
  }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  bool version_2;
  bool has_glyph_class_def;
  bool has_mark_attachment_class_def;
  bool has_mark_glyph_sets_def;
  uint16_t num_mark_glyph_sets;

 private:
  bool ParseAttachListTable(const uint8_t *data, size_t length);
  bool ParseLigCaretListTable(const uint8_t *data, size_t length);
  bool ParseMarkGlyphSetsDefTable(const uint8_t *data, size_t length);

  const uint8_t *m_data;
  size_t m_length;
  uint16_t m_num_glyphs;
};

}  // namespace ots

#endif

