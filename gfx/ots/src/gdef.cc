// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gdef.h"

#include <limits>
#include <vector>

#include "gpos.h"
#include "gsub.h"
#include "layout.h"
#include "maxp.h"

// GDEF - The Glyph Definition Table
// http://www.microsoft.com/typography/otspec/gdef.htm

#define TABLE_NAME "GDEF"

namespace {

// The maximum class value in class definition tables.
const uint16_t kMaxClassDefValue = 0xFFFF;
// The maximum class value in the glyph class definision table.
const uint16_t kMaxGlyphClassDefValue = 4;
// The maximum format number of caret value tables.
// We don't support format 3 for now. See the comment in
// ParseLigCaretListTable() for the reason.
const uint16_t kMaxCaretValueFormat = 2;

bool ParseGlyphClassDefTable(ots::OpenTypeFile *file, const uint8_t *data,
                             size_t length, const uint16_t num_glyphs) {
  return ots::ParseClassDefTable(data, length, num_glyphs,
                                 kMaxGlyphClassDefValue);
}

bool ParseAttachListTable(ots::OpenTypeFile *file, const uint8_t *data,
                          size_t length, const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  const unsigned attach_points_end =
      2 * static_cast<unsigned>(glyph_count) + 4;
  if (attach_points_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage == 0 || offset_coverage >= length ||
      offset_coverage < attach_points_end) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    OTS_WARNING("bad glyph count: %u", glyph_count);
    return OTS_FAILURE();
  }

  std::vector<uint16_t> attach_points;
  attach_points.resize(glyph_count);
  for (unsigned i = 0; i < glyph_count; ++i) {
    if (!subtable.ReadU16(&attach_points[i])) {
      return OTS_FAILURE();
    }
    if (attach_points[i] >= length ||
        attach_points[i] < attach_points_end) {
      return OTS_FAILURE();
    }
  }

  // Parse coverage table
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  // Parse attach point table
  for (unsigned i = 0; i < attach_points.size(); ++i) {
    subtable.set_offset(attach_points[i]);
    uint16_t point_count = 0;
    if (!subtable.ReadU16(&point_count)) {
      return OTS_FAILURE();
    }
    if (point_count == 0) {
      return OTS_FAILURE();
    }
    uint16_t last_point_index = 0;
    uint16_t point_index = 0;
    for (unsigned j = 0; j < point_count; ++j) {
      if (!subtable.ReadU16(&point_index)) {
        return OTS_FAILURE();
      }
      // Contour point indeces are in increasing numerical order
      if (last_point_index != 0 && last_point_index >= point_index) {
        OTS_WARNING("bad contour indeces: %u >= %u",
                    last_point_index, point_index);
        return OTS_FAILURE();
      }
      last_point_index = point_index;
    }
  }
  return true;
}

