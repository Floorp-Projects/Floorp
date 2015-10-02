// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "head.h"

#include <cstring>

// head - Font Header
// http://www.microsoft.com/typography/otspec/head.htm

#define TABLE_NAME "head"

namespace ots {

bool ots_head_parse(Font* font, const uint8_t *data, size_t length) {
  Buffer table(data, length);
  OpenTypeHEAD *head = new OpenTypeHEAD;
  font->head = head;

  uint32_t version = 0;
  if (!table.ReadU32(&version) ||
      !table.ReadU32(&head->revision)) {
    return OTS_FAILURE_MSG("Failed to read head header");
  }

  if (version >> 16 != 1) {
    return OTS_FAILURE_MSG("Bad head table version of %d", version);
  }

  // Skip the checksum adjustment
  if (!table.Skip(4)) {
    return OTS_FAILURE_MSG("Failed to read checksum");
  }

  uint32_t magic;
  if (!table.ReadU32(&magic) || magic != 0x5F0F3CF5) {
    return OTS_FAILURE_MSG("Failed to read font magic number");
  }

  if (!table.ReadU16(&head->flags)) {
    return OTS_FAILURE_MSG("Failed to read head flags");
  }

  // We allow bits 0..4, 11..13
  head->flags &= 0x381f;

  if (!table.ReadU16(&head->ppem)) {
    return OTS_FAILURE_MSG("Failed to read pixels per em");
  }

  // ppem must be in range
  if (head->ppem < 16 ||
      head->ppem > 16384) {
    return OTS_FAILURE_MSG("Bad ppm of %d", head->ppem);
  }

  // ppem must be a power of two
#if 0
  // We don't call ots_failure() for now since lots of TrueType fonts are
  // not following this rule. Putting OTS_WARNING here is too noisy.
  if ((head->ppem - 1) & head->ppem) {
    return OTS_FAILURE_MSG("ppm not a power of two: %d", head->ppem);
  }
#endif

  if (!table.ReadR64(&head->created) ||
      !table.ReadR64(&head->modified)) {
    return OTS_FAILURE_MSG("Can't read font dates");
  }

  if (!table.ReadS16(&head->xmin) ||
      !table.ReadS16(&head->ymin) ||
      !table.ReadS16(&head->xmax) ||
      !table.ReadS16(&head->ymax)) {
    return OTS_FAILURE_MSG("Failed to read font bounding box");
  }

  if (head->xmin > head->xmax) {
    return OTS_FAILURE_MSG("Bad x dimension in the font bounding box (%d, %d)", head->xmin, head->xmax);
  }
  if (head->ymin > head->ymax) {
    return OTS_FAILURE_MSG("Bad y dimension in the font bounding box (%d, %d)", head->ymin, head->ymax);
  }

  if (!table.ReadU16(&head->mac_style)) {
    return OTS_FAILURE_MSG("Failed to read font style");
  }

  // We allow bits 0..6
  head->mac_style &= 0x7f;

  if (!table.ReadU16(&head->min_ppem)) {
    return OTS_FAILURE_MSG("Failed to read font minimum ppm");
  }

  // We don't care about the font direction hint
  if (!table.Skip(2)) {
    return OTS_FAILURE_MSG("Failed to skip font direction hint");
  }

  if (!table.ReadS16(&head->index_to_loc_format)) {
    return OTS_FAILURE_MSG("Failed to read index to loc format");
  }
  if (head->index_to_loc_format < 0 ||
      head->index_to_loc_format > 1) {
    return OTS_FAILURE_MSG("Bad index to loc format %d", head->index_to_loc_format);
  }

  int16_t glyph_data_format;
  if (!table.ReadS16(&glyph_data_format) ||
      glyph_data_format) {
    return OTS_FAILURE_MSG("Failed to read glyph data format");
  }

  return true;
}

bool ots_head_should_serialise(Font *font) {
  return font->head != NULL;
}

bool ots_head_serialise(OTSStream *out, Font *font) {
  const OpenTypeHEAD *head = font->head;
  if (!out->WriteU32(0x00010000) ||
      !out->WriteU32(head->revision) ||
      !out->WriteU32(0) ||  // check sum not filled in yet
      !out->WriteU32(0x5F0F3CF5) ||
      !out->WriteU16(head->flags) ||
      !out->WriteU16(head->ppem) ||
      !out->WriteR64(head->created) ||
      !out->WriteR64(head->modified) ||
      !out->WriteS16(head->xmin) ||
      !out->WriteS16(head->ymin) ||
      !out->WriteS16(head->xmax) ||
      !out->WriteS16(head->ymax) ||
      !out->WriteU16(head->mac_style) ||
      !out->WriteU16(head->min_ppem) ||
      !out->WriteS16(2) ||
      !out->WriteS16(head->index_to_loc_format) ||
      !out->WriteS16(0)) {
    return OTS_FAILURE_MSG("Failed to write head table");
  }

  return true;
}

void ots_head_reuse(Font *font, Font *other) {
  font->head = other->head;
  font->head_reused = true;
}

void ots_head_free(Font *font) {
  delete font->head;
}

}  // namespace

#undef TABLE_NAME
