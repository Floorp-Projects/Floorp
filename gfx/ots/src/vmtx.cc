// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vmtx.h"

#include "maxp.h"
#include "vhea.h"

// vmtx - Vertical Metrics Table
// http://www.microsoft.com/typography/otspec/vmtx.htm

#define TABLE_NAME "vmtx"

namespace ots {

bool ots_vmtx_parse(Font *font, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeVMTX *vmtx = new OpenTypeVMTX;
  font->vmtx = vmtx;

  if (!font->vhea || !font->maxp) {
    return OTS_FAILURE_MSG("vhea or maxp table missing as needed by vmtx");
  }

  if (!ParseMetricsTable(font, &table, font->maxp->num_glyphs,
                         &font->vhea->header, &vmtx->metrics)) {
    return OTS_FAILURE_MSG("Failed to parse vmtx metrics");
  }

  return true;
}

bool ots_vmtx_should_serialise(Font *font) {
  // vmtx should serialise when vhea is preserved.
  return font->vmtx != NULL && font->vhea != NULL;
}

bool ots_vmtx_serialise(OTSStream *out, Font *font) {
  if (!SerialiseMetricsTable(font, out, &font->vmtx->metrics)) {
    return OTS_FAILURE_MSG("Failed to write vmtx metrics");
  }
  return true;
}

void ots_vmtx_reuse(Font *font, Font *other) {
  font->vmtx = other->vmtx;
  font->vmtx_reused = true;
}

void ots_vmtx_free(Font *font) {
  delete font->vmtx;
}

}  // namespace ots

#undef TABLE_NAME
