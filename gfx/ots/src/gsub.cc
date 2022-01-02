// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gsub.h"

#include <limits>
#include <vector>

#include "layout.h"
#include "maxp.h"

// GSUB - The Glyph Substitution Table
// http://www.microsoft.com/typography/otspec/gsub.htm

#define TABLE_NAME "GSUB"

namespace {

// The GSUB header size for table version 1.0
const size_t kGsubHeaderSize_1_0 = 4 + 3 * 2;
// GSUB header size v1.1
const size_t kGsubHeaderSize_1_1 = 4 + 3 * 2 + 4;

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
bool ParseSingleSubstitution(const ots::Font *font,
                             const uint8_t *data, const size_t length);
bool ParseMutipleSubstitution(const ots::Font *font,
                              const uint8_t *data, const size_t length);
bool ParseAlternateSubstitution(const ots::Font *font,
                                const uint8_t *data, const size_t length);
bool ParseLigatureSubstitution(const ots::Font *font,
      const uint8_t *data, const size_t length);
bool ParseContextSubstitution(const ots::Font *font,
                              const uint8_t *data, const size_t length);
bool ParseChainingContextSubstitution(const ots::Font *font,
                                      const uint8_t *data,
                                      const size_t length);
bool ParseExtensionSubstitution(const ots::Font *font,
                                const uint8_t *data, const size_t length);
bool ParseReverseChainingContextSingleSubstitution(
    const ots::Font *font, const uint8_t *data, const size_t length);

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
  arraysize(kGsubTypeParsers),
  GSUB_TYPE_EXTENSION_SUBSTITUTION, kGsubTypeParsers
};

// Lookup Type 1:
// Single Substitution Subtable
bool ParseSingleSubstitution(const ots::Font *font,
                             const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage)) {
    return OTS_FAILURE_MSG("Failed to read single subst table header");
  }

  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  if (format == 1) {
    // Parse SingleSubstFormat1
    int16_t delta_glyph_id = 0;
    if (!subtable.ReadS16(&delta_glyph_id)) {
      return OTS_FAILURE_MSG("Failed to read glyph shift from format 1 single subst table");
    }
    if (std::abs(delta_glyph_id) >= num_glyphs) {
      return OTS_FAILURE_MSG("bad glyph shift of %d in format 1 single subst table", delta_glyph_id);
    }
  } else if (format == 2) {
    // Parse SingleSubstFormat2
    uint16_t glyph_count = 0;
    if (!subtable.ReadU16(&glyph_count)) {
      return OTS_FAILURE_MSG("Failed to read glyph cound in format 2 single subst table");
    }
    if (glyph_count > num_glyphs) {
      return OTS_FAILURE_MSG("Bad glyph count %d > %d in format 2 single subst table", glyph_count, num_glyphs);
    }
    for (unsigned i = 0; i < glyph_count; ++i) {
      uint16_t substitute = 0;
      if (!subtable.ReadU16(&substitute)) {
        return OTS_FAILURE_MSG("Failed to read substitution %d in format 2 single subst table", i);
      }
      if (substitute >= num_glyphs) {
        return OTS_FAILURE_MSG("too large substitute: %u", substitute);
      }
    }
  } else {
    return OTS_FAILURE_MSG("Bad single subst table format %d", format);
  }

  if (offset_coverage < subtable.offset() || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %x", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table");
  }

  return true;
}

bool ParseSequenceTable(const ots::Font *font,
                        const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read glyph count in sequence table");
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE_MSG("bad glyph count %d > %d", glyph_count, num_glyphs);
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t substitute = 0;
    if (!subtable.ReadU16(&substitute)) {
      return OTS_FAILURE_MSG("Failed to read substitution %d in sequence table", i);
    }
    if (substitute >= num_glyphs) {
      return OTS_FAILURE_MSG("Bad substitution (%d) %d > %d", i, substitute, num_glyphs);
    }
  }

  return true;
}

