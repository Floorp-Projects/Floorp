// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ltsh.h"

#include "maxp.h"

// LTSH - Linear Threshold
// http://www.microsoft.com/typography/otspec/ltsh.htm

namespace ots {

bool OpenTypeLTSH::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table is missing");
  }

  uint16_t num_glyphs = 0;
  if (!table.ReadU16(&this->version) ||
      !table.ReadU16(&num_glyphs)) {
    return Error("Failed to read table header");
  }

  if (this->version != 0) {
    return Drop("Unsupported version: %u", this->version);
  }

  if (num_glyphs != maxp->num_glyphs) {
    return Drop("Bad numGlyphs: %u", num_glyphs);
  }

  this->ypels.reserve(num_glyphs);
  for (unsigned i = 0; i < num_glyphs; ++i) {
    uint8_t pel = 0;
    if (!table.ReadU8(&pel)) {
      return Error("Failed to read pixels for glyph %d", i);
    }
    this->ypels.push_back(pel);
  }

  return true;
}

bool OpenTypeLTSH::Serialize(OTSStream *out) {
  const uint16_t num_ypels = static_cast<uint16_t>(this->ypels.size());
  if (num_ypels != this->ypels.size() ||
      !out->WriteU16(this->version) ||
      !out->WriteU16(num_ypels)) {
    return Error("Failed to write table header");
  }
  for (uint16_t i = 0; i < num_ypels; ++i) {
    if (!out->Write(&(this->ypels[i]), 1)) {
      return Error("Failed to write pixel size for glyph %d", i);
    }
  }

  return true;
}

bool OpenTypeLTSH::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

}  // namespace ots