bool ParseLigCaretListTable(ots::OpenTypeFile *file, const uint8_t *data,
                            size_t length, const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);
  uint16_t offset_coverage = 0;
  uint16_t lig_glyph_count = 0;
  if (!subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&lig_glyph_count)) {
    return OTS_FAILURE();
  }
  const unsigned lig_glyphs_end =
      2 * static_cast<unsigned>(lig_glyph_count) + 4;
  if (lig_glyphs_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage == 0 || offset_coverage >= length ||
      offset_coverage < lig_glyphs_end) {
    return OTS_FAILURE();
  }
  if (lig_glyph_count > num_glyphs) {
    OTS_WARNING("bad ligature glyph count: %u", lig_glyph_count);
    return OTS_FAILURE();
  }

  std::vector<uint16_t> lig_glyphs;
  lig_glyphs.resize(lig_glyph_count);
  for (unsigned i = 0; i < lig_glyph_count; ++i) {
    if (!subtable.ReadU16(&lig_glyphs[i])) {
      return OTS_FAILURE();
    }
    if (lig_glyphs[i] >= length || lig_glyphs[i] < lig_glyphs_end) {
      return OTS_FAILURE();
    }
  }

  // Parse coverage table
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  // Parse ligature glyph table
  for (unsigned i = 0; i < lig_glyphs.size(); ++i) {
    subtable.set_offset(lig_glyphs[i]);
    uint16_t caret_count = 0;
    if (!subtable.ReadU16(&caret_count)) {
      return OTS_FAILURE();
    }
    if (caret_count == 0) {
      OTS_WARNING("bad caret value count: %u", caret_count);
      return OTS_FAILURE();
    }

    std::vector<uint16_t> caret_values;
    caret_values.resize(caret_count);
    uint16_t last_offset_caret = 0;
    unsigned caret_values_end = 2 * static_cast<unsigned>(caret_count) + 2;
    for (unsigned j = 0; j < caret_count; ++j) {
      if (!subtable.ReadU16(&caret_values[j])) {
        return OTS_FAILURE();
      }
      if (caret_values[j] >= length || caret_values[j] < caret_values_end) {
        return OTS_FAILURE();
      }
      // Caret offsets are in increasing coordinate order
      if (last_offset_caret != 0 && last_offset_caret >= caret_values[j]) {
        OTS_WARNING("offset isn't in increasing coordinate order: %u >= %u",
                    last_offset_caret, caret_values[j]);
        return OTS_FAILURE();
      }
      last_offset_caret = caret_values[j];
    }

    // Parse caret values table
    for (unsigned j = 0; j < caret_count; ++j) {
      subtable.set_offset(lig_glyphs[i] + caret_values[j]);
      uint16_t caret_format = 0;
      if (!subtable.ReadU16(&caret_format)) {
        return OTS_FAILURE();
      }
      // TODO(bashi): We only support caret value format 1 and 2 for now
      // because there are no fonts which contain caret value format 3
      // as far as we investigated.
      if (caret_format == 0 || caret_format > kMaxCaretValueFormat) {
        OTS_WARNING("bad caret value format: %u", caret_format);
        return OTS_FAILURE();
      }
      // CaretValueFormats contain a 2-byte field which could be
      // arbitrary value.
      if (!subtable.Skip(2)) {
        return OTS_FAILURE();
      }
    }
  }
  return true;
}

bool ParseMarkAttachClassDefTable(ots::OpenTypeFile *file, const uint8_t *data,
                                  size_t length, const uint16_t num_glyphs) {
  return ots::ParseClassDefTable(data, length, num_glyphs, kMaxClassDefValue);
}

bool ParseMarkGlyphSetsDefTable(ots::OpenTypeFile *file, const uint8_t *data,
                                size_t length, const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);
  uint16_t format = 0;
  uint16_t mark_set_count = 0;
  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&mark_set_count)) {
    return OTS_FAILURE();
  }
  if (format != 1) {
    OTS_WARNING("bad mark glyph set table format: %u", format);
    return OTS_FAILURE();
  }

  const unsigned mark_sets_end = 2 * static_cast<unsigned>(mark_set_count) + 4;
  if (mark_sets_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < mark_set_count; ++i) {
    uint32_t offset_coverage = 0;
    if (!subtable.ReadU32(&offset_coverage)) {
      return OTS_FAILURE();
    }
    if (offset_coverage >= length ||
        offset_coverage < mark_sets_end) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offset_coverage,
                                 length - offset_coverage, num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  file->gdef->num_mark_glyph_sets = mark_set_count;
  return true;
}

}  // namespace

#define DROP_THIS_TABLE \
  do { \
    file->gdef->data = 0; \
    file->gdef->length = 0; \
    OTS_FAILURE_MSG("OpenType layout data discarded"); \
  } while (0)

namespace ots {

bool ots_gdef_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  // Grab the number of glyphs in the file from the maxp table to check
  // GlyphIDs in GDEF table.
  if (!file->maxp) {
    return OTS_FAILURE();
  }
  const uint16_t num_glyphs = file->maxp->num_glyphs;

  Buffer table(data, length);

  OpenTypeGDEF *gdef = new OpenTypeGDEF;
  file->gdef = gdef;

  uint32_t version = 0;
  if (!table.ReadU32(&version)) {
    OTS_WARNING("incomplete GDEF table");
    DROP_THIS_TABLE;
    return true;
  }
  if (version < 0x00010000 || version == 0x00010001) {
    OTS_WARNING("bad GDEF version");
    DROP_THIS_TABLE;
    return true;
  }

  if (version >= 0x00010002) {
    gdef->version_2 = true;
  }

