// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

#include <limits>
#include <vector>

#include "fvar.h"
#include "gdef.h"

// OpenType Layout Common Table Formats
// http://www.microsoft.com/typography/otspec/chapter2.htm

#define TABLE_NAME "Layout" // XXX: use individual table names

namespace {

// The 'DFLT' tag of script table.
const uint32_t kScriptTableTagDflt = 0x44464c54;
// The value which represents there is no required feature index.
const uint16_t kNoRequiredFeatureIndexDefined = 0xFFFF;
// The lookup flag bit which indicates existence of MarkFilteringSet.
const uint16_t kUseMarkFilteringSetBit = 0x0010;
// The maximum type number of format for device tables.
const uint16_t kMaxDeltaFormatType = 3;
// In variation fonts, Device Tables are replaced by VariationIndex tables,
// indicated by this flag in the deltaFormat field.
const uint16_t kVariationIndex = 0x8000;

struct ScriptRecord {
  uint32_t tag;
  uint16_t offset;
};

struct LangSysRecord {
  uint32_t tag;
  uint16_t offset;
};

struct FeatureRecord {
  uint32_t tag;
  uint16_t offset;
};

bool ParseLangSysTable(const ots::Font *font,
                       ots::Buffer *subtable, const uint32_t tag,
                       const uint16_t num_features) {
  uint16_t offset_lookup_order = 0;
  uint16_t req_feature_index = 0;
  uint16_t feature_count = 0;
  if (!subtable->ReadU16(&offset_lookup_order) ||
      !subtable->ReadU16(&req_feature_index) ||
      !subtable->ReadU16(&feature_count)) {
    return OTS_FAILURE_MSG("Failed to read langsys header for tag %c%c%c%c", OTS_UNTAG(tag));
  }
  // |offset_lookup_order| is reserved and should be NULL.
  if (offset_lookup_order != 0) {
    return OTS_FAILURE_MSG("Bad lookup offset order %d for langsys tag %c%c%c%c", offset_lookup_order, OTS_UNTAG(tag));
  }
  if (req_feature_index != kNoRequiredFeatureIndexDefined &&
      req_feature_index >= num_features) {
    return OTS_FAILURE_MSG("Bad required features index %d for langsys tag %c%c%c%c", req_feature_index, OTS_UNTAG(tag));
  }
  if (feature_count > num_features) {
    return OTS_FAILURE_MSG("Bad feature count %d for langsys tag %c%c%c%c", feature_count, OTS_UNTAG(tag));
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    uint16_t feature_index = 0;
    if (!subtable->ReadU16(&feature_index)) {
      return OTS_FAILURE_MSG("Failed to read feature index %d for langsys tag %c%c%c%c", i, OTS_UNTAG(tag));
    }
    if (feature_index >= num_features) {
      return OTS_FAILURE_MSG("Bad feature index %d for feature %d for langsys tag %c%c%c%c", feature_index, i, OTS_UNTAG(tag));
    }
  }
  return true;
}

bool ParseScriptTable(const ots::Font *font,
                      const uint8_t *data, const size_t length,
                      const uint32_t tag, const uint16_t num_features) {
  ots::Buffer subtable(data, length);

  uint16_t offset_default_lang_sys = 0;
  uint16_t lang_sys_count = 0;
  if (!subtable.ReadU16(&offset_default_lang_sys) ||
      !subtable.ReadU16(&lang_sys_count)) {
    return OTS_FAILURE_MSG("Failed to read script header for script tag %c%c%c%c", OTS_UNTAG(tag));
  }

  // The spec requires a script table for 'DFLT' tag must contain non-NULL
  // |offset_default_lang_sys|.
  // https://www.microsoft.com/typography/otspec/chapter2.htm
  if (tag == kScriptTableTagDflt) {
    if (offset_default_lang_sys == 0) {
      return OTS_FAILURE_MSG("DFLT script doesn't satisfy the spec. DefaultLangSys is NULL");
    }
  }

  const unsigned lang_sys_record_end =
      6 * static_cast<unsigned>(lang_sys_count) + 4;
  if (lang_sys_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of langsys record %d for script tag %c%c%c%c", lang_sys_record_end, OTS_UNTAG(tag));
  }

  std::vector<LangSysRecord> lang_sys_records;
  lang_sys_records.resize(lang_sys_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    if (!subtable.ReadU32(&lang_sys_records[i].tag) ||
        !subtable.ReadU16(&lang_sys_records[i].offset)) {
      return OTS_FAILURE_MSG("Failed to read langsys record header %d for script tag %c%c%c%c", i, OTS_UNTAG(tag));
    }
    // The record array must store the records alphabetically by tag
    if (last_tag != 0 && last_tag > lang_sys_records[i].tag) {
      return OTS_FAILURE_MSG("Bad last tag %d for langsys record %d for script tag %c%c%c%c", last_tag, i, OTS_UNTAG(tag));
    }
    if (lang_sys_records[i].offset < lang_sys_record_end ||
        lang_sys_records[i].offset >= length) {
      return OTS_FAILURE_MSG("bad offset to lang sys table: %x",
                  lang_sys_records[i].offset);
    }
    last_tag = lang_sys_records[i].tag;
  }

  // Check lang sys tables
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    subtable.set_offset(lang_sys_records[i].offset);
    if (!ParseLangSysTable(font, &subtable, lang_sys_records[i].tag, num_features)) {
      return OTS_FAILURE_MSG("Failed to parse langsys table %d (%c%c%c%c) for script tag %c%c%c%c", i, OTS_UNTAG(lang_sys_records[i].tag), OTS_UNTAG(tag));
    }
  }

  return true;
}

bool ParseFeatureTable(const ots::Font *font,
                       const uint8_t *data, const size_t length,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_feature_params = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&offset_feature_params) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read feature table header");
  }

  const unsigned feature_table_end =
      2 * static_cast<unsigned>(lookup_count) + 4;
  if (feature_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of feature table %d", feature_table_end);
  }
  // |offset_feature_params| is generally set to NULL.
  if (offset_feature_params != 0 &&
      (offset_feature_params < feature_table_end ||
       offset_feature_params >= length)) {
    return OTS_FAILURE_MSG("Bad feature params offset %d", offset_feature_params);
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    uint16_t lookup_index = 0;
    if (!subtable.ReadU16(&lookup_index)) {
      return OTS_FAILURE_MSG("Failed to read lookup index for lookup %d", i);
    }
    // lookup index starts with 0.
    if (lookup_index >= num_lookups) {
      return OTS_FAILURE_MSG("Bad lookup index %d for lookup %d", lookup_index, i);
    }
  }
  return true;
}

