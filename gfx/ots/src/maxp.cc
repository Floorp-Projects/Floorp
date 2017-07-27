// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "maxp.h"

// maxp - Maximum Profile
// http://www.microsoft.com/typography/otspec/maxp.htm

namespace ots {

bool OpenTypeMAXP::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint32_t version = 0;
  if (!table.ReadU32(&version)) {
    return Error("Failed to read table version");
  }

  if (version >> 16 > 1) {
    return Error("Unsupported table version 0x%x", version);
  }

  if (!table.ReadU16(&this->num_glyphs)) {
    return Error("Failed to read numGlyphs");
  }

  if (!this->num_glyphs) {
    return Error("numGlyphs is 0");
  }

  if (version >> 16 == 1) {
    this->version_1 = true;
    if (!table.ReadU16(&this->max_points) ||
        !table.ReadU16(&this->max_contours) ||
        !table.ReadU16(&this->max_c_points) ||
        !table.ReadU16(&this->max_c_contours) ||
        !table.ReadU16(&this->max_zones) ||
        !table.ReadU16(&this->max_t_points) ||
        !table.ReadU16(&this->max_storage) ||
        !table.ReadU16(&this->max_fdefs) ||
        !table.ReadU16(&this->max_idefs) ||
        !table.ReadU16(&this->max_stack) ||
        !table.ReadU16(&this->max_size_glyf_instructions) ||
        !table.ReadU16(&this->max_c_components) ||
        !table.ReadU16(&this->max_c_depth)) {
      return Error("Failed to read version 1 table data");
    }

    if (this->max_zones == 0) {
      // workaround for ipa*.ttf Japanese fonts.
      Warning("Bad maxZones: %u", this->max_zones);
      this->max_zones = 1;
    } else if (this->max_zones == 3) {
      // workaround for Ecolier-*.ttf fonts.
      Warning("Bad maxZones: %u", this->max_zones);
      this->max_zones = 2;
    }

    if ((this->max_zones != 1) && (this->max_zones != 2)) {
      return Error("Bad maxZones: %u", this->max_zones);
    }
  } else {
    this->version_1 = false;
  }

  return true;
}

bool OpenTypeMAXP::Serialize(OTSStream *out) {
  if (!out->WriteU32(this->version_1 ? 0x00010000 : 0x00005000) ||
      !out->WriteU16(this->num_glyphs)) {
    return Error("Failed to write version or numGlyphs");
  }

  if (!this->version_1) return true;

  if (!out->WriteU16(this->max_points) ||
      !out->WriteU16(this->max_contours) ||
      !out->WriteU16(this->max_c_points) ||
      !out->WriteU16(this->max_c_contours)) {
    return Error("Failed to write maxp");
  }

  if (!out->WriteU16(this->max_zones) ||
      !out->WriteU16(this->max_t_points) ||
      !out->WriteU16(this->max_storage) ||
      !out->WriteU16(this->max_fdefs) ||
      !out->WriteU16(this->max_idefs) ||
      !out->WriteU16(this->max_stack) ||
      !out->WriteU16(this->max_size_glyf_instructions)) {
    return Error("Failed to write more maxp");
  }

  if (!out->WriteU16(this->max_c_components) ||
      !out->WriteU16(this->max_c_depth)) {
    return Error("Failed to write yet more maxp");
  }

  return true;
}

}  // namespace ots
