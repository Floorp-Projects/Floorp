// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gsub.h"

#include <limits>
#include <vector>

#include "gdef.h"
#include "gpos.h"
#include "layout.h"
#include "maxp.h"

// GSUB - The Glyph Substitution Table
// http://www.microsoft.com/typography/otspec/gsub.htm

namespace {

// The GSUB header size
const size_t kGsubHeaderSize = 8;

enum GSUB_TYPE {
  GSUB_TYPE_SINGLE = 1,
  GSUB_TYPE_MULTIPLE = 2,
  GSUB_TYPE_ALTERNATE = 3,
  GSUB_TYPE_LIGATURE = 4,
  GSUB_TYPE_CONTEXT = 5,
  GSUB_TYPE_CHANGING_CONTEXT = 6,
  GSUB_TYPE_EXTENSION_SUBSTITUTION = 7,
  GSUB_TYPE_REVERSE_CHAINING_CONTEXT_SINGLE = 8,
  GSUB_TYPE_RESERVED = 9
};

// Lookup type parsers.
bool ParseSingleSubstitution(const ots::OpenTypeFile *file,
                             const uint8_t *data, const size_t length);
bool ParseMutipleSubstitution(const ots::OpenTypeFile *file,
                              const uint8_t *data, const size_t length);
bool ParseAlternateSubstitution(const ots::OpenTypeFile *file,
                                const uint8_t *data, const size_t length);
bool ParseLigatureSubstitution(const ots::OpenTypeFile *file,
      const uint8_t *data, const size_t length);
bool ParseContextSubstitution(const ots::OpenTypeFile *file,
                              const uint8_t *data, const size_t length);
bool ParseChainingContextSubstitution(const ots::OpenTypeFile *file,
                                      const uint8_t *data,
                                      const size_t length);
bool ParseExtensionSubstitution(const ots::OpenTypeFile *file,
                                const uint8_t *data, const size_t length);
bool ParseReverseChainingContextSingleSubstitution(
    const ots::OpenTypeFile *file, const uint8_t *data, const size_t length);

const ots::LookupSubtableParser::TypeParser kGsubTypeParsers[] = {
  {GSUB_TYPE_SINGLE, ParseSingleSubstitution},
  {GSUB_TYPE_MULTIPLE, ParseMutipleSubstitution},
  {GSUB_TYPE_ALTERNATE, ParseAlternateSubstitution},
  {GSUB_TYPE_LIGATURE, ParseLigatureSubstitution},
  {GSUB_TYPE_CONTEXT, ParseContextSubstitution},
  {GSUB_TYPE_CHANGING_CONTEXT, ParseChainingContextSubstitution},
  {GSUB_TYPE_EXTENSION_SUBSTITUTION, ParseExtensionSubstitution},
  {GSUB_TYPE_REVERSE_CHAINING_CONTEXT_SINGLE,
    ParseReverseChainingContextSingleSubstitution}
};

const ots::LookupSubtableParser kGsubLookupSubtableParser = {
  GSUB_TYPE_RESERVED, GSUB_TYPE_EXTENSION_SUBSTITUTION, kGsubTypeParsers
};

// Lookup Type 1:
// Single Substitution Subtable
bool ParseSingleSubstitution(const ots::OpenTypeFile *file,
                             const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage)) {
    return OTS_FAILURE();
  }

  const uint16_t num_glyphs = file->maxp->num_glyphs;
  if (format == 1) {
    // Parse SingleSubstFormat1
    int16_t delta_glyph_id = 0;
    if (!subtable.ReadS16(&delta_glyph_id)) {
      return OTS_FAILURE();
    }
    if (std::abs(delta_glyph_id) >= num_glyphs) {
      return OTS_FAILURE();
    }
  } else if (format == 2) {
    // Parse SingleSubstFormat2
    uint16_t glyph_count = 0;
    if (!subtable.ReadU16(&glyph_count)) {
      return OTS_FAILURE();
    }
    if (glyph_count > num_glyphs) {
      return OTS_FAILURE();
    }
    for (unsigned i = 0; i < glyph_count; ++i) {
      uint16_t substitute = 0;
      if (!subtable.ReadU16(&substitute)) {
        return OTS_FAILURE();
      }
      if (substitute >= num_glyphs) {
        OTS_WARNING("too large substitute: %u", substitute);
        return OTS_FAILURE();
      }
    }
  } else {
    return OTS_FAILURE();
  }

  if (offset_coverage < subtable.offset() || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseSequenceTable(const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t substitute = 0;
    if (!subtable.ReadU16(&substitute)) {
      return OTS_FAILURE();
    }
    if (substitute >= num_glyphs) {
      return OTS_FAILURE();
    }
  }

  return true;
}