bool ParseLookupTable(ots::Font *font, const uint8_t *data,
                      const size_t length,
                      const ots::LookupSubtableParser* parser) {
  ots::Buffer subtable(data, length);

  uint16_t lookup_type = 0;
  uint16_t lookup_flag = 0;
  uint16_t subtable_count = 0;
  if (!subtable.ReadU16(&lookup_type) ||
      !subtable.ReadU16(&lookup_flag) ||
      !subtable.ReadU16(&subtable_count)) {
    return OTS_FAILURE_MSG("Failed to read lookup table header");
  }

  if (lookup_type == 0 || lookup_type > parser->num_types) {
    return OTS_FAILURE_MSG("Bad lookup type %d", lookup_type);
  }

  bool use_mark_filtering_set = lookup_flag & kUseMarkFilteringSetBit;

  std::vector<uint16_t> subtables;
  subtables.reserve(subtable_count);
  // If the |kUseMarkFilteringSetBit| of |lookup_flag| is set,
  // extra 2 bytes will follow after subtable offset array.
  const unsigned lookup_table_end = 2 * static_cast<unsigned>(subtable_count) +
      (use_mark_filtering_set ? 8 : 6);
  if (lookup_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of lookup %d", lookup_table_end);
  }
  for (unsigned i = 0; i < subtable_count; ++i) {
    uint16_t offset_subtable = 0;
    if (!subtable.ReadU16(&offset_subtable)) {
      return OTS_FAILURE_MSG("Failed to read subtable offset %d", i);
    }
    if (offset_subtable < lookup_table_end ||
        offset_subtable >= length) {
      return OTS_FAILURE_MSG("Bad subtable offset %d for subtable %d", offset_subtable, i);
    }
    subtables.push_back(offset_subtable);
  }
  if (subtables.size() != subtable_count) {
    return OTS_FAILURE_MSG("Bad subtable size %ld", subtables.size());
  }

  if (use_mark_filtering_set) {
    uint16_t mark_filtering_set = 0;
    if (!subtable.ReadU16(&mark_filtering_set)) {
      return OTS_FAILURE_MSG("Failed to read mark filtering set");
    }

    ots::OpenTypeGDEF *gdef = static_cast<ots::OpenTypeGDEF*>(
        font->GetTypedTable(OTS_TAG_GDEF));

    if (gdef && (gdef->num_mark_glyph_sets == 0 ||
        mark_filtering_set >= gdef->num_mark_glyph_sets)) {
      return OTS_FAILURE_MSG("Bad mark filtering set %d", mark_filtering_set);
    }
  }

  // Parse lookup subtables for this lookup type.
  for (unsigned i = 0; i < subtable_count; ++i) {
    if (!parser->Parse(font, data + subtables[i], length - subtables[i],
                       lookup_type)) {
      return OTS_FAILURE_MSG("Failed to parse subtable %d", i);
    }
  }
  return true;
}

bool ParseClassDefFormat1(const ots::Font *font,
                          const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE_MSG("Failed to skip class definition header");
  }

  uint16_t start_glyph = 0;
  if (!subtable.ReadU16(&start_glyph)) {
    return OTS_FAILURE_MSG("Failed to read starting glyph of class definition");
  }
  if (start_glyph > num_glyphs) {
    return OTS_FAILURE_MSG("Bad starting glyph %d in class definition", start_glyph);
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read glyph count in class definition");
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE_MSG("bad glyph count: %u", glyph_count);
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&class_value)) {
      return OTS_FAILURE_MSG("Failed to read class value for glyph %d in class definition", i);
    }
    if (class_value > num_classes) {
      return OTS_FAILURE_MSG("Bad class value %d for glyph %d in class definition", class_value, i);
    }
  }

  return true;
}

bool ParseClassDefFormat2(const ots::Font *font,
                          const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE_MSG("Failed to read class definition format");
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE_MSG("Failed to read classRangeCount");
  }
  if (range_count > num_glyphs) {
    return OTS_FAILURE_MSG("classRangeCount > glyph count: %u > %u", range_count, num_glyphs);
  }

  uint16_t last_end = 0;
  for (unsigned i = 0; i < range_count; ++i) {
    uint16_t start = 0;
    uint16_t end = 0;
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&start) ||
        !subtable.ReadU16(&end) ||
        !subtable.ReadU16(&class_value)) {
      return OTS_FAILURE_MSG("Failed to read ClassRangeRecord %d", i);
    }
    if (start > end) {
      return OTS_FAILURE_MSG("ClassRangeRecord %d, start > end: %u > %u", i, start, end);
    }
    if (last_end && start <= last_end) {
      return OTS_FAILURE_MSG("ClassRangeRecord %d start overlaps with end of the previous one: %u <= %u", i, start, last_end);
    }
    if (class_value > num_classes) {
      return OTS_FAILURE_MSG("ClassRangeRecord %d class > number of classes: %u > %u", i, class_value, num_classes);
    }
    last_end = end;
  }

  return true;
}

bool ParseCoverageFormat1(const ots::Font *font,
                          const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t expected_num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE_MSG("Failed to skip coverage format");
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE_MSG("Failed to read glyph count in coverage");
  }
  if (glyph_count > num_glyphs) {
    return OTS_FAILURE_MSG("bad glyph count: %u", glyph_count);
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t glyph = 0;
    if (!subtable.ReadU16(&glyph)) {
      return OTS_FAILURE_MSG("Failed to read glyph %d in coverage", i);
    }
    if (glyph > num_glyphs) {
      return OTS_FAILURE_MSG("bad glyph ID: %u", glyph);
    }
  }

  if (expected_num_glyphs && expected_num_glyphs != glyph_count) {
      return OTS_FAILURE_MSG("unexpected number of glyphs: %u", glyph_count);
  }

  return true;
}

bool ParseCoverageFormat2(const ots::Font *font,
                          const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t expected_num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE_MSG("Failed to skip format of coverage type 2");
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE_MSG("Failed to read range count in coverage");
  }
  if (range_count > num_glyphs) {
    return OTS_FAILURE_MSG("bad range count: %u", range_count);
  }
  uint16_t last_end = 0;
  uint16_t last_start_coverage_index = 0;
  for (unsigned i = 0; i < range_count; ++i) {
    uint16_t start = 0;
    uint16_t end = 0;
    uint16_t start_coverage_index = 0;
    if (!subtable.ReadU16(&start) ||
        !subtable.ReadU16(&end) ||
        !subtable.ReadU16(&start_coverage_index)) {
      return OTS_FAILURE_MSG("Failed to read range %d in coverage", i);
    }

    // Some of the Adobe Pro fonts have ranges that overlap by one element: the
    // start of one range is equal to the end of the previous range. Therefore
    // the < in the following condition should be <= were it not for this.
    // See crbug.com/134135.
    if (start > end || (last_end && start < last_end)) {
      return OTS_FAILURE_MSG("glyph range is overlapping.");
    }
    if (start_coverage_index != last_start_coverage_index) {
      return OTS_FAILURE_MSG("bad start coverage index.");
    }
    last_end = end;
    last_start_coverage_index += end - start + 1;
  }

  if (expected_num_glyphs &&
      expected_num_glyphs != last_start_coverage_index) {
      return OTS_FAILURE_MSG("unexpected number of glyphs: %u", last_start_coverage_index);
  }

  return true;
}

