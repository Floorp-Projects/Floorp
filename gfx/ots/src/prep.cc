// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "prep.h"

// prep - Control Value Program
// http://www.microsoft.com/typography/otspec/prep.htm

#define TABLE_NAME "prep"

namespace ots {

bool ots_prep_parse(Font *font, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypePREP *prep = new OpenTypePREP;
  font->prep = prep;

  if (length >= 128 * 1024u) {
    return OTS_FAILURE_MSG("table length %ld > 120K", length);  // almost all prep tables are less than 9k bytes.
  }

  if (!table.Skip(length)) {
    return OTS_FAILURE_MSG("Failed to read table of length %ld", length);
  }

  prep->data = data;
  prep->length = length;
  return true;
}

bool ots_prep_should_serialise(Font *font) {
  if (!font->glyf) return false;  // this table is not for CFF fonts.
  return font->prep != NULL;
}

bool ots_prep_serialise(OTSStream *out, Font *font) {
  const OpenTypePREP *prep = font->prep;

  if (!out->Write(prep->data, prep->length)) {
    return OTS_FAILURE_MSG("Failed to write table length");
  }

  return true;
}

void ots_prep_reuse(Font *font, Font *other) {
  font->prep = other->prep;
  font->prep_reused = true;
}

void ots_prep_free(Font *font) {
  delete font->prep;
}

}  // namespace ots

#undef TABLE_NAME
