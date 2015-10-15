// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hhea.h"

#include "head.h"
#include "maxp.h"

// hhea - Horizontal Header
// http://www.microsoft.com/typography/otspec/hhea.htm

#define TABLE_NAME "hhea"

namespace ots {

bool ots_hhea_parse(Font *font, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHHEA *hhea = new OpenTypeHHEA;
  font->hhea = hhea;

  if (!table.ReadU32(&hhea->header.version)) {
    return OTS_FAILURE_MSG("Failed to read hhea version");
  }
  if (hhea->header.version >> 16 != 1) {
    return OTS_FAILURE_MSG("Bad hhea version of %d", hhea->header.version);
  }

  if (!ParseMetricsHeader(font, &table, &hhea->header)) {
    return OTS_FAILURE_MSG("Failed to parse horizontal metrics");
  }

  return true;
}

bool ots_hhea_should_serialise(Font *font) {
  return font->hhea != NULL;
}

bool ots_hhea_serialise(OTSStream *out, Font *font) {
  if (!SerialiseMetricsHeader(font, out, &font->hhea->header)) {
    return OTS_FAILURE_MSG("Failed to serialise horizontal metrics");
  }
  return true;
}

void ots_hhea_reuse(Font *font, Font *other) {
  font->hhea = other->hhea;
  font->hhea_reused = true;
}

void ots_hhea_free(Font *font) {
  delete font->hhea;
}

}  // namespace ots

#undef TABLE_NAME