// Parsers for Contextual subtables in GSUB/GPOS tables.

bool ParseLookupRecord(const ots::Font *font,
                       ots::Buffer *subtable, const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  uint16_t sequence_index = 0;
  uint16_t lookup_list_index = 0;
  if (!subtable->ReadU16(&sequence_index) ||
      !subtable->ReadU16(&lookup_list_index)) {
    return OTS_FAILURE_MSG("Failed to read header for lookup record");
  }
  if (sequence_index >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad sequence index %d in lookup record", sequence_index);
  }
  if (lookup_list_index >= num_lookups) {
    return OTS_FAILURE_MSG("Bad lookup list index %d in lookup record", lookup_list_index);
  }
  return true;
}

bool ParseRuleSubtable(const ots::Font *font,
                       const uint8_t *data, const size_t length,
                       const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read rule subtable header");
  }

  if (glyph_count == 0 || glyph_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad glyph count %d in rule subtable", glyph_count);
  }
  for (unsigned i = 0; i < glyph_count - static_cast<unsigned>(1); ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE_MSG("Failed to read glyph %d", i);
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE_MSG("Bad glyph %d for entry %d", glyph_id, i);
    }
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup record %d", i);
    }
  }
  return true;
}

bool ParseRuleSetTable(const ots::Font *font,
                       const uint8_t *data, const size_t length,
                       const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t rule_count = 0;
  if (!subtable.ReadU16(&rule_count)) {
    return OTS_FAILURE_MSG("Failed to read rule count in rule set");
  }
  const unsigned rule_end = 2 * static_cast<unsigned>(rule_count) + 2;
  if (rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of rule %d in rule set", rule_end);
  }

  for (unsigned i = 0; i < rule_count; ++i) {
    uint16_t offset_rule = 0;
    if (!subtable.ReadU16(&offset_rule)) {
      return OTS_FAILURE_MSG("Failed to read rule offset for rule set %d", i);
    }
    if (offset_rule < rule_end || offset_rule >= length) {
      return OTS_FAILURE_MSG("Bad rule offset %d in set %d", offset_rule, i);
    }
    if (!ParseRuleSubtable(font, data + offset_rule, length - offset_rule,
                           num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse rule set %d", i);
    }
  }

  return true;
}

bool ParseContextFormat1(const ots::Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t rule_set_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&rule_set_count)) {
    return OTS_FAILURE_MSG("Failed to read header of context format 1");
  }

  const unsigned rule_set_end = static_cast<unsigned>(6) +
      rule_set_count * 2;
  if (rule_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of rule set %d of context format 1", rule_set_end);
  }
  if (offset_coverage < rule_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d in context format 1", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table in context format 1");
  }

  for (unsigned i = 0; i < rule_set_count; ++i) {
    uint16_t offset_rule = 0;
    if (!subtable.ReadU16(&offset_rule)) {
      return OTS_FAILURE_MSG("Failed to read rule offset %d in context format 1", i);
    }
    if (offset_rule < rule_set_end || offset_rule >= length) {
      return OTS_FAILURE_MSG("Bad rule offset %d in rule %d in context format 1", offset_rule, i);
    }
    if (!ParseRuleSetTable(font, data + offset_rule, length - offset_rule,
                           num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse rule set %d in context format 1", i);
    }
  }

  return true;
}

bool ParseClassRuleTable(const ots::Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read header of class rule table");
  }

  if (glyph_count == 0 || glyph_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad glyph count %d in class rule table", glyph_count);
  }

  // ClassRule table contains an array of classes. Each value of classes
  // could take arbitrary values including zero so we don't check these value.
  const unsigned num_classes = glyph_count - static_cast<unsigned>(1);
  if (!subtable.Skip(2 * num_classes)) {
    return OTS_FAILURE_MSG("Failed to skip classes in class rule table");
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup record %d in class rule table", i);
    }
  }
  return true;
}

bool ParseClassSetTable(const ots::Font *font,
                        const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t class_rule_count = 0;
  if (!subtable.ReadU16(&class_rule_count)) {
    return OTS_FAILURE_MSG("Failed to read class rule count in class set table");
  }
  const unsigned class_rule_end =
      2 * static_cast<unsigned>(class_rule_count) + 2;
  if (class_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("bad class rule end %d in class set table", class_rule_end);
  }
  for (unsigned i = 0; i < class_rule_count; ++i) {
    uint16_t offset_class_rule = 0;
    if (!subtable.ReadU16(&offset_class_rule)) {
      return OTS_FAILURE_MSG("Failed to read class rule offset %d in class set table", i);
    }
    if (offset_class_rule < class_rule_end || offset_class_rule >= length) {
      return OTS_FAILURE_MSG("Bad class rule offset %d in class %d", offset_class_rule, i);
    }
    if (!ParseClassRuleTable(font, data + offset_class_rule,
                             length - offset_class_rule, num_glyphs,
                             num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse class rule table %d", i);
    }
  }

  return true;
}

bool ParseContextFormat2(const ots::Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t offset_class_def = 0;
  uint16_t class_set_cnt = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&offset_class_def) ||
      !subtable.ReadU16(&class_set_cnt)) {
    return OTS_FAILURE_MSG("Failed to read header for context format 2");
  }

  const unsigned class_set_end = 2 * static_cast<unsigned>(class_set_cnt) + 8;
  if (class_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of class set %d for context format 2", class_set_end);
  }
  if (offset_coverage < class_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d in context format 2", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table in context format 2");
  }

  if (offset_class_def < class_set_end || offset_class_def >= length) {
    return OTS_FAILURE_MSG("bad class definition offset %d in context format 2", offset_class_def);
  }
  if (!ots::ParseClassDefTable(font, data + offset_class_def,
                               length - offset_class_def,
                               num_glyphs, ots::kMaxClassDefValue)) {
    return OTS_FAILURE_MSG("Failed to parse class definition table in context format 2");
  }

  for (unsigned i = 0; i < class_set_cnt; ++i) {
    uint16_t offset_class_rule = 0;
    if (!subtable.ReadU16(&offset_class_rule)) {
      return OTS_FAILURE_MSG("Failed to read class rule offset %d in context format 2", i);
    }
    if (offset_class_rule) {
      if (offset_class_rule < class_set_end || offset_class_rule >= length) {
        return OTS_FAILURE_MSG("Bad class rule offset %d for rule %d in context format 2", offset_class_rule, i);
      }
      if (!ParseClassSetTable(font, data + offset_class_rule,
                              length - offset_class_rule, num_glyphs,
                              num_lookups)) {
        return OTS_FAILURE_MSG("Failed to parse class set %d in context format 2", i);
      }
    }
  }

  return true;
}

