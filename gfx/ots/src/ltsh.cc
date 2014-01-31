// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ltsh.h"

#include "maxp.h"

// LTSH - Linear Threshold
// http://www.microsoft.com/typography/otspec/ltsh.htm

#define TABLE_NAME "LTSH"

#define DROP_THIS_TABLE \
  do { \
    delete file->ltsh; \
    file->ltsh = 0; \
    OTS_FAILURE_MSG("Table discarded"); \
  } while (0)

namespace ots {

bool ots_ltsh_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!file->maxp) {
    return OTS_FAILURE_MSG("Missing maxp table from font needed by ltsh");
  }

  OpenTypeLTSH *ltsh = new OpenTypeLTSH;
  file->ltsh = ltsh;

  uint16_t num_glyphs = 0;
  if (!table.ReadU16(&ltsh->version) ||
      !table.ReadU16(&num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to read ltsh header");
  }

  if (ltsh->version != 0) {
    OTS_WARNING("bad version: %u", ltsh->version);
    DROP_THIS_TABLE;
    return true;
  }

  if (num_glyphs != file->maxp->num_glyphs) {
    OTS_WARNING("bad num_glyphs: %u", num_glyphs);
    DROP_THIS_TABLE;
    return true;
  }

  ltsh->ypels.reserve(num_glyphs);
  for (unsigned i = 0; i < num_glyphs; ++i) {
    uint8_t pel = 0;
    if (!table.ReadU8(&pel)) {
      return OTS_FAILURE_MSG("Failed to read pixels for glyph %d", i);
    }
    ltsh->ypels.push_back(pel);
  }

  return true;
}

bool ots_ltsh_should_serialise(OpenTypeFile *file) {
  if (!file->glyf) return false;  // this table is not for CFF fonts.
  return file->ltsh != NULL;
}

bool ots_ltsh_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeLTSH *ltsh = file->ltsh;

  if (!out->WriteU16(ltsh->version) ||
      !out->WriteU16(ltsh->ypels.size())) {
    return OTS_FAILURE_MSG("Failed to write pels size");
  }
  for (unsigned i = 0; i < ltsh->ypels.size(); ++i) {
    if (!out->Write(&(ltsh->ypels[i]), 1)) {
      return OTS_FAILURE_MSG("Failed to write pixel size for glyph %d", i);
    }
  }

  return true;
}

void ots_ltsh_free(OpenTypeFile *file) {
  delete file->ltsh;
}

}  // namespace ots