// Lookup Type 2:
// Multiple Substitution Subtable
bool ParseMutipleSubstitution(const ots::Font *font,
                              const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t sequence_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&sequence_count)) {
    return OTS_FAILURE_MSG("Can't read header of multiple subst table");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Bad multiple subst table format %d", format);
  }

  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  const unsigned sequence_end = static_cast<unsigned>(6) +
      sequence_count * 2;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad sequence end %d, in multiple subst", sequence_end);
  }
  for (unsigned i = 0; i < sequence_count; ++i) {
    uint16_t offset_sequence = 0;
    if (!subtable.ReadU16(&offset_sequence)) {
      return OTS_FAILURE_MSG("Failed to read sequence offset for sequence %d", i);
    }
    if (offset_sequence < sequence_end || offset_sequence >= length) {
      return OTS_FAILURE_MSG("Bad sequence offset %d for sequence %d", offset_sequence, i);
    }
    if (!ParseSequenceTable(font, data + offset_sequence, length - offset_sequence,
                            num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse sequence table %d", i);
    }
  }

  if (offset_coverage < sequence_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table");
  }

  return true;
}

bool ParseAlternateSetTable(const ots::Font *font,
                            const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read alternate set header");
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE_MSG("Bad glyph count %d > %d in alternate set table", glyph_count, num_glyphs);
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t alternate = 0;
    if (!subtable.ReadU16(&alternate)) {
      return OTS_FAILURE_MSG("Can't read alternate %d", i);
    }
    if (alternate >= num_glyphs) {
      return OTS_FAILURE_MSG("Too large alternate: %u", alternate);
    }
  }
  return true;
}

// Lookup Type 3:
// Alternate Substitution Subtable
bool ParseAlternateSubstitution(const ots::Font *font,
                                const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t alternate_set_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&alternate_set_count)) {
    return OTS_FAILURE_MSG("Can't read alternate subst header");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Bad alternate subst table format %d", format);
  }

  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  const unsigned alternate_set_end = static_cast<unsigned>(6) +
      alternate_set_count * 2;
  if (alternate_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of alternate set %d", alternate_set_end);
  }
  for (unsigned i = 0; i < alternate_set_count; ++i) {
    uint16_t offset_alternate_set = 0;
    if (!subtable.ReadU16(&offset_alternate_set)) {
      return OTS_FAILURE_MSG("Can't read alternate set offset for set %d", i);
    }
    if (offset_alternate_set < alternate_set_end ||
        offset_alternate_set >= length) {
      return OTS_FAILURE_MSG("Bad alternate set offset %d for set %d", offset_alternate_set, i);
    }
    if (!ParseAlternateSetTable(font, data + offset_alternate_set,
                                length - offset_alternate_set,
                                num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse alternate set");
    }
  }

  if (offset_coverage < alternate_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table");
  }

  return true;
}

bool ParseLigatureTable(const ots::Font *font,
                        const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t lig_glyph = 0;
  uint16_t comp_count = 0;

  if (!subtable.ReadU16(&lig_glyph) ||
      !subtable.ReadU16(&comp_count)) {
    return OTS_FAILURE_MSG("Failed to read ligature table header");
  }

  if (lig_glyph >= num_glyphs) {
    return OTS_FAILURE_MSG("too large lig_glyph: %u", lig_glyph);
  }
  if (comp_count == 0) {
    return OTS_FAILURE_MSG("Component count cannot be 0");
  }
  for (unsigned i = 0; i < comp_count - static_cast<unsigned>(1); ++i) {
    uint16_t component = 0;
    if (!subtable.ReadU16(&component)) {
      return OTS_FAILURE_MSG("Can't read ligature component %d", i);
    }
    if (component >= num_glyphs) {
      return OTS_FAILURE_MSG("Bad ligature component %d of %d", i, component);
    }
  }

  return true;
}