bool ParseContextFormat3(const ots::Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read header in context format 3");
  }

  if (glyph_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad glyph count %d in context format 3", glyph_count);
  }
  const unsigned lookup_record_end = 2 * static_cast<unsigned>(glyph_count) +
      4 * static_cast<unsigned>(lookup_count) + 6;
  if (lookup_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of lookup %d in context format 3", lookup_record_end);
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t offset_coverage = 0;
    if (!subtable.ReadU16(&offset_coverage)) {
      return OTS_FAILURE_MSG("Failed to read coverage offset %d in conxtext format 3", i);
    }
    if (offset_coverage < lookup_record_end || offset_coverage >= length) {
      return OTS_FAILURE_MSG("Bad coverage offset %d for glyph %d in context format 3", offset_coverage, i);
    }
    if (!ots::ParseCoverageTable(font, data + offset_coverage,
                                 length - offset_coverage, num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse coverage table for glyph %d in context format 3", i);
    }
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup record %d in context format 3", i);
    }
  }

  return true;
}

// Parsers for Chaning Contextual subtables in GSUB/GPOS tables.

bool ParseChainRuleSubtable(const ots::Font *font,
                            const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs,
                            const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t backtrack_count = 0;
  if (!subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE_MSG("Failed to read backtrack count in chain rule subtable");
  }
  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad backtrack count %d in chain rule subtable", backtrack_count);
  }
  for (unsigned i = 0; i < backtrack_count; ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE_MSG("Failed to read backtrack glyph %d in chain rule subtable", i);
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE_MSG("Bad glyph id %d for bactrack glyph %d in chain rule subtable", glyph_id, i);
    }
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE_MSG("Failed to read input count in chain rule subtable");
  }
  if (input_count == 0 || input_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad input count %d in chain rule subtable", input_count);
  }
  for (unsigned i = 0; i < input_count - static_cast<unsigned>(1); ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE_MSG("Failed to read input glyph %d in chain rule subtable", i);
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE_MSG("Bad glyph id %d for input glyph %d in chain rule subtable", glyph_id, i);
    }
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE_MSG("Failed to read lookahead count in chain rule subtable");
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad lookahead count %d in chain rule subtable", lookahead_count);
  }
  for (unsigned i = 0; i < lookahead_count; ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE_MSG("Failed to read lookahead glyph %d in chain rule subtable", i);
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE_MSG("Bad glyph id %d for lookadhead glyph %d in chain rule subtable", glyph_id, i);
    }
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read lookup count in chain rule subtable");
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup record %d in chain rule subtable", i);
    }
  }

  return true;
}

bool ParseChainRuleSetTable(const ots::Font *font,
                            const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs,
                            const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t chain_rule_count = 0;
  if (!subtable.ReadU16(&chain_rule_count)) {
    return OTS_FAILURE_MSG("Failed to read rule count in chain rule set");
  }
  const unsigned chain_rule_end =
      2 * static_cast<unsigned>(chain_rule_count) + 2;
  if (chain_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of chain rule %d in chain rule set", chain_rule_end);
  }
  for (unsigned i = 0; i < chain_rule_count; ++i) {
    uint16_t offset_chain_rule = 0;
    if (!subtable.ReadU16(&offset_chain_rule)) {
      return OTS_FAILURE_MSG("Failed to read chain rule offset %d in chain rule set", i);
    }
    if (offset_chain_rule < chain_rule_end || offset_chain_rule >= length) {
      return OTS_FAILURE_MSG("Bad chain rule offset %d for chain rule %d in chain rule set", offset_chain_rule, i);
    }
    if (!ParseChainRuleSubtable(font, data + offset_chain_rule,
                                length - offset_chain_rule,
                                num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chain rule %d in chain rule set", i);
    }
  }

  return true;
}

bool ParseChainContextFormat1(const ots::Font *font,
                              const uint8_t *data, const size_t length,
                              const uint16_t num_glyphs,
                              const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t chain_rule_set_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&chain_rule_set_count)) {
    return OTS_FAILURE_MSG("Failed to read header of chain context format 1");
  }

  const unsigned chain_rule_set_end =
      2 * static_cast<unsigned>(chain_rule_set_count) + 6;
  if (chain_rule_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad chain rule end %d in chain context format 1", chain_rule_set_end);
  }
  if (offset_coverage < chain_rule_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d in chain context format 1", chain_rule_set_end);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table for chain context format 1");
  }

  for (unsigned i = 0; i < chain_rule_set_count; ++i) {
    uint16_t offset_chain_rule_set = 0;
    if (!subtable.ReadU16(&offset_chain_rule_set)) {
      return OTS_FAILURE_MSG("Failed to read chain rule offset %d in chain context format 1", i);
    }
    if (offset_chain_rule_set < chain_rule_set_end ||
        offset_chain_rule_set >= length) {
      return OTS_FAILURE_MSG("Bad chain rule set offset %d for chain rule set %d in chain context format 1", offset_chain_rule_set, i);
    }
    if (!ParseChainRuleSetTable(font, data + offset_chain_rule_set,
                                   length - offset_chain_rule_set,
                                   num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chain rule set %d in chain context format 1", i);
    }
  }

  return true;
}

