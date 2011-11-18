// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "layout.h"

#include <limits>
#include <vector>

#include "gdef.h"

// OpenType Layout Common Table Formats
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace {

// The 'DFLT' tag of script table.
const uint32_t kScriptTableTagDflt = 0x44464c54;
// The value which represents there is no required feature index.
const uint16_t kNoRequiredFeatureIndexDefined = 0xFFFF;
// The lookup flag bit which indicates existence of MarkFilteringSet.
const uint16_t kUseMarkFilteringSetBit = 0x0010;
// The lookup flags which require GDEF table.
const uint16_t kGdefRequiredFlags = 0x0002 | 0x0004 | 0x0008;
// The mask for MarkAttachmentType.
const uint16_t kMarkAttachmentTypeMask = 0xFF00;
// The maximum type number of format for device tables.
const uint16_t kMaxDeltaFormatType = 3;
// The maximum number of class value.
const uint16_t kMaxClassDefValue = 0xFFFF;

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

bool ParseLangSysTable(ots::Buffer *subtable, const uint32_t tag,
                       const uint16_t num_features) {
  uint16_t offset_lookup_order = 0;
  uint16_t req_feature_index = 0;
  uint16_t feature_count = 0;
  if (!subtable->ReadU16(&offset_lookup_order) ||
      !subtable->ReadU16(&req_feature_index) ||
      !subtable->ReadU16(&feature_count)) {
    return OTS_FAILURE();
  }
  // |offset_lookup_order| is reserved and should be NULL.
  if (offset_lookup_order != 0) {
    return OTS_FAILURE();
  }
  if (req_feature_index != kNoRequiredFeatureIndexDefined &&
      req_feature_index >= num_features) {
    return OTS_FAILURE();
  }
  if (feature_count > num_features) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    uint16_t feature_index = 0;
    if (!subtable->ReadU16(&feature_index)) {
      return OTS_FAILURE();
    }
    if (feature_index >= num_features) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseScriptTable(const uint8_t *data, const size_t length,
                      const uint32_t tag, const uint16_t num_features) {
  ots::Buffer subtable(data, length);

  uint16_t offset_default_lang_sys = 0;
  uint16_t lang_sys_count = 0;
  if (!subtable.ReadU16(&offset_default_lang_sys) ||
      !subtable.ReadU16(&lang_sys_count)) {
    return OTS_FAILURE();
  }

  // The spec requires a script table for 'DFLT' tag must contain non-NULL
  // |offset_default_lang_sys| and |lang_sys_count| == 0
  if (tag == kScriptTableTagDflt &&
      (offset_default_lang_sys == 0 || lang_sys_count != 0)) {
    OTS_WARNING("DFLT table doesn't satisfy the spec.");
    return OTS_FAILURE();
  }

  const unsigned lang_sys_record_end =
      6 * static_cast<unsigned>(lang_sys_count) + 4;
  if (lang_sys_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  std::vector<LangSysRecord> lang_sys_records;
  lang_sys_records.resize(lang_sys_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    if (!subtable.ReadU32(&lang_sys_records[i].tag) ||
        !subtable.ReadU16(&lang_sys_records[i].offset)) {
      return OTS_FAILURE();
    }
    // The record array must store the records alphabetically by tag
    if (last_tag != 0 && last_tag > lang_sys_records[i].tag) {
      return OTS_FAILURE();
    }
    if (lang_sys_records[i].offset < lang_sys_record_end ||
        lang_sys_records[i].offset >= length) {
      OTS_WARNING("bad offset to lang sys table: %x",
                  lang_sys_records[i].offset);
      return OTS_FAILURE();
    }
    last_tag = lang_sys_records[i].tag;
  }

  // Check lang sys tables
  for (unsigned i = 0; i < lang_sys_count; ++i) {
    subtable.set_offset(lang_sys_records[i].offset);
    if (!ParseLangSysTable(&subtable, lang_sys_records[i].tag, num_features)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseFeatureTable(const uint8_t *data, const size_t length,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_feature_params = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&offset_feature_params) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }

  const unsigned feature_table_end =
      2 * static_cast<unsigned>(lookup_count) + 4;
  if (feature_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  // |offset_feature_params| is generally set to NULL.
  if (offset_feature_params != 0 &&
      (offset_feature_params < feature_table_end ||
       offset_feature_params >= length)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    uint16_t lookup_index = 0;
    if (!subtable.ReadU16(&lookup_index)) {
      return OTS_FAILURE();
    }
    // lookup index starts with 0.
    if (lookup_index >= num_lookups) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseLookupTable(ots::OpenTypeFile *file, const uint8_t *data,
                      const size_t length,
                      const ots::LookupSubtableParser* parser) {
  ots::Buffer subtable(data, length);

  uint16_t lookup_type = 0;
  uint16_t lookup_flag = 0;
  uint16_t subtable_count = 0;
  if (!subtable.ReadU16(&lookup_type) ||
      !subtable.ReadU16(&lookup_flag) ||
      !subtable.ReadU16(&subtable_count)) {
    return OTS_FAILURE();
  }

  if (lookup_type == 0 || lookup_type > parser->num_types) {
    return OTS_FAILURE();
  }

  // Check lookup flags.
  if ((lookup_flag & kGdefRequiredFlags) &&
      (!file->gdef || !file->gdef->has_glyph_class_def)) {
    return OTS_FAILURE();
  }
  if ((lookup_flag & kMarkAttachmentTypeMask) &&
      (!file->gdef || !file->gdef->has_mark_attachment_class_def)) {
    return OTS_FAILURE();
  }
  bool use_mark_filtering_set = false;
  if (lookup_flag & kUseMarkFilteringSetBit) {
    if (!file->gdef || !file->gdef->has_mark_glyph_sets_def) {
      return OTS_FAILURE();
    }
    use_mark_filtering_set = true;
  }

  std::vector<uint16_t> subtables;
  subtables.reserve(subtable_count);
  // If the |kUseMarkFilteringSetBit| of |lookup_flag| is set,
  // extra 2 bytes will follow after subtable offset array.
  const unsigned lookup_table_end = 2 * static_cast<unsigned>(subtable_count) +
      (use_mark_filtering_set ? 8 : 6);
  if (lookup_table_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < subtable_count; ++i) {
    uint16_t offset_subtable = 0;
    if (!subtable.ReadU16(&offset_subtable)) {
      return OTS_FAILURE();
    }
    if (offset_subtable < lookup_table_end ||
        offset_subtable >= length) {
      return OTS_FAILURE();
    }
    subtables.push_back(offset_subtable);
  }
  if (subtables.size() != subtable_count) {
    return OTS_FAILURE();
  }

  if (use_mark_filtering_set) {
    uint16_t mark_filtering_set = 0;
    if (!subtable.ReadU16(&mark_filtering_set)) {
      return OTS_FAILURE();
    }
    if (file->gdef->num_mark_glyph_sets == 0 ||
        mark_filtering_set >= file->gdef->num_mark_glyph_sets) {
      return OTS_FAILURE();
    }
  }

  // Parse lookup subtables for this lookup type.
  for (unsigned i = 0; i < subtable_count; ++i) {
    if (!parser->Parse(file, data + subtables[i], length - subtables[i],
                       lookup_type)) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseClassDefFormat1(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t start_glyph = 0;
  if (!subtable.ReadU16(&start_glyph)) {
    return OTS_FAILURE();
  }
  if (start_glyph > num_glyphs) {
    OTS_WARNING("bad start glyph ID: %u", start_glyph);
    return OTS_FAILURE();
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    OTS_WARNING("bad glyph count: %u", glyph_count);
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&class_value)) {
      return OTS_FAILURE();
    }
    if (class_value > num_classes) {
      OTS_WARNING("bad class value: %u", class_value);
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseClassDefFormat2(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_classes) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE();
  }
  if (range_count > num_glyphs) {
    OTS_WARNING("bad range count: %u", range_count);
    return OTS_FAILURE();
  }

  uint16_t last_end = 0;
  for (unsigned i = 0; i < range_count; ++i) {
    uint16_t start = 0;
    uint16_t end = 0;
    uint16_t class_value = 0;
    if (!subtable.ReadU16(&start) ||
        !subtable.ReadU16(&end) ||
        !subtable.ReadU16(&class_value)) {
      return OTS_FAILURE();
    }
    if (start > end || (last_end && start <= last_end)) {
      OTS_WARNING("glyph range is overlapping.");
      return OTS_FAILURE();
    }
    if (class_value > num_classes) {
      OTS_WARNING("bad class value: %u", class_value);
      return OTS_FAILURE();
    }
    last_end = end;
  }

  return true;
}

bool ParseCoverageFormat1(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t glyph_count = 0;
  if (!subtable.ReadU16(&glyph_count)) {
    return OTS_FAILURE();
  }
  if (glyph_count > num_glyphs) {
    OTS_WARNING("bad glyph count: %u", glyph_count);
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t glyph = 0;
    if (!subtable.ReadU16(&glyph)) {
      return OTS_FAILURE();
    }
    if (glyph > num_glyphs) {
      OTS_WARNING("bad glyph ID: %u", glyph);
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseCoverageFormat2(const uint8_t *data, size_t length,
                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Skip format field.
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  uint16_t range_count = 0;
  if (!subtable.ReadU16(&range_count)) {
    return OTS_FAILURE();
  }
  if (range_count > num_glyphs) {
    OTS_WARNING("bad range count: %u", range_count);
    return OTS_FAILURE();
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
      return OTS_FAILURE();
    }
    if (start > end || (last_end && start <= last_end)) {
      OTS_WARNING("glyph range is overlapping.");
      return OTS_FAILURE();
    }
    if (start_coverage_index != last_start_coverage_index) {
      OTS_WARNING("bad start coverage index.");
      return OTS_FAILURE();
    }
    last_end = end;
    last_start_coverage_index += end - start + 1;
  }

  return true;
}

// Parsers for Contextual subtables in GSUB/GPOS tables.

bool ParseLookupRecord(ots::Buffer *subtable, const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  uint16_t sequence_index = 0;
  uint16_t lookup_list_index = 0;
  if (!subtable->ReadU16(&sequence_index) ||
      !subtable->ReadU16(&lookup_list_index)) {
    return OTS_FAILURE();
  }
  if (sequence_index >= num_glyphs) {
    return OTS_FAILURE();
  }
  if (lookup_list_index >= num_lookups) {
    return OTS_FAILURE();
  }
  return true;
}

bool ParseRuleSubtable(const uint8_t *data, const size_t length,
                       const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }

  if (glyph_count == 0 || glyph_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count - static_cast<unsigned>(1); ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE();
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseRuleSetTable(const uint8_t *data, const size_t length,
                       const uint16_t num_glyphs,
                       const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t rule_count = 0;
  if (!subtable.ReadU16(&rule_count)) {
    return OTS_FAILURE();
  }
  const unsigned rule_end = 2 * static_cast<unsigned>(rule_count) + 2;
  if (rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < rule_count; ++i) {
    uint16_t offset_rule = 0;
    if (!subtable.ReadU16(&offset_rule)) {
      return OTS_FAILURE();
    }
    if (offset_rule < rule_end || offset_rule >= length) {
      return OTS_FAILURE();
    }
    if (!ParseRuleSubtable(data + offset_rule, length - offset_rule,
                           num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseContextFormat1(const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t rule_set_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&rule_set_count)) {
    return OTS_FAILURE();
  }

  const unsigned rule_set_end = static_cast<unsigned>(6) +
      rule_set_count * 2;
  if (rule_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage < rule_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < rule_set_count; ++i) {
    uint16_t offset_rule = 0;
    if (!subtable.ReadU16(&offset_rule)) {
      return OTS_FAILURE();
    }
    if (offset_rule < rule_set_end || offset_rule >= length) {
      return OTS_FAILURE();
    }
    if (!ParseRuleSetTable(data + offset_rule, length - offset_rule,
                           num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseClassRuleTable(const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }

  if (glyph_count == 0 || glyph_count >= num_glyphs) {
    return OTS_FAILURE();
  }

  // ClassRule table contains an array of classes. Each value of classes
  // could take arbitrary values including zero so we don't check these value.
  const unsigned num_classes = glyph_count - static_cast<unsigned>(1);
  if (!subtable.Skip(2 * num_classes)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }
  return true;
}

bool ParseClassSetTable(const uint8_t *data, const size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t class_rule_count = 0;
  if (!subtable.ReadU16(&class_rule_count)) {
    return OTS_FAILURE();
  }
  const unsigned class_rule_end =
      2 * static_cast<unsigned>(class_rule_count) + 2;
  if (class_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < class_rule_count; ++i) {
    uint16_t offset_class_rule = 0;
    if (!subtable.ReadU16(&offset_class_rule)) {
      return OTS_FAILURE();
    }
    if (offset_class_rule < class_rule_end || offset_class_rule >= length) {
      return OTS_FAILURE();
    }
    if (!ParseClassRuleTable(data + offset_class_rule,
                             length - offset_class_rule, num_glyphs,
                             num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseContextFormat2(const uint8_t *data, const size_t length,
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
    return OTS_FAILURE();
  }

  const unsigned class_set_end = 2 * static_cast<unsigned>(class_set_cnt) + 8;
  if (class_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage < class_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  if (offset_class_def < class_set_end || offset_class_def >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseClassDefTable(data + offset_class_def,
                               length - offset_class_def,
                               num_glyphs, kMaxClassDefValue)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < class_set_cnt; ++i) {
    uint16_t offset_class_rule = 0;
    if (!subtable.ReadU16(&offset_class_rule)) {
      return OTS_FAILURE();
    }
    if (offset_class_rule) {
      if (offset_class_rule < class_set_end || offset_class_rule >= length) {
        return OTS_FAILURE();
      }
      if (!ParseClassSetTable(data + offset_class_rule,
                              length - offset_class_rule, num_glyphs,
                              num_lookups)) {
        return OTS_FAILURE();
      }
    }
  }

  return true;
}

bool ParseContextFormat3(const uint8_t *data, const size_t length,
                         const uint16_t num_glyphs,
                         const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t glyph_count = 0;
  uint16_t lookup_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&glyph_count) ||
      !subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }

  if (glyph_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  const unsigned lookup_record_end = 2 * static_cast<unsigned>(glyph_count) +
      4 * static_cast<unsigned>(lookup_count) + 6;
  if (lookup_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < glyph_count; ++i) {
    uint16_t offset_coverage = 0;
    if (!subtable.ReadU16(&offset_coverage)) {
      return OTS_FAILURE();
    }
    if (offset_coverage < lookup_record_end || offset_coverage >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offset_coverage,
                                 length - offset_coverage, num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

// Parsers for Chaning Contextual subtables in GSUB/GPOS tables.

bool ParseChainRuleSubtable(const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs,
                            const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t backtrack_count = 0;
  if (!subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE();
  }
  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < backtrack_count; ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE();
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE();
    }
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE();
  }
  if (input_count == 0 || input_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < input_count - static_cast<unsigned>(1); ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE();
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE();
    }
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE();
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < lookahead_count; ++i) {
    uint16_t glyph_id = 0;
    if (!subtable.ReadU16(&glyph_id)) {
      return OTS_FAILURE();
    }
    if (glyph_id > num_glyphs) {
      return OTS_FAILURE();
    }
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseChainRuleSetTable(const uint8_t *data, const size_t length,
                            const uint16_t num_glyphs,
                            const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t chain_rule_count = 0;
  if (!subtable.ReadU16(&chain_rule_count)) {
    return OTS_FAILURE();
  }
  const unsigned chain_rule_end =
      2 * static_cast<unsigned>(chain_rule_count) + 2;
  if (chain_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < chain_rule_count; ++i) {
    uint16_t offset_chain_rule = 0;
    if (!subtable.ReadU16(&offset_chain_rule)) {
      return OTS_FAILURE();
    }
    if (offset_chain_rule < chain_rule_end || offset_chain_rule >= length) {
      return OTS_FAILURE();
    }
    if (!ParseChainRuleSubtable(data + offset_chain_rule,
                                length - offset_chain_rule,
                                num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseChainContextFormat1(const uint8_t *data, const size_t length,
                              const uint16_t num_glyphs,
                              const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t offset_coverage = 0;
  uint16_t chain_rule_set_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&chain_rule_set_count)) {
    return OTS_FAILURE();
  }

  const unsigned chain_rule_set_end =
      2 * static_cast<unsigned>(chain_rule_set_count) + 6;
  if (chain_rule_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage < chain_rule_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < chain_rule_set_count; ++i) {
    uint16_t offset_chain_rule_set = 0;
    if (!subtable.ReadU16(&offset_chain_rule_set)) {
      return OTS_FAILURE();
    }
    if (offset_chain_rule_set < chain_rule_set_end ||
        offset_chain_rule_set >= length) {
      return OTS_FAILURE();
    }
    if (!ParseChainRuleSetTable(data + offset_chain_rule_set,
                                   length - offset_chain_rule_set,
                                   num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseChainClassRuleSubtable(const uint8_t *data, const size_t length,
                                 const uint16_t num_glyphs,
                                 const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  // In this subtable, we don't check the value of classes for now since
  // these could take arbitrary values.

  uint16_t backtrack_count = 0;
  if (!subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE();
  }
  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  if (!subtable.Skip(2 * backtrack_count)) {
    return OTS_FAILURE();
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE();
  }
  if (input_count == 0 || input_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  if (!subtable.Skip(2 * (input_count - 1))) {
    return OTS_FAILURE();
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE();
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  if (!subtable.Skip(2 * lookahead_count)) {
    return OTS_FAILURE();
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseChainClassSetTable(const uint8_t *data, const size_t length,
                             const uint16_t num_glyphs,
                             const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t chain_class_rule_count = 0;
  if (!subtable.ReadU16(&chain_class_rule_count)) {
    return OTS_FAILURE();
  }
  const unsigned chain_class_rule_end =
      2 * static_cast<unsigned>(chain_class_rule_count) + 2;
  if (chain_class_rule_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < chain_class_rule_count; ++i) {
    uint16_t offset_chain_class_rule = 0;
    if (!subtable.ReadU16(&offset_chain_class_rule)) {
      return OTS_FAILURE();
    }
    if (offset_chain_class_rule < chain_class_rule_end ||
        offset_chain_class_rule >= length) {
      return OTS_FAILURE();
    }
    if (!ParseChainClassRuleSubtable(data + offset_chain_class_rule,
                                     length - offset_chain_class_rule,
                                     num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseChainContextFormat2(const uint8_t *data, const size_t length,
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
    return OTS_FAILURE();
  }

  const unsigned chain_class_set_end =
      2 * static_cast<unsigned>(chain_class_set_count) + 12;
  if (chain_class_set_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  if (offset_coverage < chain_class_set_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(data + offset_coverage,
                               length - offset_coverage, num_glyphs)) {
    return OTS_FAILURE();
  }

  // Classes for backtrack/lookahead sequences might not be defined.
  if (offset_backtrack_class_def) {
    if (offset_backtrack_class_def < chain_class_set_end ||
        offset_backtrack_class_def >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseClassDefTable(data + offset_backtrack_class_def,
                                 length - offset_backtrack_class_def,
                                 num_glyphs, kMaxClassDefValue)) {
      return OTS_FAILURE();
    }
  }

  if (offset_input_class_def < chain_class_set_end ||
      offset_input_class_def >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseClassDefTable(data + offset_input_class_def,
                               length - offset_input_class_def,
                               num_glyphs, kMaxClassDefValue)) {
    return OTS_FAILURE();
  }

  if (offset_lookahead_class_def) {
    if (offset_lookahead_class_def < chain_class_set_end ||
        offset_lookahead_class_def >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseClassDefTable(data + offset_lookahead_class_def,
                                 length - offset_lookahead_class_def,
                                 num_glyphs, kMaxClassDefValue)) {
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < chain_class_set_count; ++i) {
    uint16_t offset_chain_class_set = 0;
    if (!subtable.ReadU16(&offset_chain_class_set)) {
      return OTS_FAILURE();
    }
    // |offset_chain_class_set| could be NULL.
    if (offset_chain_class_set) {
      if (offset_chain_class_set < chain_class_set_end ||
          offset_chain_class_set >= length) {
        return OTS_FAILURE();
      }
      if (!ParseChainClassSetTable(data + offset_chain_class_set,
                                   length - offset_chain_class_set,
                                   num_glyphs, num_lookups)) {
        return OTS_FAILURE();
      }
    }
  }

  return true;
}

bool ParseChainContextFormat3(const uint8_t *data, const size_t length,
                              const uint16_t num_glyphs,
                              const uint16_t num_lookups) {
  ots::Buffer subtable(data, length);

  uint16_t backtrack_count = 0;
  // Skip format field.
  if (!subtable.Skip(2) ||
      !subtable.ReadU16(&backtrack_count)) {
    return OTS_FAILURE();
  }

  if (backtrack_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  std::vector<uint16_t> offsets_backtrack;
  offsets_backtrack.reserve(backtrack_count);
  for (unsigned i = 0; i < backtrack_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    offsets_backtrack.push_back(offset);
  }
  if (offsets_backtrack.size() != backtrack_count) {
    return OTS_FAILURE();
  }

  uint16_t input_count = 0;
  if (!subtable.ReadU16(&input_count)) {
    return OTS_FAILURE();
  }
  if (input_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  std::vector<uint16_t> offsets_input;
  offsets_input.reserve(input_count);
  for (unsigned i = 0; i < input_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    offsets_input.push_back(offset);
  }
  if (offsets_input.size() != input_count) {
    return OTS_FAILURE();
  }

  uint16_t lookahead_count = 0;
  if (!subtable.ReadU16(&lookahead_count)) {
    return OTS_FAILURE();
  }
  if (lookahead_count >= num_glyphs) {
    return OTS_FAILURE();
  }
  std::vector<uint16_t> offsets_lookahead;
  offsets_lookahead.reserve(lookahead_count);
  for (unsigned i = 0; i < lookahead_count; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    offsets_lookahead.push_back(offset);
  }
  if (offsets_lookahead.size() != lookahead_count) {
    return OTS_FAILURE();
  }

  uint16_t lookup_count = 0;
  if (!subtable.ReadU16(&lookup_count)) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < lookup_count; ++i) {
    if (!ParseLookupRecord(&subtable, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  }

  const unsigned lookup_record_end =
      2 * (static_cast<unsigned>(backtrack_count) +
           static_cast<unsigned>(input_count) +
           static_cast<unsigned>(lookahead_count)) +
      4 * static_cast<unsigned>(lookup_count) + 10;
  if (lookup_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < backtrack_count; ++i) {
    if (offsets_backtrack[i] < lookup_record_end ||
        offsets_backtrack[i] >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offsets_backtrack[i],
                                 length - offsets_backtrack[i], num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  for (unsigned i = 0; i < input_count; ++i) {
    if (offsets_input[i] < lookup_record_end || offsets_input[i] >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseCoverageTable(data + offsets_input[i],
                                 length - offsets_input[i], num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  for (unsigned i = 0; i < lookahead_count; ++i) {
    if (offsets_lookahead[i] < lookup_record_end ||
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

namespace ots {

bool LookupSubtableParser::Parse(const OpenTypeFile *file, const uint8_t *data,
                                 const size_t length,
                                 const uint16_t lookup_type) const {
  for (unsigned i = 0; i < num_types; ++i) {
    if (parsers[i].type == lookup_type && parsers[i].parse) {
      if (!parsers[i].parse(file, data, length)) {
        return OTS_FAILURE();
      }
      return true;
    }
  }
  return OTS_FAILURE();
}

// Parsing ScriptListTable requires number of features so we need to
// parse FeatureListTable before calling this function.
bool ParseScriptListTable(const uint8_t *data, const size_t length,
                          const uint16_t num_features) {
  Buffer subtable(data, length);

  uint16_t script_count = 0;
  if (!subtable.ReadU16(&script_count)) {
    return OTS_FAILURE();
  }

  const unsigned script_record_end =
      6 * static_cast<unsigned>(script_count) + 2;
  if (script_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  std::vector<ScriptRecord> script_list;
  script_list.reserve(script_count);
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < script_count; ++i) {
    ScriptRecord record;
    if (!subtable.ReadU32(&record.tag) ||
        !subtable.ReadU16(&record.offset)) {
      return OTS_FAILURE();
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
      return OTS_FAILURE();
    }
    script_list.push_back(record);
  }
  if (script_list.size() != script_count) {
    return OTS_FAILURE();
  }

  // Check script records.
  for (unsigned i = 0; i < script_count; ++i) {
    if (!ParseScriptTable(data + script_list[i].offset,
                          length - script_list[i].offset,
                          script_list[i].tag, num_features)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

// Parsing FeatureListTable requires number of lookups so we need to parse
// LookupListTable before calling this function.
bool ParseFeatureListTable(const uint8_t *data, const size_t length,
                           const uint16_t num_lookups,
                           uint16_t* num_features) {
  Buffer subtable(data, length);

  uint16_t feature_count = 0;
  if (!subtable.ReadU16(&feature_count)) {
    return OTS_FAILURE();
  }

  std::vector<FeatureRecord> feature_records;
  feature_records.resize(feature_count);
  const unsigned feature_record_end =
      6 * static_cast<unsigned>(feature_count) + 2;
  if (feature_record_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  uint32_t last_tag = 0;
  for (unsigned i = 0; i < feature_count; ++i) {
    if (!subtable.ReadU32(&feature_records[i].tag) ||
        !subtable.ReadU16(&feature_records[i].offset)) {
      return OTS_FAILURE();
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
      return OTS_FAILURE();
    }
  }

  for (unsigned i = 0; i < feature_count; ++i) {
    if (!ParseFeatureTable(data + feature_records[i].offset,
                           length - feature_records[i].offset, num_lookups)) {
      return OTS_FAILURE();
    }
  }
  *num_features = feature_count;
  return true;
}

// For parsing GPOS/GSUB tables, this function should be called at first to
// obtain the number of lookups because parsing FeatureTableList requires
// the number.
bool ParseLookupListTable(OpenTypeFile *file, const uint8_t *data,
                          const size_t length,
                          const LookupSubtableParser* parser,
                          uint16_t *num_lookups) {
  Buffer subtable(data, length);

  if (!subtable.ReadU16(num_lookups)) {
    return OTS_FAILURE();
  }

  std::vector<uint16_t> lookups;
  lookups.reserve(*num_lookups);
  const unsigned lookup_end =
      2 * static_cast<unsigned>(*num_lookups) + 2;
  if (lookup_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }
  for (unsigned i = 0; i < *num_lookups; ++i) {
    uint16_t offset = 0;
    if (!subtable.ReadU16(&offset)) {
      return OTS_FAILURE();
    }
    if (offset < lookup_end || offset >= length) {
      return OTS_FAILURE();
    }
    lookups.push_back(offset);
  }
  if (lookups.size() != *num_lookups) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < *num_lookups; ++i) {
    if (!ParseLookupTable(file, data + lookups[i], length - lookups[i],
                          parser)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ParseClassDefTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }
  if (format == 1) {
    return ParseClassDefFormat1(data, length, num_glyphs, num_classes);
  } else if (format == 2) {
    return ParseClassDefFormat2(data, length, num_glyphs, num_classes);
  }

  return OTS_FAILURE();
}

bool ParseCoverageTable(const uint8_t *data, size_t length,
                        const uint16_t num_glyphs) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }
  if (format == 1) {
    return ParseCoverageFormat1(data, length, num_glyphs);
  } else if (format == 2) {
    return ParseCoverageFormat2(data, length, num_glyphs);
  }

  return OTS_FAILURE();
}

bool ParseDeviceTable(const uint8_t *data, size_t length) {
  Buffer subtable(data, length);

  uint16_t start_size = 0;
  uint16_t end_size = 0;
  uint16_t delta_format = 0;
  if (!subtable.ReadU16(&start_size) ||
      !subtable.ReadU16(&end_size) ||
      !subtable.ReadU16(&delta_format)) {
    return OTS_FAILURE();
  }
  if (start_size > end_size) {
    OTS_WARNING("bad size range: %u > %u", start_size, end_size);
    return OTS_FAILURE();
  }
  if (delta_format == 0 || delta_format > kMaxDeltaFormatType) {
    OTS_WARNING("bad delta format: %u", delta_format);
    return OTS_FAILURE();
  }
  // The number of delta values per uint16. The device table should contain
  // at least |num_units| * 2 bytes compressed data.
  const unsigned num_units = (end_size - start_size) /
      (1 << (4 - delta_format)) + 1;
  // Just skip |num_units| * 2 bytes since the compressed data could take
  // arbitrary values.
  if (!subtable.Skip(num_units * 2)) {
    return OTS_FAILURE();
  }
  return true;
}

bool ParseContextSubtable(const uint8_t *data, const size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }

  if (format == 1) {
    if (!ParseContextFormat1(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else if (format == 2) {
    if (!ParseContextFormat2(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else if (format == 3) {
    if (!ParseContextFormat3(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseChainingContextSubtable(const uint8_t *data, const size_t length,
                                  const uint16_t num_glyphs,
                                  const uint16_t num_lookups) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  if (!subtable.ReadU16(&format)) {
    return OTS_FAILURE();
  }

  if (format == 1) {
    if (!ParseChainContextFormat1(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else if (format == 2) {
    if (!ParseChainContextFormat2(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else if (format == 3) {
    if (!ParseChainContextFormat3(data, length, num_glyphs, num_lookups)) {
      return OTS_FAILURE();
    }
  } else {
    return OTS_FAILURE();
  }

  return true;
}

bool ParseExtensionSubtable(const OpenTypeFile *file,
                            const uint8_t *data, const size_t length,
                            const LookupSubtableParser* parser) {
  Buffer subtable(data, length);

  uint16_t format = 0;
  uint16_t lookup_type = 0;
  uint32_t offset_extension = 0;
  if (!subtable.ReadU16(&format) ||
      !subtable.ReadU16(&lookup_type) ||
      !subtable.ReadU32(&offset_extension)) {
    return OTS_FAILURE();
  }

  if (format != 1) {
    return OTS_FAILURE();
  }
  // |lookup_type| should be other than |parser->extension_type|.
  if (lookup_type < 1 || lookup_type > parser->num_types ||
      lookup_type == parser->extension_type) {
    return OTS_FAILURE();
  }

  const unsigned format_end = static_cast<unsigned>(8);
  if (offset_extension < format_end ||
      offset_extension >= length) {
    return OTS_FAILURE();
  }

  // Parse the extension subtable of |lookup_type|.
  if (!parser->Parse(file, data + offset_extension, length - offset_extension,
                     lookup_type)) {
    return OTS_FAILURE();
  }

  return true;
}

}  // namespace ots