bool ParseLigatureSetTable(const ots::Font *font,
                           const uint8_t *data, const size_t length,
                           const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  uint16_t ligature_count = 0;

  if (!subtable.ReadU16(&ligature_count)) {
    return OTS_FAILURE_MSG("Can't read ligature count in ligature set");
  }

  const unsigned ligature_end = static_cast<unsigned>(2) + ligature_count * 2;
  if (ligature_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of ligature %d in ligature set", ligature_end);
  }
  for (unsigned i = 0; i < ligature_count; ++i) {
    uint16_t offset_ligature = 0;
    if (!subtable.ReadU16(&offset_ligature)) {
      return OTS_FAILURE_MSG("Failed to read ligature offset %d", i);
    }
    if (offset_ligature < ligature_end || offset_ligature >= length) {
      return OTS_FAILURE_MSG("Bad ligature offset %d for ligature %d", offset_ligature, i);
    }
    if (!ParseLigatureTable(font, data + offset_ligature, length - offset_ligature,
                            num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse ligature %d", i);
    }
  }

  return true;
}

// Lookup Type 4:
// Ligature Substitution Subtable
bool ParseLigatureSubstitution(const ots::Font *font,
                               const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;
  uint16_t lig_set_count = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&lig_set_count)) {
    return OTS_FAILURE_MSG("Failed to read ligature substitution header");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Bad ligature substitution table format %d", format);
  }

  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;
  const unsigned ligature_set_end = static_cast<unsigned>(6) +
      lig_set_count * 2;
  if (ligature_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of ligature set %d in ligature substitution table", ligature_set_end);
  }
  for (unsigned i = 0; i < lig_set_count; ++i) {
    uint16_t offset_ligature_set = 0;
    if (!subtable.ReadU16(&offset_ligature_set)) {
      return OTS_FAILURE_MSG("Can't read ligature set offset %d", i);
    }
    if (offset_ligature_set < ligature_set_end ||
        offset_ligature_set >= length) {
      return OTS_FAILURE_MSG("Bad ligature set offset %d for set %d", offset_ligature_set, i);
    }
    if (!ParseLigatureSetTable(font, data + offset_ligature_set,
                               length - offset_ligature_set, num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse ligature set %d", i);
    }
  }

  if (offset_coverage < ligature_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table");
  }

  return true;
}

// Lookup Type 5:
// Contextual Substitution Subtable
bool ParseContextSubstitution(const ots::Font *font,
                              const uint8_t *data, const size_t length) {
  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  ots::OpenTypeGSUB *gsub = static_cast<ots::OpenTypeGSUB*>(
      font->GetTypedTable(OTS_TAG_GSUB));
  if (!gsub) {
    return OTS_FAILURE_MSG("Internal error!");
  }
  return ots::ParseContextSubtable(font, data, length, maxp->num_glyphs,
                                   gsub->num_lookups);
}

// Lookup Type 6:
// Chaining Contextual Substitution Subtable
bool ParseChainingContextSubstitution(const ots::Font *font,
                                      const uint8_t *data,
                                      const size_t length) {
  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  ots::OpenTypeGSUB *gsub = static_cast<ots::OpenTypeGSUB*>(
      font->GetTypedTable(OTS_TAG_GSUB));
  if (!gsub) {
    return OTS_FAILURE_MSG("Internal error!");
  }
  return ots::ParseChainingContextSubtable(font, data, length,
                                           maxp->num_glyphs,
                                           gsub->num_lookups);
}

// Lookup Type 7:
// Extension Substition
bool ParseExtensionSubstitution(const ots::Font *font,
                                const uint8_t *data, const size_t length) {
  return ots::ParseExtensionSubtable(font, data, length,
                                     &kGsubLookupSubtableParser);
}

