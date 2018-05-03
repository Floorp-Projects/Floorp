// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gdef.h"

#include <limits>
#include <vector>

#include "gpos.h"
#include "gsub.h"
#include "layout.h"
#include "maxp.h"

#ifdef OTS_VARIATIONS
#include "variations.h"
#endif

// GDEF - The Glyph Definition Table
// http://www.microsoft.com/typography/otspec/gdef.htm

namespace {

// The maximum class value in class definition tables.
const uint16_t kMaxClassDefValue = 0xFFFF;
// The maximum class value in the glyph class definision table.
const uint16_t kMaxGlyphClassDefValue = 4;
// The maximum format number of caret value tables.
// We don't support format 3 for now. See the comment in
// ParseLigCaretListTable() for the reason.
const uint16_t kMaxCaretValueFormat = 2;

}  // namespace

namespace ots {

bool OpenTypeGDEF::ParseAttachListTable(const uint8_t *data, size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&glyph_count)) {
    return Error("Failed to read gdef header");
  }
  const unsigned attach_points_end =
      2 * static_cast<unsigned>(glyph_count) + 4;
  if (attach_points_end > std::numeric_limits<uint16_t>::max()) {
    return Error("Bad glyph count in gdef");
  }
  if (offset_coverage == 0 || offset_coverage >= length ||
      offset_coverage < attach_points_end) {
    return Error("Bad coverage offset %d", offset_coverage);
  }
  if (glyph_count > this->m_num_glyphs) {
    return Error("Bad glyph count %u", glyph_count);
  }

  std::vector<uint16_t> attach_points;
  attach_points.resize(glyph_count);
  for (unsigned i = 0; i < glyph_count; ++i) {
    if (!subtable.ReadU16(&attach_points[i])) {
      return Error("Can't read attachment point %d", i);
    }
    if (attach_points[i] >= length ||
        attach_points[i] < attach_points_end) {
      return Error("Bad attachment point %d of %d", i, attach_points[i]);
    }
  }

  // Parse coverage table
  if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage,
                               length - offset_coverage, this->m_num_glyphs)) {
    return Error("Bad coverage table");
  }

  // Parse attach point table
  for (unsigned i = 0; i < attach_points.size(); ++i) {
    subtable.set_offset(attach_points[i]);
    uint16_t point_count = 0;
    if (!subtable.ReadU16(&point_count)) {
      return Error("Can't read point count %d", i);
    }
    if (point_count == 0) {
      return Error("zero point count %d", i);
    }
    uint16_t last_point_index = 0;
    uint16_t point_index = 0;
    for (unsigned j = 0; j < point_count; ++j) {
      if (!subtable.ReadU16(&point_index)) {
        return Error("Can't read point index %d in point %d", j, i);
      }
      // Contour point indeces are in increasing numerical order
      if (last_point_index != 0 && last_point_index >= point_index) {
        return Error("bad contour indeces: %u >= %u",
                    last_point_index, point_index);
      }
      last_point_index = point_index;
    }
  }
  return true;
}