bool ParseChainClassRuleSubtable(const ots::Font *font,
                                 const uint8_t *data, const size_t length,
                                 const uint16_t num_glyphs,
                                 const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  // In this subtable, we don't check the value of classes for now since
  // these could take arbitrary values.

  uint16_t backtrack_count = 0;
  if (!subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE_MSG("Failed to read backtrack count in chain class rule subtable");
  }
  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad backtrack count %d in chain class rule subtable", backtrack_count);
  }
  if (!subtable.Skip(2 * backtrack_count)) {
    return OTS_FAILURE_MSG("Failed to skip backtrack offsets in chain class rule subtable");
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE_MSG("Failed to read input count in chain class rule subtable");
  }
  if (input_count == 0 || input_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad input count %d in chain class rule subtable", input_count);
  }
  if (!subtable.Skip(2 * (input_count - 1))) {
    return OTS_FAILURE_MSG("Failed to skip input offsets in chain class rule subtable");
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE_MSG("Failed to read lookahead count in chain class rule subtable");
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad lookahead count %d in chain class rule subtable", lookahead_count);
  }
  if (!subtable.Skip(2 * lookahead_count)) {
    return OTS_FAILURE_MSG("Failed to skip lookahead offsets in chain class rule subtable");
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read lookup count in chain class rule subtable");
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup record %d in chain class rule subtable", i);
    }
  }

  return true;
}

bool ParseChainClassSetTable(const ots::Font *font,
                             const uint8_t *data, const size_t length,
                             const uint16_t num_glyphs,
                             const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t chain_class_rule_count = 0;
  if (!subtable.ReadU16(&chain_class_rule_count)) {
    return OTS_FAILURE_MSG("Failed to read rule count in chain class set");
  }
  const unsigned chain_class_rule_end =
      2 * static_cast<unsigned>(chain_class_rule_count) + 2;
  if (chain_class_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of chain class set %d in chain class set", chain_class_rule_end);
  }
  for (unsigned i = 0; i < chain_class_rule_count; ++i) {
    uint16_t offset_chain_class_rule = 0;
    if (!subtable.ReadU16(&offset_chain_class_rule)) {
      return OTS_FAILURE_MSG("Failed to read chain class rule offset %d in chain class set", i);
    }
    if (offset_chain_class_rule < chain_class_rule_end ||
        offset_chain_class_rule >= length) {
      return OTS_FAILURE_MSG("Bad chain class rule offset %d for chain class %d in chain class set", offset_chain_class_rule, i);
    }
    if (!ParseChainClassRuleSubtable(font, data + offset_chain_class_rule,
                                     length - offset_chain_class_rule,
                                     num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chain class rule %d in chain class set", i);
    }
  }

  return true;
}

bool ParseChainContextFormat2(const ots::Font *font,
                              const uint8_t *data, const size_t length,
                              const uint16_t num_glyphs,
                              const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t offset_backtrack_class_def = 0;
  uint16_t offset_input_class_def = 0;
  uint16_t offset_lookahead_class_def = 0;
  uint16_t chain_class_set_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&offset_backtrack_class_def) ||
      !subtable.ReadU16(&offset_input_class_def) ||
      !subtable.ReadU16(&offset_lookahead_class_def) ||
      !subtable.ReadU16(&chain_class_set_count)) {
    return OTS_FAILURE_MSG("Failed to read header of chain context format 2");
  }

  const unsigned chain_class_set_end =
      2 * static_cast<unsigned>(chain_class_set_count) + 12;
  if (chain_class_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad chain class set end %d in chain context format 2", chain_class_set_end);
  }
  if (offset_coverage < chain_class_set_end || offset_coverage >= length) {
    return OTS_FAILURE_MSG("Bad coverage offset %d in chain context format 2", offset_coverage);
  }
  if (!ots::ParseCoverageTable(font, data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to parse coverage table in chain context format 2");
  }

  // Classes for backtrack/lookahead sequences might not be defined.
  if (offset_backtrack_class_def) {
    if (offset_backtrack_class_def < chain_class_set_end ||
        offset_backtrack_class_def >= length) {
      return OTS_FAILURE_MSG("Bad backtrack class offset %d in chain context format 2", offset_backtrack_class_def);
    }
    if (!ots::ParseClassDefTable(font, data + offset_backtrack_class_def,
                                 length - offset_backtrack_class_def,
                                 num_glyphs, ots::kMaxClassDefValue)) {
      return OTS_FAILURE_MSG("Failed to parse backtrack class defn table in chain context format 2");
    }
  }

  if (offset_input_class_def < chain_class_set_end ||
      offset_input_class_def >= length) {
    return OTS_FAILURE_MSG("Bad input class defn offset %d in chain context format 2", offset_input_class_def);
  }
  if (!ots::ParseClassDefTable(font, data + offset_input_class_def,
                               length - offset_input_class_def,
                               num_glyphs, ots::kMaxClassDefValue)) {
    return OTS_FAILURE_MSG("Failed to parse input class defn in chain context format 2");
  }

  if (offset_lookahead_class_def) {
    if (offset_lookahead_class_def < chain_class_set_end ||
        offset_lookahead_class_def >= length) {
      return OTS_FAILURE_MSG("Bad lookahead class defn offset %d in chain context format 2", offset_lookahead_class_def);
    }
    if (!ots::ParseClassDefTable(font, data + offset_lookahead_class_def,
                                 length - offset_lookahead_class_def,
                                 num_glyphs, ots::kMaxClassDefValue)) {
      return OTS_FAILURE_MSG("Failed to parse lookahead class defn in chain context format 2");
    }
  }

  for (unsigned i = 0; i < chain_class_set_count; ++i) {
    uint16_t offset_chain_class_set = 0;
    if (!subtable.ReadU16(&offset_chain_class_set)) {
      return OTS_FAILURE_MSG("Failed to read chain class set offset %d", i);
    }
    // |offset_chain_class_set| could be NULL.
    if (offset_chain_class_set) {
      if (offset_chain_class_set < chain_class_set_end ||
          offset_chain_class_set >= length) {
        return OTS_FAILURE_MSG("Bad chain set class offset %d for chain set %d in chain context format 2", offset_chain_class_set, i);
      }
      if (!ParseChainClassSetTable(font, data + offset_chain_class_set,
                                   length - offset_chain_class_set,
                                   num_glyphs, num_lookups)) {
        return OTS_FAILURE_MSG("Failed to parse chain class set table %d in chain context format 2", i);
      }
    }
  }

  return true;
}