// Lookup Type 8:
// Reverse Chaining Contexual Single Substitution Subtable
bool ParseReverseChainingContextSingleSubstitution(
    const ots::Font *font, const uint8_t *data, const size_t length) {
  ots::Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t offset_coverage = 0;

  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&offset_coverage)) {
    return OTS_FAILURE_MSG("Failed to read reverse chaining header");
  }

  ots::OpenTypeMAXP *maxp = static_cast<ots::OpenTypeMAXP*>(
      font->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return OTS_FAILURE_MSG("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;

  uint16_t backtrack_glyph_count = 0;
  if (!subtable.ReadU16(&backtrack_glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read backtrack glyph count in reverse chaining table");
  }
  std::vector<uint16_t> offsets_backtrack;
  offsets_backtrack.reserve(backtrack_glyph_count);
  for (unsigned i = 0; i < backtrack_glyph_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Failed to read backtrack offset %d", i);
    }
    offsets_backtrack.push_back(offset);
  }

  uint16_t lookahead_glyph_count = 0;
  if (!subtable.ReadU16(&lookahead_glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read look ahead glyph count");
  }
  std::vector<uint16_t> offsets_lookahead;
  offsets_lookahead.reserve(lookahead_glyph_count);
  for (unsigned i = 0; i < lookahead_glyph_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Can't read look ahead offset %d", i);
    }
    offsets_lookahead.push_back(offset);
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE_MSG("Can't read glyph count in reverse chaining table");
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t substitute = 0;
    if (!subtable.ReadU16(&substitute)) {
      return OTS_FAILURE_MSG("Failed to read substitution %d reverse chaining table", i);
    }
    if (substitute >= num_glyphs) {
      return OTS_FAILURE_MSG("Bad substitute glyph %d in reverse chaining table substitution %d", substitute, i);
    }
  }

  const unsigned substitute_end = static_cast<unsigned>(10) +
      (backtrack_glyph_count + lookahead_glyph_count + glyph_count) * 2;
  if (substitute_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad substitute end offset in reverse chaining table");
  }

  if (offset_coverage < substitute_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d in reverse chaining table", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table in reverse chaining table");
  }

  for (unsigned i = 0; i < backtrack_glyph_count; ++i) {
    if (offsets_backtrack[i] < substitute_end ||
        offsets_backtrack[i] >= length) {
      return OTS_FAILURE_MSG("Bad backtrack offset %d for backtrack %d in reverse chaining table", offsets_backtrack[i], i);
    }
    if (!ots::ParseCoverageTable(font, data + offsets_backtrack[i],
                                 length - offsets_backtrack[i], num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse coverage table for backtrack %d in reverse chaining table", i);
    }
  }

  for (unsigned i = 0; i < lookahead_glyph_count; ++i) {
    if (offsets_lookahead[i] < substitute_end ||
        offsets_lookahead[i] >= length) {
      return OTS_FAILURE_MSG("Bad lookahead offset %d for lookahead %d in reverse chaining table", offsets_lookahead[i], i);
    }
    if (!ots::ParseCoverageTable(font, data + offsets_lookahead[i],
                                 length - offsets_lookahead[i], num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse lookahead coverage table %d in reverse chaining table", i);
    }
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeGSUB::Parse(const uint8_t *data, size_t length) {
  // Parsing gsub table requires |maxp->num_glyphs|
  Font *font = GetFont();
  Buffer table(data, length);

  uint16_t version_major = 0, version_minor = 0;
  uint16_t offset_script_list = 0;
  uint16_t offset_feature_list = 0;
  uint16_t offset_lookup_list = 0;
  uint32_t offset_feature_variations = 0;
  if (!table.ReadU16(&version_major) ||
      !table.ReadU16(&version_minor) ||
      !table.ReadU16(&offset_script_list) ||
      !table.ReadU16(&offset_feature_list) ||
      !table.ReadU16(&offset_lookup_list)) {
    return Error("Incomplete table");
  }

  if (version_major != 1 || version_minor > 1) {
    return Error("Bad version");
  }

  if (version_minor > 0) {
    if (!table.ReadU32(&offset_feature_variations)) {
      return Error("Incomplete table");
    }
  }

  const size_t header_size =
    (version_minor == 0) ? kGsubHeaderSize_1_0 : kGsubHeaderSize_1_1;

  if (offset_lookup_list) {
    if (offset_lookup_list < header_size || offset_lookup_list >= length) {
      return Error("Bad lookup list offset in table header");
    }

    if (!ParseLookupListTable(font, data + offset_lookup_list,
                              length - offset_lookup_list,
                              &kGsubLookupSubtableParser,
                              &this->num_lookups)) {
      return Error("Failed to parse lookup list table");
    }
  }

  uint16_t num_features = 0;
  if (offset_feature_list) {
    if (offset_feature_list < header_size || offset_feature_list >= length) {
      return Error("Bad feature list offset in table header");
    }

    if (!ParseFeatureListTable(font, data + offset_feature_list,
                               length - offset_feature_list, this->num_lookups,
                               &num_features)) {
      return Error("Failed to parse feature list table");
    }
  }

  if (offset_script_list) {
    if (offset_script_list < header_size || offset_script_list >= length) {
      return Error("Bad script list offset in table header");
    }

    if (!ParseScriptListTable(font, data + offset_script_list,
                              length - offset_script_list, num_features)) {
      return Error("Failed to parse script list table");
    }
  }

  if (offset_feature_variations) {
    if (offset_feature_variations < header_size || offset_feature_variations >= length) {
      return Error("Bad feature variations offset in table header");
    }

    if (!ParseFeatureVariationsTable(font, data + offset_feature_variations,
                                     length - offset_feature_variations,
                                     this->num_lookups)) {
      return Error("Failed to parse feature variations table");
    }
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeGSUB::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write GSUB table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
