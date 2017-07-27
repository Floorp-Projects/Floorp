// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vorg.h"

#include <vector>

// VORG - Vertical Origin Table
// http://www.microsoft.com/typography/otspec/vorg.htm

namespace ots {

bool OpenTypeVORG::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint16_t num_recs;
  if (!table.ReadU16(&this->major_version) ||
      !table.ReadU16(&this->minor_version) ||
      !table.ReadS16(&this->default_vert_origin_y) ||
      !table.ReadU16(&num_recs)) {
    return Error("Failed to read header");
  }
  if (this->major_version != 1) {
    return Drop("Unsupported majorVersion: %u", this->major_version);
  }
  if (this->minor_version != 0) {
    return Drop("Unsupported minorVersion: %u", this->minor_version);
  }

  // num_recs might be zero (e.g., DFHSMinchoPro5-W3-Demo.otf).
  if (!num_recs) {
    return true;
  }

  uint16_t last_glyph_index = 0;
  this->metrics.reserve(num_recs);
  for (unsigned i = 0; i < num_recs; ++i) {
    OpenTypeVORGMetrics rec;

    if (!table.ReadU16(&rec.glyph_index) ||
        !table.ReadS16(&rec.vert_origin_y)) {
      return Error("Failed to read record %d", i);
    }
    if ((i != 0) && (rec.glyph_index <= last_glyph_index)) {
      return Drop("The table is not sorted");
    }
    last_glyph_index = rec.glyph_index;

    this->metrics.push_back(rec);
  }

  return true;
}

bool OpenTypeVORG::Serialize(OTSStream *out) {
  const uint16_t num_metrics = static_cast<uint16_t>(this->metrics.size());
  if (num_metrics != this->metrics.size() ||
      !out->WriteU16(this->major_version) ||
      !out->WriteU16(this->minor_version) ||
      !out->WriteS16(this->default_vert_origin_y) ||
      !out->WriteU16(num_metrics)) {
    return Error("Failed to write table header");
  }

  for (uint16_t i = 0; i < num_metrics; ++i) {
    const OpenTypeVORGMetrics& rec = this->metrics[i];
    if (!out->WriteU16(rec.glyph_index) ||
        !out->WriteS16(rec.vert_origin_y)) {
      return Error("Failed to write record %d", i);
    }
  }

  return true;
}

bool OpenTypeVORG::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for fonts with TT glyphs.
         GetFont()->GetTable(OTS_TAG_CFF) != NULL;
}

}  // namespace ots