// Lookup Type 2:
// Multiple Substitution Subtable
bool ParseMutipleSubstitution(const ots::OpenTypeFile *file,
                              const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t sequence_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&sequence_count)) {
    return OTS_FAILURE();
  }

  if (format != 1) {
    return OTS_FAILURE();
  }

  const uint16_t num_glyphs = file->maxp->num_glyphs;
  const unsigned sequence_end = static_cast<unsigned>(6) +
      sequence_count * 2;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < sequence_count; ++i) {
    uint16_t offset_sequence = 0;
    if (!subtable.ReadU16(&offset_sequence)) {
      return OTS_FAILURE();
    }
    if (offset_sequence < sequence_end || offset_sequence >= length) {
      return OTS_FAILURE();
    }
    if (!ParseSequenceTable(data + offset_sequence, length - offset_sequence,
                            num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  if (offset_coverage < sequence_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseAlternateSetTable(const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t alternate = 0;
    if (!subtable.ReadU16(&alternate)) {
      return OTS_FAILURE();
    }
    if (alternate >= num_glyphs) {
      OTS_WARNING("too arge alternate: %u", alternate);
      return OTS_FAILURE();
    }
  }
  return true;
}

// Lookup Type 3:
// Alternate Substitution Subtable
bool ParseAlternateSubstitution(const ots::OpenTypeFile *file,
                                const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t alternate_set_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&alternate_set_count)) {
    return OTS_FAILURE();
  }

  if (format != 1) {
    return OTS_FAILURE();
  }

  const uint16_t num_glyphs = file->maxp->num_glyphs;
  const unsigned alternate_set_end = static_cast<unsigned>(6) +
      alternate_set_count * 2;
  if (alternate_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < alternate_set_count; ++i) {
    uint16_t offset_alternate_set = 0;
    if (!subtable.ReadU16(&offset_alternate_set)) {
      return OTS_FAILURE();
    }
    if (offset_alternate_set < alternate_set_end ||
        offset_alternate_set >= length) {
      return OTS_FAILURE();
    }
    if (!ParseAlternateSetTable(data + offset_alternate_set,
                                length - offset_alternate_set,
                                num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  if (offset_coverage < alternate_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseLigatureTable(const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t lig_glyph = 0;
  uint16_t comp_count = 0;

  if (!subtable.ReadU16(&lig_glyph) ||
      !subtable.ReadU16(&comp_count)) {
    return OTS_FAILURE();
  }

  if (lig_glyph >= num_glyphs) {
    OTS_WARNING("too large lig_glyph: %u", lig_glyph);
    return OTS_FAILURE();
  }
  if (comp_count == 0 || comp_count > num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < comp_count - static_cast<unsigned>(1); ++i) {
    uint16_t component = 0;
    if (!subtable.ReadU16(&component)) {
      return OTS_FAILURE();
    }
    if (component >= num_glyphs) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseLigatureSetTable(const uint8_t *data, const size_t length,
                           const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t ligature_count = 0;

  if (!subtable.ReadU16(&ligature_count)) {
    return OTS_FAILURE();
  }

  const unsigned ligature_end = static_cast<unsigned>(2) + ligature_count * 2;
  if (ligature_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < ligature_count; ++i) {
    uint16_t offset_ligature = 0;
    if (!subtable.ReadU16(&offset_ligature)) {
      return OTS_FAILURE();
    }
    if (offset_ligature < ligature_end || offset_ligature >= length) {
      return OTS_FAILURE();
    }
    if (!ParseLigatureTable(data + offset_ligature, length - offset_ligature,
                            num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

// Lookup Type 4:
// Ligature Substitution Subtable
bool ParseLigatureSubstitution(const ots::OpenTypeFile *file,
                               const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t lig_set_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&lig_set_count)) {
    return OTS_FAILURE();
  }

  if (format != 1) {
    return OTS_FAILURE();
  }

  const uint16_t num_glyphs = file->maxp->num_glyphs;
  const unsigned ligature_set_end = static_cast<unsigned>(6) +
      lig_set_count * 2;
  if (ligature_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < lig_set_count; ++i) {
    uint16_t offset_ligature_set = 0;
    if (!subtable.ReadU16(&offset_ligature_set)) {
      return OTS_FAILURE();
    }
    if (offset_ligature_set < ligature_set_end ||
        offset_ligature_set >= length) {
      return OTS_FAILURE();
    }
    if (!ParseLigatureSetTable(data + offset_ligature_set,
                               length - offset_ligature_set, num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  if (offset_coverage < ligature_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  return true;
}

// Lookup Type 5:
// Contextual Substitution Subtable
bool ParseContextSubstitution(const ots::OpenTypeFile *file,
                              const uint8_t *data, const size_t length) {
  return ots::ParseContextSubtable(data, length, file->maxp->num_glyphs,
                                   file->gsub->num_lookups);
}

// Lookup Type 6:
// Chaining Contextual Substitution Subtable
bool ParseChainingContextSubstitution(const ots::OpenTypeFile *file,
                                      const uint8_t *data,
                                      const size_t length) {
  return ots::ParseChainingContextSubtable(data, length,
                                           file->maxp->num_glyphs,
                                           file->gsub->num_lookups);
}

// Lookup Type 7:
// Extension Substition
bool ParseExtensionSubstitution(const ots::OpenTypeFile *file,
                                const uint8_t *data, const size_t length) {
  return ots::ParseExtensionSubtable(file, data, length,
                                     &kGsubLookupSubtableParser);
}

// Lookup Type 8:
// Reverse Chaining Contexual Single Substitution Subtable
bool ParseReverseChainingContextSingleSubstitution(
    const ots::OpenTypeFile *file, const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage)) {
    return OTS_FAILURE();
  }

  const uint16_t num_glyphs = file->maxp->num_glyphs;

  uint16_t backtrack_glyph_count = 0;
  if (!subtable.ReadU16(&backtrack_glyph_count)) {
    return OTS_FAILURE();
  }
  if (backtrack_glyph_count > num_glyphs) {
    return OTS_FAILURE();
  }
  std::vector<uint16_t> offsets_backtrack;
  offsets_backtrack.reserve(backtrack_glyph_count);
  for (unsigned i = 0; i < backtrack_glyph_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    offsets_backtrack.push_back(offset);
  }

  uint16_t lookahead_glyph_count = 0;
  if (!subtable.ReadU16(&lookahead_glyph_count)) {
    return OTS_FAILURE();
  }
  if (lookahead_glyph_count > num_glyphs) {
    return OTS_FAILURE();
  }
  std::vector<uint16_t> offsets_lookahead;
  offsets_lookahead.reserve(lookahead_glyph_count);
  for (unsigned i = 0; i < lookahead_glyph_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    offsets_lookahead.push_back(offset);
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t substitute = 0;
    if (!subtable.ReadU16(&substitute)) {
      return OTS_FAILURE();
    }
    if (substitute >= num_glyphs) {
      return OTS_FAILURE();
    }
  }

  const unsigned substitute_end = static_cast<unsigned>(10) +
      (backtrack_glyph_count + lookahead_glyph_count + glyph_count) * 2;
  if (substitute_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  if (offset_coverage < substitute_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < backtrack_glyph_count; ++i) {
    if (offsets_backtrack[i] < substitute_end ||
        offsets_backtrack[i] >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offsets_backtrack[i],
                                 length - offsets_backtrack[i], num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < lookahead_glyph_count; ++i) {
    if (offsets_lookahead[i] < substitute_end ||
        offsets_lookahead[i] >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offsets_lookahead[i],
                                 length - offsets_lookahead[i], num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

}  // namespace

#define DROP_THIS_TABLE \
  do { file->gsub->data = 0; file->gsub->length = 0; } while (0)

namespace ots {

// As far as I checked, following fonts contain invalid values in GSUB table.
// OTS will drop their GSUB table.
//
// # too large substitute (value is 0xFFFF)
// kaiu.ttf
// mingliub2.ttf
// mingliub1.ttf
// mingliub0.ttf
// GraublauWeb.otf
// GraublauWebBold.otf
//
// # too large alternate (value is 0xFFFF)
// ManchuFont.ttf
//
// # bad offset to lang sys table (NULL offset)
// DejaVuMonoSansBold.ttf
// DejaVuMonoSansBoldOblique.ttf
// DejaVuMonoSansOblique.ttf
// DejaVuSansMono-BoldOblique.ttf
// DejaVuSansMono-Oblique.ttf
// DejaVuSansMono-Bold.ttf
//
// # bad start coverage index
// GenBasBI.ttf
// GenBasI.ttf
// AndBasR.ttf
// GenBkBasI.ttf
// CharisSILR.ttf
// CharisSILBI.ttf
// CharisSILI.ttf
// CharisSILB.ttf
// DoulosSILR.ttf
// CharisSILBI.ttf
// GenBkBasB.ttf
// GenBkBasR.ttf
// GenBkBasBI.ttf
// GenBasB.ttf
// GenBasR.ttf
//
// # glyph range is overlapping
// KacstTitleL.ttf
// KacstDecorative.ttf
// KacstTitle.ttf
// KacstArt.ttf
// KacstPoster.ttf
// KacstQurn.ttf
// KacstDigital.ttf
// KacstBook.ttf
// KacstFarsi.ttf

bool ots_gsub_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  // Parsing gsub table requires |file->maxp->num_glyphs|
  if (!file->maxp) {
    return OTS_FAILURE();
  }

  Buffer table(data, length);

  OpenTypeGSUB *gsub = new OpenTypeGSUB;
  file->gsub = gsub;

  uint32_t version = 0;
  uint16_t offset_script_list = 0;
  uint16_t offset_feature_list = 0;
  uint16_t offset_lookup_list = 0;
  if (!table.ReadU32(&version) ||
      !table.ReadU16(&offset_script_list) ||
      !table.ReadU16(&offset_feature_list) ||
      !table.ReadU16(&offset_lookup_list)) {
    return OTS_FAILURE();
  }

  if (version != 0x00010000) {
    OTS_WARNING("bad GSUB version");
    DROP_THIS_TABLE;
    return true;
  }
  if ((offset_script_list < kGsubHeaderSize ||
       offset_script_list >= length) ||
      (offset_feature_list < kGsubHeaderSize ||
       offset_feature_list >= length) ||
      (offset_lookup_list < kGsubHeaderSize ||
       offset_lookup_list >= length)) {
    OTS_WARNING("bad offset in GSUB header");
    DROP_THIS_TABLE;
    return true;
  }

  if (!ParseLookupListTable(file, data + offset_lookup_list,
                            length - offset_lookup_list,
                            &kGsubLookupSubtableParser,
                            &gsub->num_lookups)) {
    OTS_WARNING("faild to parse lookup list table");
    DROP_THIS_TABLE;
    return true;
  }

  uint16_t num_features = 0;
  if (!ParseFeatureListTable(data + offset_feature_list,
                             length - offset_feature_list, gsub->num_lookups,
                             &num_features)) {
    OTS_WARNING("faild to parse feature list table");
    DROP_THIS_TABLE;
    return true;
  }

  if (!ParseScriptListTable(data + offset_script_list,
                            length - offset_script_list, num_features)) {
    OTS_WARNING("faild to parse script list table");
    DROP_THIS_TABLE;
    return true;
  }

  gsub->data = data;
  gsub->length = length;
  return true;
}

bool ots_gsub_should_serialise(OpenTypeFile *file) {
  const bool needed_tables_dropped =
      (file->gdef && file->gdef->data == NULL) ||
      (file->gpos && file->gpos->data == NULL);
  return file->gsub != NULL && file->gsub->data != NULL
      && !needed_tables_dropped;
}

bool ots_gsub_serialise(OTSStream *out, OpenTypeFile *file) {
  if (!out->Write(file->gsub->data, file->gsub->length)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_gsub_free(OpenTypeFile *file) {
  delete file->gsub;
}

}  // namespace ots

