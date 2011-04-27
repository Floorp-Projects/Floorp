// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "maxp.h"

// maxp - Maximum Profile
// http://www.microsoft.com/opentype/otspec/maxp.htm

namespace ots {

bool ots_maxp_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeMAXP *maxp = new OpenTypeMAXP;
  file->maxp = maxp;

  uint32_t version = 0;
  if (!table.ReadU32(&version)) {
    return OTS_FAILURE();
  }

  if (version >> 16 > 1) {
    return OTS_FAILURE();
  }

  if (!table.ReadU16(&maxp->num_glyphs)) {
    return OTS_FAILURE();
  }

  if (!maxp->num_glyphs) {
    return OTS_FAILURE();
  }

  if (version >> 16 == 1) {
    maxp->version_1 = true;
    if (!table.ReadU16(&maxp->max_points) ||
        !table.ReadU16(&maxp->max_contours) ||
        !table.ReadU16(&maxp->max_c_points) ||
        !table.ReadU16(&maxp->max_c_contours) ||
        !table.ReadU16(&maxp->max_zones) ||
        !table.ReadU16(&maxp->max_t_points) ||
        !table.ReadU16(&maxp->max_storage) ||
        !table.ReadU16(&maxp->max_fdefs) ||
        !table.ReadU16(&maxp->max_idefs) ||
        !table.ReadU16(&maxp->max_stack) ||
        !table.ReadU16(&maxp->max_size_glyf_instructions) ||
        !table.ReadU16(&maxp->max_c_components) ||
        !table.ReadU16(&maxp->max_c_depth)) {
      return OTS_FAILURE();
    }

    if (maxp->max_zones == 0) {
      // workaround for ipa*.ttf Japanese fonts.
      OTS_WARNING("bad max_zones: %u", maxp->max_zones);
      maxp->max_zones = 1;
    } else if (maxp->max_zones == 3) {
      // workaround for Ecolier-*.ttf fonts.
      OTS_WARNING("bad max_zones: %u", maxp->max_zones);
      maxp->max_zones = 2;
    }

    if ((maxp->max_zones != 1) && (maxp->max_zones != 2)) {
      return OTS_FAILURE();
    }
  } else {
    maxp->version_1 = false;
  }

  return true;
}

bool ots_maxp_should_serialise(OpenTypeFile *file) {
  return file->maxp != NULL;
}

bool ots_maxp_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypeMAXP *maxp = file->maxp;

  if (!out->WriteU32(maxp->version_1 ? 0x00010000 : 0x00005000) ||
      !out->WriteU16(maxp->num_glyphs)) {
    return OTS_FAILURE();
  }

  if (!maxp->version_1) return true;

  if (!out->WriteU16(maxp->max_points) ||
      !out->WriteU16(maxp->max_contours) ||
      !out->WriteU16(maxp->max_c_points) ||
      !out->WriteU16(maxp->max_c_contours)) {
    return OTS_FAILURE();
  }

  if (g_transcode_hints) {
    if (!out->WriteU16(maxp->max_zones) ||
        !out->WriteU16(maxp->max_t_points) ||
        !out->WriteU16(maxp->max_storage) ||
        !out->WriteU16(maxp->max_fdefs) ||
        !out->WriteU16(maxp->max_idefs) ||
        !out->WriteU16(maxp->max_stack) ||
        !out->WriteU16(maxp->max_size_glyf_instructions)) {
      return OTS_FAILURE();
    }
  } else {
    if (!out->WriteU16(1) ||  // max zones
        !out->WriteU16(0) ||  // max twilight points
        !out->WriteU16(0) ||  // max storage
        !out->WriteU16(0) ||  // max function defs
        !out->WriteU16(0) ||  // max instruction defs
        !out->WriteU16(0) ||  // max stack elements
        !out->WriteU16(0)) {  // max instruction byte count
      return OTS_FAILURE();
    }
  }

  if (!out->WriteU16(maxp->max_c_components) ||
      !out->WriteU16(maxp->max_c_depth)) {
    return OTS_FAILURE();
  }

  return true;
}

void ots_maxp_free(OpenTypeFile *file) {
  delete file->maxp;
}

}  // namespace ots
