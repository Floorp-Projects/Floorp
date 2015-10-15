// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hmtx.h"

#include "hhea.h"
#include "maxp.h"

// hmtx - Horizontal Metrics
// http://www.microsoft.com/typography/otspec/hmtx.htm

#define TABLE_NAME "hmtx"

namespace ots {

bool ots_hmtx_parse(Font *font, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHMTX *hmtx = new OpenTypeHMTX;
  font->hmtx = hmtx;

  if (!font->hhea || !font->maxp) {
    return OTS_FAILURE_MSG("Missing hhea or maxp tables in font, needed by hmtx");
  }

  if (!ParseMetricsTable(font, &table, font->maxp->num_glyphs,
                         &font->hhea->header, &hmtx->metrics)) {
    return OTS_FAILURE_MSG("Failed to parse hmtx metrics");
  }

  return true;
}

bool ots_hmtx_should_serialise(Font *font) {
  return font->hmtx != NULL;
}

bool ots_hmtx_serialise(OTSStream *out, Font *font) {
  if (!SerialiseMetricsTable(font, out, &font->hmtx->metrics)) {
    return OTS_FAILURE_MSG("Failed to serialise htmx metrics");
  }
  return true;
}

void ots_hmtx_reuse(Font *font, Font *other) {
  font->hmtx = other->hmtx;
  font->hmtx_reused = true;
}

void ots_hmtx_free(Font *font) {
  delete font->hmtx;
}

}  // namespace ots

#undef TABLE_NAME
