// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ltsh.h"

#include "maxp.h"

// LTSH - Linear Threshold
// http://www.microsoft.com/typography/otspec/ltsh.htm

#define TABLE_NAME "LTSH"

#define DROP_THIS_TABLE(...) \
  do { \
    OTS_FAILURE_MSG_(font->file, TABLE_NAME ": " __VA_ARGS__); \
    OTS_FAILURE_MSG("Table discarded"); \
    delete font->ltsh; \
    font->ltsh = 0; \
  } while (0)

namespace ots {

bool ots_ltsh_parse(Font *font, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!font->maxp) {
    return OTS_FAILURE_MSG("Missing maxp table from font needed by ltsh");
  }

  OpenTypeLTSH *ltsh = new OpenTypeLTSH;
  font->ltsh = ltsh;

  uint16_t num_glyphs = 0;
  if (!table.ReadU16(&ltsh->version) ||
      !table.ReadU16(&num_glyphs)) {
    return OTS_FAILURE_MSG("Failed to read ltsh header");
  }

  if (ltsh->version != 0) {
    DROP_THIS_TABLE("bad version: %u", ltsh->version);
    return true;
  }

  if (num_glyphs != font->maxp->num_glyphs) {
    DROP_THIS_TABLE("bad num_glyphs: %u", num_glyphs);
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

bool ots_ltsh_should_serialise(Font *font) {
  if (!font->glyf) return false;  // this table is not for CFF fonts.
  return font->ltsh != NULL;
}

bool ots_ltsh_serialise(OTSStream *out, Font *font) {
  const OpenTypeLTSH *ltsh = font->ltsh;

  const uint16_t num_ypels = static_cast<uint16_t>(ltsh->ypels.size());
  if (num_ypels != ltsh->ypels.size() ||
      !out->WriteU16(ltsh->version) ||
      !out->WriteU16(num_ypels)) {
    return OTS_FAILURE_MSG("Failed to write pels size");
  }
  for (uint16_t i = 0; i < num_ypels; ++i) {
    if (!out->Write(&(ltsh->ypels[i]), 1)) {
      return OTS_FAILURE_MSG("Failed to write pixel size for glyph %d", i);
    }
  }

  return true;
}

void ots_ltsh_reuse(Font *font, Font *other) {
  font->ltsh = other->ltsh;
  font->ltsh_reused = true;
}

void ots_ltsh_free(Font *font) {
  delete font->ltsh;
}

}  // namespace ots

#undef TABLE_NAME
#undef DROP_THIS_TABLE