bool OpenTypeGDEF::ParseLigCaretListTable(const uint8_t *data, size_t length) {
  ots::Buffer subtable(data, length);
  uint16_t offset_coverage = 0;
  uint16_t lig_glyph_count = 0;
  if (!subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&lig_glyph_count)) {
    return Error("Can't read caret structure");
  }
  const unsigned lig_glyphs_end =
      2 * static_cast<unsigned>(lig_glyph_count) + 4;
  if (lig_glyphs_end > std::numeric_limits<uint16_t>::max()) {
    return Error("Bad caret structure");
  }
  if (offset_coverage == 0 || offset_coverage >= length ||
      offset_coverage < lig_glyphs_end) {
    return Error("Bad caret coverate offset %d", offset_coverage);
  }
  if (lig_glyph_count > this->m_num_glyphs) {
    return Error("bad ligature glyph count: %u", lig_glyph_count);
  }

  std::vector<uint16_t> lig_glyphs;
  lig_glyphs.resize(lig_glyph_count);
  for (unsigned i = 0; i < lig_glyph_count; ++i) {
    if (!subtable.ReadU16(&lig_glyphs[i])) {
      return Error("Can't read ligature glyph location %d", i);
    }
    if (lig_glyphs[i] >= length || lig_glyphs[i] < lig_glyphs_end) {
      return Error("Bad ligature glyph location %d in glyph %d", lig_glyphs[i], i);
    }
  }

  // Parse coverage table
  if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage,
                               length - offset_coverage, this->m_num_glyphs)) {
    return Error("Can't parse caret coverage table");
  }

  // Parse ligature glyph table
  for (unsigned i = 0; i < lig_glyphs.size(); ++i) {
    subtable.set_offset(lig_glyphs[i]);
    uint16_t caret_count = 0;
    if (!subtable.ReadU16(&caret_count)) {
      return Error("Can't read caret count for glyph %d", i);
    }
    if (caret_count == 0) {
      return Error("bad caret value count: %u", caret_count);
    }

    std::vector<uint16_t> caret_value_offsets;
    caret_value_offsets.resize(caret_count);
    unsigned caret_value_offsets_end = 2 * static_cast<unsigned>(caret_count) + 2;
    for (unsigned j = 0; j < caret_count; ++j) {
      if (!subtable.ReadU16(&caret_value_offsets[j])) {
        return Error("Can't read caret offset %d for glyph %d", j, i);
      }
      if (caret_value_offsets[j] >= length || caret_value_offsets[j] < caret_value_offsets_end) {
        return Error("Bad caret offset %d for caret %d glyph %d", caret_value_offsets[j], j, i);
      }
    }

    // Parse caret values table
    for (unsigned j = 0; j < caret_count; ++j) {
      subtable.set_offset(lig_glyphs[i] + caret_value_offsets[j]);
      uint16_t caret_format = 0;
      if (!subtable.ReadU16(&caret_format)) {
        return Error("Can't read caret values table %d in glyph %d", j, i);
      }
      // TODO(bashi): We only support caret value format 1 and 2 for now
      // because there are no fonts which contain caret value format 3
      // as far as we investigated.
      if (caret_format == 0 || caret_format > kMaxCaretValueFormat) {
        return Error("bad caret value format: %u", caret_format);
      }
      // CaretValueFormats contain a 2-byte field which could be
      // arbitrary value.
      if (!subtable.Skip(2)) {
        return Error("Bad caret value table structure %d in glyph %d", j, i);
      }
    }
  }
  return true;
}

bool OpenTypeGDEF::ParseMarkGlyphSetsDefTable(const uint8_t *data, size_t length) {
  ots::Buffer subtable(data, length);
  uint16_t format = 0;
  uint16_t mark_set_count = 0;
  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&mark_set_count)) {
    return Error("Can' read mark glyph table structure");
  }
  if (format != 1) {
    return Error("bad mark glyph set table format: %u", format);
  }

  const unsigned mark_sets_end = 2 * static_cast<unsigned>(mark_set_count) + 4;
  if (mark_sets_end > std::numeric_limits<uint16_t>::max()) {
    return Error("Bad mark_set %d", mark_sets_end);
  }
  for (unsigned i = 0; i < mark_set_count; ++i) {
    uint32_t offset_coverage = 0;
    if (!subtable.ReadU32(&offset_coverage)) {
      return Error("Can't read covrage location for mark set %d", i);
    }
    if (offset_coverage >= length ||
        offset_coverage < mark_sets_end) {
      return Error("Bad coverage location %d for mark set %d", offset_coverage, i);
    }
    if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage,
                                 length - offset_coverage, this->m_num_glyphs)) {
      return Error("Failed to parse coverage table for mark set %d", i);
    }
  }
  this->num_mark_glyph_sets = mark_set_count;
  return true;
}