bool ParseChainContextFormat3(const ots::Font *font,
                              const uint8_t *data, const size_t length,
                              const uint16_t num_glyphs,
                              const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t backtrack_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE_MSG("Failed to read backtrack count in chain context format 3");
  }

  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad backtrack count %d in chain context format 3", backtrack_count);
  }
  std::vector<uint16_t> offsets_backtrack;
  offsets_backtrack.reserve(backtrack_count);
  for (unsigned i = 0; i < backtrack_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Failed to read backtrack offset %d in chain context format 3", i);
    }
    offsets_backtrack.push_back(offset);
  }
  if (offsets_backtrack.size() != backtrack_count) {
    return OTS_FAILURE_MSG("Bad backtrack offsets size %ld in chain context format 3", offsets_backtrack.size());
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE_MSG("Failed to read input count in chain context format 3");
  }
  if (input_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad input count %d in chain context format 3", input_count);
  }
  std::vector<uint16_t> offsets_input;
  offsets_input.reserve(input_count);
  for (unsigned i = 0; i < input_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Failed to read input offset %d in chain context format 3", i);
    }
    offsets_input.push_back(offset);
  }
  if (offsets_input.size() != input_count) {
    return OTS_FAILURE_MSG("Bad input offsets size %ld in chain context format 3", offsets_input.size());
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE_MSG("Failed ot read lookahead count in chain context format 3");
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE_MSG("Bad lookahead count %d in chain context format 3", lookahead_count);
  }
  std::vector<uint16_t> offsets_lookahead;
  offsets_lookahead.reserve(lookahead_count);
  for (unsigned i = 0; i < lookahead_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Failed to read lookahead offset %d in chain context format 3", i);
    }
    offsets_lookahead.push_back(offset);
  }
  if (offsets_lookahead.size() != lookahead_count) {
    return OTS_FAILURE_MSG("Bad lookahead offsets size %ld in chain context format 3", offsets_lookahead.size());
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE_MSG("Failed to read lookup count in chain context format 3");
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(font, &subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse lookup %d in chain context format 3", i);
    }
  }

  const unsigned lookup_record_end =
      2 * (static_cast<unsigned>(backtrack_count) +
           static_cast<unsigned>(input_count) +
           static_cast<unsigned>(lookahead_count)) +
      4 * static_cast<unsigned>(lookup_count) + 10;
  if (lookup_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of lookup record %d in chain context format 3", lookup_record_end);
  }
  for (unsigned i = 0; i < backtrack_count; ++i) {
    if (offsets_backtrack[i] < lookup_record_end ||
        offsets_backtrack[i] >= length) {
      return OTS_FAILURE_MSG("Bad backtrack offset of %d for backtrack %d in chain context format 3", offsets_backtrack[i], i);
    }
    if (!ots::ParseCoverageTable(font, data + offsets_backtrack[i],
                                 length - offsets_backtrack[i], num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse backtrack coverage %d in chain context format 3", i);
    }
  }
  for (unsigned i = 0; i < input_count; ++i) {
    if (offsets_input[i] < lookup_record_end || offsets_input[i] >= length) {
      return OTS_FAILURE_MSG("Bad input offset %d for input %d in chain context format 3", offsets_input[i], i);
    }
    if (!ots::ParseCoverageTable(font, data + offsets_input[i],
                                 length - offsets_input[i], num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse input coverage table %d in chain context format 3", i);
    }
  }
  for (unsigned i = 0; i < lookahead_count; ++i) {
    if (offsets_lookahead[i] < lookup_record_end ||
        offsets_lookahead[i] >= length) {
      return OTS_FAILURE_MSG("Bad lookadhead offset %d for lookahead %d in chain context format 3", offsets_lookahead[i], i);
    }
    if (!ots::ParseCoverageTable(font, data + offsets_lookahead[i],
                                 length - offsets_lookahead[i], num_glyphs)) {
      return OTS_FAILURE_MSG("Failed to parse lookahead coverage table %d in chain context format 3", i);
    }
  }

  return true;
}

}  // namespace

namespace ots {

bool LookupSubtableParser::Parse(const Font *font, const uint8_t *data,
                                 const size_t length,
                                 const uint16_t lookup_type) const {
  for (unsigned i = 0; i < num_types; ++i) {
    if (parsers[i].type == lookup_type && parsers[i].parse) {
      if (!parsers[i].parse(font, data, length)) {
        return OTS_FAILURE_MSG("Failed to parse lookup subtable %d", i);
      }
      return true;
    }
  }
  return OTS_FAILURE_MSG("No lookup subtables to parse");
}

// Parsing ScriptListTable requires number of features so we need to
// parse FeatureListTable before calling this function.
bool ParseScriptListTable(const ots::Font *font,
                          const uint8_t *data, const size_t length,
                          const uint16_t num_features) {
  Buffer subtable(data, length);

  uint16_t script_count = 0;
  if (!subtable.ReadU16(&script_count)) {
    return OTS_FAILURE_MSG("Failed to read script count in script list table");
  }

  const unsigned script_record_end =
      6 * static_cast<unsigned>(script_count) + 2;
  if (script_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of script record %d in script list table", script_record_end);
  }
  std::vector<ScriptRecord> script_list;
  script_list.reserve(script_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < script_count; ++i) {
    ScriptRecord record;
    if (!subtable.ReadU32(&record.tag) ||
        !subtable.ReadU16(&record.offset)) {
      return OTS_FAILURE_MSG("Failed to read script record %d in script list table", i);
    }
    // Script tags should be arranged alphabetically by tag
    if (last_tag != 0 && last_tag > record.tag) {
      // Several fonts don't arrange tags alphabetically.
      // It seems that the order of tags might not be a security issue
      // so we just warn it.
      OTS_WARNING("tags aren't arranged alphabetically.");
    }
    last_tag = record.tag;
    if (record.offset < script_record_end || record.offset >= length) {
      return OTS_FAILURE_MSG("Bad record offset %d for script %c%c%c%c entry %d in script list table", record.offset, OTS_UNTAG(record.tag), i);
    }
    script_list.push_back(record);
  }
  if (script_list.size() != script_count) {
    return OTS_FAILURE_MSG("Bad script list size %ld in script list table", script_list.size());
  }

  // Check script records.
  for (unsigned i = 0; i < script_count; ++i) {
    if (!ParseScriptTable(font, data + script_list[i].offset,
                          length - script_list[i].offset,
                          script_list[i].tag, num_features)) {
      return OTS_FAILURE_MSG("Failed to parse script table %d", i);
    }
  }

  return true;
}

// Parsing FeatureListTable requires number of lookups so we need to parse
// LookupListTable before calling this function.
bool ParseFeatureListTable(const ots::Font *font,
                           const uint8_t *data, const size_t length,
                           const uint16_t num_lookups,
                           uint16_t* num_features) {
  Buffer subtable(data, length);

  uint16_t feature_count = 0;
  if (!subtable.ReadU16(&feature_count)) {
    return OTS_FAILURE_MSG("Failed to read feature count");
  }

  std::vector<FeatureRecord> feature_records;
  feature_records.resize(feature_count);
  const unsigned feature_record_end =
      6 * static_cast<unsigned>(feature_count) + 2;
  if (feature_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of feature record %d", feature_record_end);
  }
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < feature_count; ++i) {
    if (!subtable.ReadU32(&feature_records[i].tag) ||
        !subtable.ReadU16(&feature_records[i].offset)) {
      return OTS_FAILURE_MSG("Failed to read feature header %d", i);
    }
    // Feature record array should be arranged alphabetically by tag
    if (last_tag != 0 && last_tag > feature_records[i].tag) {
      // Several fonts don't arrange tags alphabetically.
      // It seems that the order of tags might not be a security issue
      // so we just warn it.
      OTS_WARNING("tags aren't arranged alphabetically.");
    }
    last_tag = feature_records[i].tag;
    if (feature_records[i].offset < feature_record_end ||
        feature_records[i].offset >= length) {
      return OTS_FAILURE_MSG("Bad feature offset %d for feature %d %c%c%c%c", feature_records[i].offset, i, OTS_UNTAG(feature_records[i].tag));
    }
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    if (!ParseFeatureTable(font, data + feature_records[i].offset,
                           length - feature_records[i].offset, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse feature table %d", i);
    }
  }
  *num_features = feature_count;
  return true;
}

// For parsing GPOS/GSUB tables, this function should be called at first to
// obtain the number of lookups because parsing FeatureTableList requires
// the number.
bool ParseLookupListTable(Font *font, const uint8_t *data,
                          const size_t length,
                          const LookupSubtableParser* parser,
                          uint16_t *num_lookups) {
  Buffer subtable(data, length);

  if (!subtable.ReadU16(num_lookups)) {
    return OTS_FAILURE_MSG("Failed to read number of lookups");
  }

  std::vector<uint16_t> lookups;
  lookups.reserve(*num_lookups);
  const unsigned lookup_end =
      2 * static_cast<unsigned>(*num_lookups) + 2;
  if (lookup_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE_MSG("Bad end of lookups %d", lookup_end);
  }
  for (unsigned i = 0; i < *num_lookups; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE_MSG("Failed to read lookup offset %d", i);
    }
    if (offset < lookup_end || offset >= length) {
      return OTS_FAILURE_MSG("Bad lookup offset %d for lookup %d", offset, i);
    }
    lookups.push_back(offset);
  }
  if (lookups.size() != *num_lookups) {
    return OTS_FAILURE_MSG("Bad lookup offsets list size %ld", lookups.size());
  }

