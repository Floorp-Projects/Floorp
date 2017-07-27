// Copyright (c) 2014-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_MATH_H_
#define OTS_MATH_H_

#include "ots.h"

namespace ots {

class OpenTypeMATH : public Table {
 public:
  explicit OpenTypeMATH(Font *font, uint32_t tag)
      : Table(font, tag, tag),
        m_data(NULL),
        m_length(0) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  bool ParseMathValueRecord(ots::Buffer* subtable,
                            const uint8_t *data,
                            const size_t length);
  bool ParseMathConstantsTable(const uint8_t *data, size_t length);
  bool ParseMathValueRecordSequenceForGlyphs(ots::Buffer* subtable,
                                             const uint8_t *data,
                                             const size_t length,
                                             const uint16_t num_glyphs);
  bool ParseMathItalicsCorrectionInfoTable(const uint8_t *data,
                                           size_t length,
                                           const uint16_t num_glyphs);
  bool ParseMathTopAccentAttachmentTable(const uint8_t *data,
                                         size_t length,
                                         const uint16_t num_glyphs);
  bool ParseMathKernTable(const uint8_t *data, size_t length);
  bool ParseMathKernInfoTable(const uint8_t *data,
                              size_t length,
                              const uint16_t num_glyphs);
  bool ParseMathGlyphInfoTable(const uint8_t *data,
                               size_t length,
                               const uint16_t num_glyphs);
  bool ParseGlyphAssemblyTable(const uint8_t *data,
                               size_t length,
                               const uint16_t num_glyphs);
  bool ParseMathGlyphConstructionTable(const uint8_t *data,
                                       size_t length,
                                       const uint16_t num_glyphs);
  bool ParseMathGlyphConstructionSequence(ots::Buffer* subtable,
                                          const uint8_t *data,
                                          size_t length,
                                          const uint16_t num_glyphs,
                                          uint16_t offset_coverage,
                                          uint16_t glyph_count,
                                          const unsigned sequence_end);
  bool ParseMathVariantsTable(const uint8_t *data,
                              size_t length,
                              const uint16_t num_glyphs);

  const uint8_t *m_data;
  size_t m_length;
};

}  // namespace ots

#endif

