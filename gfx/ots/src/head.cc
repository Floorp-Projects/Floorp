// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "head.h"

#include <cstring>

// head - Font Header
// http://www.microsoft.com/typography/otspec/head.htm

namespace ots {

bool OpenTypeHEAD::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint32_t version = 0;
  if (!table.ReadU32(&version) ||
      !table.ReadU32(&this->revision)) {
    return Error("Failed to read table header");
  }

  if (version >> 16 != 1) {
    return Error("Unsupported majorVersion: %d", version >> 16);
  }

  // Skip the checksum adjustment
  if (!table.Skip(4)) {
    return Error("Failed to read checksum");
  }

  uint32_t magic;
  if (!table.ReadU32(&magic) || magic != 0x5F0F3CF5) {
    return Error("Failed to read or incorrect magicNumber");
  }

  if (!table.ReadU16(&this->flags)) {
    return Error("Failed to read flags");
  }

  // We allow bits 0..4, 11..13
  this->flags &= 0x381f;

  if (!table.ReadU16(&this->upem)) {
    return Error("Failed to read unitsPerEm");
  }

  // upem must be in range
  if (this->upem < 16 ||
      this->upem > 16384) {
    return Error("unitsPerEm on in the range [16, 16384]: %d", this->upem);
  }

  if (!table.ReadR64(&this->created) ||
      !table.ReadR64(&this->modified)) {
    return Error("Can't read font dates");
  }

  if (!table.ReadS16(&this->xmin) ||
      !table.ReadS16(&this->ymin) ||
      !table.ReadS16(&this->xmax) ||
      !table.ReadS16(&this->ymax)) {
    return Error("Failed to read font bounding box");
  }

  if (this->xmin > this->xmax) {
    return Error("Bad x dimension in the font bounding box (%d, %d)",
                  this->xmin, this->xmax);
  }
  if (this->ymin > this->ymax) {
    return Error("Bad y dimension in the font bounding box (%d, %d)",
                  this->ymin, this->ymax);
  }

  if (!table.ReadU16(&this->mac_style)) {
    return Error("Failed to read macStyle");
  }

  // We allow bits 0..6
  this->mac_style &= 0x7f;

  if (!table.ReadU16(&this->min_ppem)) {
    return Error("Failed to read lowestRecPPEM");
  }

  // We don't care about the font direction hint
  if (!table.Skip(2)) {
    return Error("Failed to read fontDirectionHint");
  }

  if (!table.ReadS16(&this->index_to_loc_format)) {
    return Error("Failed to read indexToLocFormat");
  }
  if (this->index_to_loc_format < 0 ||
      this->index_to_loc_format > 1) {
    return Error("Bad indexToLocFormat %d", this->index_to_loc_format);
  }

  int16_t glyph_data_format;
  if (!table.ReadS16(&glyph_data_format) ||
      glyph_data_format) {
    return Error("Failed to read or bad glyphDataFormat");
  }

  return true;
}

bool OpenTypeHEAD::Serialize(OTSStream *out) {
  if (!out->WriteU32(0x00010000) ||
      !out->WriteU32(this->revision) ||
      !out->WriteU32(0) ||  // check sum not filled in yet
      !out->WriteU32(0x5F0F3CF5) ||
      !out->WriteU16(this->flags) ||
      !out->WriteU16(this->upem) ||
      !out->WriteR64(this->created) ||
      !out->WriteR64(this->modified) ||
      !out->WriteS16(this->xmin) ||
      !out->WriteS16(this->ymin) ||
      !out->WriteS16(this->xmax) ||
      !out->WriteS16(this->ymax) ||
      !out->WriteU16(this->mac_style) ||
      !out->WriteU16(this->min_ppem) ||
      !out->WriteS16(2) ||
      !out->WriteS16(this->index_to_loc_format) ||
      !out->WriteS16(0)) {
    return Error("Failed to write table");
  }

  return true;
}

}  // namespace
