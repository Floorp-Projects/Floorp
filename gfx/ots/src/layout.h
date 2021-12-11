// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_LAYOUT_H_
#define OTS_LAYOUT_H_

#include "ots.h"

// Utility functions for OpenType layout common table formats.
// http://www.microsoft.com/typography/otspec/chapter2.htm

namespace ots {

// The maximum number of class value.
const uint16_t kMaxClassDefValue = 0xFFFF;

struct LookupSubtableParser {
  struct TypeParser {
    uint16_t type;
    bool (*parse)(const Font *font, const uint8_t *data,
                  const size_t length);
  };
  size_t num_types;
  uint16_t extension_type;
  const TypeParser *parsers;

  bool Parse(const Font *font, const uint8_t *data,
             const size_t length, const uint16_t lookup_type) const;
};

bool ParseScriptListTable(const ots::Font *font,
                          const uint8_t *data, const size_t length,
                          const uint16_t num_features);

bool ParseFeatureListTable(const ots::Font *font,
                           const uint8_t *data, const size_t length,
                           const uint16_t num_lookups,
                           uint16_t *num_features);

bool ParseLookupListTable(Font *font, const uint8_t *data,
                          const size_t length,
                          const LookupSubtableParser* parser,
                          uint16_t* num_lookups);

bool ParseClassDefTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t num_classes);

bool ParseCoverageTable(const ots::Font *font,
                        const uint8_t *data, size_t length,
                        const uint16_t num_glyphs,
                        const uint16_t expected_num_glyphs = 0);

bool ParseDeviceTable(const ots::Font *font,
                      const uint8_t *data, size_t length);

// Parser for 'Contextual' subtable shared by GSUB/GPOS tables.
bool ParseContextSubtable(const ots::Font *font,
                          const uint8_t *data, const size_t length,
                          const uint16_t num_glyphs,
                          const uint16_t num_lookups);

// Parser for 'Chaining Contextual' subtable shared by GSUB/GPOS tables.
bool ParseChainingContextSubtable(const ots::Font *font,
                                  const uint8_t *data, const size_t length,
                                  const uint16_t num_glyphs,
                                  const uint16_t num_lookups);

bool ParseExtensionSubtable(const Font *font,
                            const uint8_t *data, const size_t length,
                            const LookupSubtableParser* parser);

// For feature variations table (in GSUB/GPOS v1.1)
bool ParseConditionTable(const Font *font,
                         const uint8_t *data, const size_t length,
                         const uint16_t axis_count);

bool ParseConditionSetTable(const Font *font,
                            const uint8_t *data, const size_t length,
                            const uint16_t axis_count);

bool ParseFeatureTableSubstitutionTable(const Font *font,
                                        const uint8_t *data, const size_t length,
                                        const uint16_t num_lookups);

bool ParseFeatureVariationsTable(const Font *font,
                                 const uint8_t *data, const size_t length,
                                 const uint16_t num_lookups);

}  // namespace ots

#endif  // OTS_LAYOUT_H_