bool OpenTypeGDEF::Parse(const uint8_t *data, size_t length) {
  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));

  // Grab the number of glyphs in the font from the maxp table to check
  // GlyphIDs in GDEF table.
  if (!maxp) {
    return Error("No maxp table in font, needed by GDEF");
  }
  this->m_num_glyphs = maxp->num_glyphs;

  Buffer table(data, length);

  uint16_t version_major = 0, version_minor = 0;
  if (!table.ReadU16(&version_major) ||
      !table.ReadU16(&version_minor)) {
    return Error("Incomplete table");
  }
  if (version_major != 1 || version_minor == 1) { // there is no v1.1
    return Error("Bad version");
  }

  uint16_t offset_glyph_class_def = 0;
  uint16_t offset_attach_list = 0;
  uint16_t offset_lig_caret_list = 0;
  uint16_t offset_mark_attach_class_def = 0;
  if (!table.ReadU16(&offset_glyph_class_def) ||
      !table.ReadU16(&offset_attach_list) ||
      !table.ReadU16(&offset_lig_caret_list) ||
      !table.ReadU16(&offset_mark_attach_class_def)) {
    return Error("Incomplete table");
  }
  uint16_t offset_mark_glyph_sets_def = 0;
  if (version_minor >= 2) {
    if (!table.ReadU16(&offset_mark_glyph_sets_def)) {
      return Error("Incomplete table");
    }
  }
  uint32_t item_var_store_offset = 0;
  if (version_minor >= 3) {
    if (!table.ReadU32(&item_var_store_offset)) {
      return Error("Incomplete table");
    }
  }

  unsigned gdef_header_end = 4 + 4 * 2;
  if (version_minor >= 2)
    gdef_header_end += 2;
  if (version_minor >= 3)
    gdef_header_end += 4;

  // Parse subtables
  if (offset_glyph_class_def) {
    if (offset_glyph_class_def >= length ||
        offset_glyph_class_def < gdef_header_end) {
      return Error("Invalid offset to glyph classes");
    }
    if (!ots::ParseClassDefTable(GetFont(), data + offset_glyph_class_def,
                                 length - offset_glyph_class_def,
                                 this->m_num_glyphs, kMaxGlyphClassDefValue)) {
      return Error("Invalid glyph classes");
    }
  }

  if (offset_attach_list) {
    if (offset_attach_list >= length ||
        offset_attach_list < gdef_header_end) {
      return Error("Invalid offset to attachment list");
    }
    if (!ParseAttachListTable(data + offset_attach_list,
                              length - offset_attach_list)) {
      return Error("Invalid attachment list");
    }
  }

  if (offset_lig_caret_list) {
    if (offset_lig_caret_list >= length ||
        offset_lig_caret_list < gdef_header_end) {
      return Error("Invalid offset to ligature caret list");
    }
    if (!ParseLigCaretListTable(data + offset_lig_caret_list,
                                length - offset_lig_caret_list)) {
      return Error("Invalid ligature caret list");
    }
  }

  if (offset_mark_attach_class_def) {
    if (offset_mark_attach_class_def >= length ||
        offset_mark_attach_class_def < gdef_header_end) {
      return Error("Invalid offset to mark attachment list");
    }
    if (!ots::ParseClassDefTable(GetFont(),
                                 data + offset_mark_attach_class_def,
                                 length - offset_mark_attach_class_def,
                                 this->m_num_glyphs, kMaxClassDefValue)) {
      return Error("Invalid mark attachment list");
    }
  }

  if (offset_mark_glyph_sets_def) {
    if (offset_mark_glyph_sets_def >= length ||
        offset_mark_glyph_sets_def < gdef_header_end) {
      return Error("invalid offset to mark glyph sets");
    }
    if (!ParseMarkGlyphSetsDefTable(data + offset_mark_glyph_sets_def,
                                    length - offset_mark_glyph_sets_def)) {
      return Error("Invalid mark glyph sets");
    }
  }

  if (item_var_store_offset) {
    if (item_var_store_offset >= length ||
        item_var_store_offset < gdef_header_end) {
      return Error("invalid offset to item variation store");
    }
#ifdef OTS_VARIATIONS
    if (!ParseItemVariationStore(GetFont(), data + item_var_store_offset,
                                 length - item_var_store_offset)) {
      return Error("Invalid item variation store");
    }
#endif
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeGDEF::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write table");
  }

  return true;
}

}  // namespace ots