  uint16_t offset_glyph_class_def = 0;
  uint16_t offset_attach_list = 0;
  uint16_t offset_lig_caret_list = 0;
  uint16_t offset_mark_attach_class_def = 0;
  if (!table.ReadU16(&offset_glyph_class_def) ||
      !table.ReadU16(&offset_attach_list) ||
      !table.ReadU16(&offset_lig_caret_list) ||
      !table.ReadU16(&offset_mark_attach_class_def)) {
    OTS_WARNING("incomplete GDEF table");
    DROP_THIS_TABLE;
    return true;
  }
  uint16_t offset_mark_glyph_sets_def = 0;
  if (gdef->version_2) {
    if (!table.ReadU16(&offset_mark_glyph_sets_def)) {
      OTS_WARNING("incomplete GDEF table");
      DROP_THIS_TABLE;
      return true;
    }
  }

  unsigned gdef_header_end = 8;
  if (gdef->version_2)
    gdef_header_end += 2;

  // Parse subtables
  if (offset_glyph_class_def) {
    if (offset_glyph_class_def >= length ||
        offset_glyph_class_def < gdef_header_end) {
      OTS_WARNING("invalid offset to glyph classes");
      DROP_THIS_TABLE;
      return true;
    }
    if (!ParseGlyphClassDefTable(file, data + offset_glyph_class_def,
                                 length - offset_glyph_class_def,
                                 num_glyphs)) {
      OTS_WARNING("invalid glyph classes");
      DROP_THIS_TABLE;
      return true;
    }
    gdef->has_glyph_class_def = true;
  }

  if (offset_attach_list) {
    if (offset_attach_list >= length ||
        offset_attach_list < gdef_header_end) {
      OTS_WARNING("invalid offset to attachment list");
      DROP_THIS_TABLE;
      return true;
    }
    if (!ParseAttachListTable(file, data + offset_attach_list,
                              length - offset_attach_list,
                              num_glyphs)) {
      OTS_WARNING("invalid attachment list");
      DROP_THIS_TABLE;
      return true;
    }
  }

  if (offset_lig_caret_list) {
    if (offset_lig_caret_list >= length ||
        offset_lig_caret_list < gdef_header_end) {
      OTS_WARNING("invalid offset to lig-caret list");
      DROP_THIS_TABLE;
      return true;
    }
    if (!ParseLigCaretListTable(file, data + offset_lig_caret_list,
                              length - offset_lig_caret_list,
                              num_glyphs)) {
      OTS_WARNING("invalid ligature caret list");
      DROP_THIS_TABLE;
      return true;
    }
  }

  if (offset_mark_attach_class_def) {
    if (offset_mark_attach_class_def >= length ||
        offset_mark_attach_class_def < gdef_header_end) {
      OTS_WARNING("invalid offset to mark attachment list");
      return OTS_FAILURE();
    }
    if (!ParseMarkAttachClassDefTable(file,
                                      data + offset_mark_attach_class_def,
                                      length - offset_mark_attach_class_def,
                                      num_glyphs)) {
      OTS_WARNING("invalid mark attachment list");
      DROP_THIS_TABLE;
      return true;
    }
    gdef->has_mark_attachment_class_def = true;
  }

  if (offset_mark_glyph_sets_def) {
    if (offset_mark_glyph_sets_def >= length ||
        offset_mark_glyph_sets_def < gdef_header_end) {
      OTS_WARNING("invalid offset to mark glyph sets");
      return OTS_FAILURE();
    }
    if (!ParseMarkGlyphSetsDefTable(file,
                                    data + offset_mark_glyph_sets_def,
                                    length - offset_mark_glyph_sets_def,
                                    num_glyphs)) {
      OTS_WARNING("invalid mark glyph sets");
      DROP_THIS_TABLE;
      return true;
    }
    gdef->has_mark_glyph_sets_def = true;
  }
  gdef->data = data;
  gdef->length = length;
  return true;
}

bool ots_gdef_should_serialise(OpenTypeFile *file) {
  const bool needed_tables_dropped =
      (file->gsub && file->gsub->data == NULL) ||
      (file->gpos && file->gpos->data == NULL);
  return file->gdef != NULL && file->gdef->data != NULL &&
      !needed_tables_dropped;
}

bool ots_gdef_serialise(OTSStream *out, OpenTypeFile *file) {
  if (!out->Write(file->gdef->data, file->gdef->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_gdef_free(OpenTypeFile *file) {
  delete file->gdef;
}

}  // namespace ots

