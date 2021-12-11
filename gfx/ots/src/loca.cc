// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "loca.h"

#include "head.h"
#include "maxp.h"

// loca - Index to Location
// http://www.microsoft.com/typography/otspec/loca.htm

namespace ots {

bool OpenTypeLOCA::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // We can't do anything useful in validating this data except to ensure that
  // the values are monotonically increasing.

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));
  if (!maxp || !head) {
    return Error("Required maxp or head tables are missing");
  }

  const unsigned num_glyphs = maxp->num_glyphs;
  unsigned last_offset = 0;
  this->offsets.resize(num_glyphs + 1);
  // maxp->num_glyphs is uint16_t, thus the addition never overflows.

  if (head->index_to_loc_format == 0) {
    // Note that the <= here (and below) is correct. There is one more offset
    // than the number of glyphs in order to give the length of the final
    // glyph.
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint16_t offset = 0;
      if (!table.ReadU16(&offset)) {
        return Error("Failed to read offset for glyph %d", i);
      }
      if (offset < last_offset) {
        return Error("Out of order offset %d < %d for glyph %d", offset, last_offset, i);
      }
      last_offset = offset;
      this->offsets[i] = offset * 2;
    }
  } else {
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint32_t offset = 0;
      if (!table.ReadU32(&offset)) {
        return Error("Failed to read offset for glyph %d", i);
      }
      if (offset < last_offset) {
        return Error("Out of order offset %d < %d for glyph %d", offset, last_offset, i);
      }
      last_offset = offset;
      this->offsets[i] = offset;
    }
  }

  return true;
}

bool OpenTypeLOCA::Serialize(OTSStream *out) {
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));

  if (!head) {
    return Error("Required head table is missing");
  }

  if (head->index_to_loc_format == 0) {
    for (unsigned i = 0; i < this->offsets.size(); ++i) {
      const uint16_t offset = static_cast<uint16_t>(this->offsets[i] >> 1);
      if ((offset != (this->offsets[i] >> 1)) ||
          !out->WriteU16(offset)) {
        return Error("Failed to write glyph offset for glyph %d", i);
      }
    }
  } else {
    for (unsigned i = 0; i < this->offsets.size(); ++i) {
      if (!out->WriteU32(this->offsets[i])) {
        return Error("Failed to write glyph offset for glyph %d", i);
      }
    }
  }

  return true;
}

}  // namespace ots
