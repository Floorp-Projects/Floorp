// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "loca.h"

#include "head.h"
#include "maxp.h"

// loca - Index to Location
// http://www.microsoft.com/opentype/otspec/loca.htm

namespace ots {

bool ots_loca_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // We can't do anything useful in validating this data except to ensure that
  // the values are monotonically increasing.

  OpenTypeLOCA *loca = new OpenTypeLOCA;
  file->loca = loca;

  if (!file->maxp || !file->head) {
    return OTS_FAILURE();
  }

  const unsigned num_glyphs = file->maxp->num_glyphs;
  unsigned last_offset = 0;
  loca->offsets.resize(num_glyphs + 1);
  // maxp->num_glyphs is uint16_t, thus the addition never overflows.

  if (file->head->index_to_loc_format == 0) {
    // Note that the <= here (and below) is correct. There is one more offset
    // than the number of glyphs in order to give the length of the final
    // glyph.
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint16_t offset = 0;
      if (!table.ReadU16(&offset)) {
        return OTS_FAILURE();
      }
      if (offset < last_offset) {
        return OTS_FAILURE();
      }
      last_offset = offset;
      loca->offsets[i] = offset * 2;
    }
  } else {
    for (unsigned i = 0; i <= num_glyphs; ++i) {
      uint32_t offset = 0;
      if (!table.ReadU32(&offset)) {
        return OTS_FAILURE();
      }
      if (offset < last_offset) {
        return OTS_FAILURE();
      }
      last_offset = offset;
      loca->offsets[i] = offset;
    }
  }

  return true;
}

bool ots_loca_should_serialise(OpenTypeFile *file) {
  return file->loca;
}

bool ots_loca_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeLOCA *loca = file->loca;
  const OpenTypeHEAD *head = file->head;

  if (!head) {
    return OTS_FAILURE();
  }

  if (head->index_to_loc_format == 0) {
    for (unsigned i = 0; i < loca->offsets.size(); ++i) {
      if (!out->WriteU16(loca->offsets[i] >> 1)) {
        return OTS_FAILURE();
      }
    }
  } else {
    for (unsigned i = 0; i < loca->offsets.size(); ++i) {
      if (!out->WriteU32(loca->offsets[i])) {
        return OTS_FAILURE();
      }
    }
  }

  return true;
}

void ots_loca_free(OpenTypeFile *file) {
  delete file->loca;
}

}  // namespace ots