  for (unsigned i = 0; i < *num_lookups; ++i) {
    if (!ParseLookupTable(font, data + lookups[i], length - lookups[i],
                          parser)) {
      return OTS_FAILURE_MSG("Failed to parse lookup %d", i);
    }
  }

  return true;
}

bool ParseClassDefTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read class defn format");
  }
  if (format == 1) {
    return ParseClassDefFormat1(font, data, length, num_glyphs, num_classes);
  } else if (format == 2) {
    return ParseClassDefFormat2(font, data, length, num_glyphs, num_classes);
  }

  return OTS_FAILURE_MSG("Bad class defn format %d", format);
}

bool ParseCoverageTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t expected_num_glyphs) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read coverage table format");
  }
  if (format == 1) {
    return ParseCoverageFormat1(font, data, length, num_glyphs, expected_num_glyphs);
  } else if (format == 2) {
    return ParseCoverageFormat2(font, data, length, num_glyphs, expected_num_glyphs);
  }

  return OTS_FAILURE_MSG("Bad coverage table format %d", format);
}

bool ParseDeviceTable(const ots::Font *font,
                      const uint8_t *data, size_t length) {
  Buffer subtable(data, length);

  uint16_t start_size = 0;
  uint16_t end_size = 0;
  uint16_t delta_format = 0;
  if (!subtable.ReadU16(&start_size) ||
      !subtable.ReadU16(&end_size) ||
      !subtable.ReadU16(&delta_format)) {
    return OTS_FAILURE_MSG("Failed to read device table header");
  }
  if (delta_format == kVariationIndex) {
    // start_size and end_size are replaced by deltaSetOuterIndex
    // and deltaSetInnerIndex respectively, but we don't attempt to
    // check them here, so nothing more to do.
    return true;
  }
  if (start_size > end_size) {
    return OTS_FAILURE_MSG("Bad device table size range: %u > %u", start_size, end_size);
  }
  if (delta_format == 0 || delta_format > kMaxDeltaFormatType) {
    return OTS_FAILURE_MSG("Bad device table delta format: 0x%x", delta_format);
  }
  // The number of delta values per uint16. The device table should contain
  // at least |num_units| * 2 bytes compressed data.
  const unsigned num_units = (end_size - start_size) /
      (1 << (4 - delta_format)) + 1;
  // Just skip |num_units| * 2 bytes since the compressed data could take
  // arbitrary values.
  if (!subtable.Skip(num_units * 2)) {
    return OTS_FAILURE_MSG("Failed to skip data in device table");
  }
  return true;
}

