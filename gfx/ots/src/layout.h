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

class OpenTypeLayoutTable : public Table {
  public:
    explicit OpenTypeLayoutTable(Font *font, uint32_t tag, uint32_t type)
      : Table(font, tag, type) { }

    bool Parse(const uint8_t *data, size_t length);
    bool Serialize(OTSStream *out);

  protected:
    bool ParseContextSubtable(const uint8_t *data, const size_t length);
    bool ParseChainingContextSubtable(const uint8_t *data, const size_t length);
    bool ParseExtensionSubtable(const uint8_t *data, const size_t length);

  private:
    bool ParseScriptListTable(const uint8_t *data, const size_t length);
    bool ParseFeatureListTable(const uint8_t *data, const size_t length);
    bool ParseLookupListTable(const uint8_t *data, const size_t length);
    bool ParseFeatureVariationsTable(const uint8_t *data, const size_t length);
    bool ParseLookupTable(const uint8_t *data, const size_t length);

    virtual bool ValidLookupSubtableType(const uint16_t lookup_type,
                                         bool extension = false) const = 0;
    virtual bool ParseLookupSubtable(const uint8_t *data, const size_t length,
                                     const uint16_t lookup_type) = 0;

    const uint8_t *m_data = nullptr;
    size_t m_length = 0;
    uint16_t m_num_features = 0;
    uint16_t m_num_lookups = 0;
};

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

}  // namespace ots

#endif  // OTS_LAYOUT_H_

