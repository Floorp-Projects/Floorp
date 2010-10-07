// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hhea.h"

#include "head.h"
#include "maxp.h"

// hhea - Horizontal Header
// http://www.microsoft.com/opentype/otspec/hhea.htm

namespace ots {

bool ots_hhea_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHHEA *hhea = new OpenTypeHHEA;
  file->hhea = hhea;

  uint32_t version = 0;
  if (!table.ReadU32(&version)) {
    return OTS_FAILURE();
  }
  if (version >> 16 != 1) {
    return OTS_FAILURE();
  }

  if (!table.ReadS16(&hhea->ascent) ||
      !table.ReadS16(&hhea->descent) ||
      !table.ReadS16(&hhea->linegap) ||
      !table.ReadU16(&hhea->adv_width_max) ||
      !table.ReadS16(&hhea->min_lsb) ||
      !table.ReadS16(&hhea->min_rsb) ||
      !table.ReadS16(&hhea->x_max_extent) ||
      !table.ReadS16(&hhea->caret_slope_rise) ||
      !table.ReadS16(&hhea->caret_slope_run) ||
      !table.ReadS16(&hhea->caret_offset)) {
    return OTS_FAILURE();
  }

  if (hhea->ascent < 0) {
    OTS_WARNING("bad ascent: %d", hhea->ascent);
    hhea->ascent = 0;
  }
  if (hhea->linegap < 0) {
    OTS_WARNING("bad linegap: %d", hhea->linegap);
    hhea->linegap = 0;
  }

  if (!file->head) {
    return OTS_FAILURE();
  }

  // if the font is non-slanted, caret_offset should be zero.
  if (!(file->head->mac_style & 2) &&
      (hhea->caret_offset != 0)) {
    OTS_WARNING("bad caret offset: %d", hhea->caret_offset);
    hhea->caret_offset = 0;
  }

  // skip the reserved bytes
  if (!table.Skip(8)) {
    return OTS_FAILURE();
  }

  int16_t data_format;
  if (!table.ReadS16(&data_format)) {
    return OTS_FAILURE();
  }
  if (data_format) {
    return OTS_FAILURE();
  }

  if (!table.ReadU16(&hhea->num_hmetrics)) {
    return OTS_FAILURE();
  }

  if (!file->maxp) {
    return OTS_FAILURE();
  }

  if (hhea->num_hmetrics > file->maxp->num_glyphs) {
    return OTS_FAILURE();
  }

  return true;
}

bool ots_hhea_should_serialise(OpenTypeFile *file) {
  return file->hhea;
}

bool ots_hhea_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeHHEA *hhea = file->hhea;

  if (!out->WriteU32(0x00010000) ||
      !out->WriteS16(hhea->ascent) ||
      !out->WriteS16(hhea->descent) ||
      !out->WriteS16(hhea->linegap) ||
      !out->WriteU16(hhea->adv_width_max) ||
      !out->WriteS16(hhea->min_lsb) ||
      !out->WriteS16(hhea->min_rsb) ||
      !out->WriteS16(hhea->x_max_extent) ||
      !out->WriteS16(hhea->caret_slope_rise) ||
      !out->WriteS16(hhea->caret_slope_run) ||
      !out->WriteS16(hhea->caret_offset) ||
      !out->WriteR64(0) ||  // reserved
      !out->WriteS16(0) ||  // metric data format
      !out->WriteU16(hhea->num_hmetrics)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_hhea_free(OpenTypeFile *file) {
  delete file->hhea;
}

}  // namespace ots