bool ParseContextSubtable(const ots::Font *font,
                          const uint8_t *data, const size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read context subtable format");
  }

  if (format == 1) {
    if (!ParseContextFormat1(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse context format 1 subtable");
    }
  } else if (format == 2) {
    if (!ParseContextFormat2(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse context format 2 subtable");
    }
  } else if (format == 3) {
    if (!ParseContextFormat3(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse context format 3 subtable");
    }
  } else {
    return OTS_FAILURE_MSG("Bad context subtable format %d", format);
  }

  return true;
}

bool ParseChainingContextSubtable(const ots::Font *font,
                                  const uint8_t *data, const size_t length,
                                  const uint16_t num_glyphs,
                                  const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read chaining context subtable format");
  }

  if (format == 1) {
    if (!ParseChainContextFormat1(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chaining context format 1 subtable");
    }
  } else if (format == 2) {
    if (!ParseChainContextFormat2(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chaining context format 2 subtable");
    }
  } else if (format == 3) {
    if (!ParseChainContextFormat3(font, data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse chaining context format 3 subtable");
    }
  } else {
    return OTS_FAILURE_MSG("Bad chaining context subtable format %d", format);
  }

  return true;
}

bool ParseExtensionSubtable(const Font *font,
                            const uint8_t *data, const size_t length,
                            const LookupSubtableParser* parser) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t lookup_type = 0;
  uint32_t offset_extension = 0;
  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&lookup_type) ||
      !subtable.ReadU32(&offset_extension)) {
    return OTS_FAILURE_MSG("Failed to read extension table header");
  }

  if (format != 1) {
    return OTS_FAILURE_MSG("Bad extension table format %d", format);
  }
  // |lookup_type| should be other than |parser->extension_type|.
  if (lookup_type < 1 || lookup_type > parser->num_types ||
      lookup_type == parser->extension_type) {
    return OTS_FAILURE_MSG("Bad lookup type %d in extension table", lookup_type);
  }

  const unsigned format_end = static_cast<unsigned>(8);
  if (offset_extension < format_end ||
      offset_extension >= length) {
    return OTS_FAILURE_MSG("Bad extension offset %d", offset_extension);
  }

  // Parse the extension subtable of |lookup_type|.
  if (!parser->Parse(font, data + offset_extension, length - offset_extension,
                     lookup_type)) {
    return OTS_FAILURE_MSG("Failed to parse lookup from extension lookup");
  }

  return true;
}

bool ParseConditionTable(const Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t axis_count) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE_MSG("Failed to read condition table format");
  }

  if (format != 1) {
    // An unknown format is not an error, but should be ignored per spec.
    return true;
  }

  uint16_t axis_index = 0;
  int16_t filter_range_min_value = 0;
  int16_t filter_range_max_value = 0;
  if (!subtable.ReadU16(&axis_index) ||
      !subtable.ReadS16(&filter_range_min_value) ||
      !subtable.ReadS16(&filter_range_max_value)) {
    return OTS_FAILURE_MSG("Failed to read condition table (format 1)");
  }

  if (axis_index >= axis_count) {
    return OTS_FAILURE_MSG("Axis index out of range in condition");
  }

  // Check min/max values are within range -1.0 .. 1.0 and properly ordered
  if (filter_range_min_value < -0x4000 || // -1.0 in F2DOT14 format
      filter_range_max_value > 0x4000 || // +1.0 in F2DOT14 format
      filter_range_min_value > filter_range_max_value) {
    return OTS_FAILURE_MSG("Invalid filter range in condition");
  }

  return true;
}

bool ParseConditionSetTable(const Font *font,
                            const uint8_t *data, const size_t length,
                            const uint16_t axis_count) {
  Buffer subtable(data, length);

  uint16_t condition_count = 0;
  if (!subtable.ReadU16(&condition_count)) {
    return OTS_FAILURE_MSG("Failed to read condition count");
  }

  for (uint16_t i = 0; i < condition_count; i++) {
    uint32_t condition_offset = 0;
    if (!subtable.ReadU32(&condition_offset)) {
      return OTS_FAILURE_MSG("Failed to read condition offset");
    }
    if (condition_offset < subtable.offset() || condition_offset >= length) {
      return OTS_FAILURE_MSG("Offset out of range");
    }
    if (!ParseConditionTable(font, data + condition_offset, length - condition_offset,
                             axis_count)) {
      return OTS_FAILURE_MSG("Failed to parse condition table");
    }
  }

  return true;
}

bool ParseFeatureTableSubstitutionTable(const Font *font,
                                        const uint8_t *data, const size_t length,
                                        const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t version_major = 0;
  uint16_t version_minor = 0;
  uint16_t substitution_count = 0;
  const size_t kFeatureTableSubstitutionHeaderSize = 3 * sizeof(uint16_t);

  if (!subtable.ReadU16(&version_major) ||
      !subtable.ReadU16(&version_minor) ||
      !subtable.ReadU16(&substitution_count)) {
    return OTS_FAILURE_MSG("Failed to read feature table substitution table header");
  }

  for (uint16_t i = 0; i < substitution_count; i++) {
    uint16_t feature_index = 0;
    uint32_t alternate_feature_table_offset = 0;
    const size_t kFeatureTableSubstitutionRecordSize = sizeof(uint16_t) + sizeof(uint32_t);

    if (!subtable.ReadU16(&feature_index) ||
        !subtable.ReadU32(&alternate_feature_table_offset)) {
      return OTS_FAILURE_MSG("Failed to read feature table substitution record");
    }

    if (alternate_feature_table_offset < kFeatureTableSubstitutionHeaderSize +
                                         kFeatureTableSubstitutionRecordSize * substitution_count ||
        alternate_feature_table_offset >= length) {
      return OTS_FAILURE_MSG("Invalid alternate feature table offset");
    }

    if (!ParseFeatureTable(font, data + alternate_feature_table_offset,
                           length - alternate_feature_table_offset, num_lookups)) {
      return OTS_FAILURE_MSG("Failed to parse alternate feature table");
    }
  }

  return true;
}

bool ParseFeatureVariationsTable(const Font *font,
                                 const uint8_t *data, const size_t length,
                                 const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t version_major = 0;
  uint16_t version_minor = 0;
  uint32_t feature_variation_record_count = 0;

  if (!subtable.ReadU16(&version_major) ||
      !subtable.ReadU16(&version_minor) ||
      !subtable.ReadU32(&feature_variation_record_count)) {
    return OTS_FAILURE_MSG("Failed to read feature variations table header");
  }

  OpenTypeFVAR* fvar = static_cast<OpenTypeFVAR*>(font->GetTypedTable(OTS_TAG_FVAR));
  if (!fvar) {
    return OTS_FAILURE_MSG("Not a variation font");
  }
  const uint16_t axis_count = fvar->AxisCount();

  const size_t kEndOfFeatureVariationRecords =
    2 * sizeof(uint16_t) + sizeof(uint32_t) +
    feature_variation_record_count * 2 * sizeof(uint32_t);

  for (uint32_t i = 0; i < feature_variation_record_count; i++) {
    uint32_t condition_set_offset = 0;
    uint32_t feature_table_substitution_offset = 0;
    if (!subtable.ReadU32(&condition_set_offset) ||
        !subtable.ReadU32(&feature_table_substitution_offset)) {
      return OTS_FAILURE_MSG("Failed to read feature variation record");
    }

    if (condition_set_offset) {
      if (condition_set_offset < kEndOfFeatureVariationRecords ||
          condition_set_offset >= length) {
        return OTS_FAILURE_MSG("Condition set offset out of range");
      }
      if (!ParseConditionSetTable(font, data + condition_set_offset,
                                  length - condition_set_offset,
                                  axis_count)) {
        return OTS_FAILURE_MSG("Failed to parse condition set table");
      }
    }

    if (feature_table_substitution_offset) {
      if (feature_table_substitution_offset < kEndOfFeatureVariationRecords ||
          feature_table_substitution_offset >= length) {
        return OTS_FAILURE_MSG("Feature table substitution offset out of range");
      }
      if (!ParseFeatureTableSubstitutionTable(font, data + feature_table_substitution_offset,
                                              length - feature_table_substitution_offset,
                                              num_lookups)) {
        return OTS_FAILURE_MSG("Failed to parse feature table substitution table");
      }
    }
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
