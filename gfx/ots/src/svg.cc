// Copyright (c) 2012 Mozilla Foundation. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ots.h"

#include "svg.h"

#define NONFATAL_FAILURE(msg) \
  do { \
    OTS_WARNING(msg); \
    delete file->svg; file->svg = 0; \
    return true; \
  } while (0)

namespace ots {

bool ots_svg_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeSVG *svg = new OpenTypeSVG;
  file->svg = svg;

  uint16_t version;
  if (!table.ReadU16(&version)) {
    NONFATAL_FAILURE("Couldn't read SVG table header");
  }
  if (version != 0) {
    NONFATAL_FAILURE("Unknown SVG table version");
  }

  uint32_t doc_index_offset;
  if (!table.ReadU32(&doc_index_offset)) {
    NONFATAL_FAILURE("Couldn't read doc index offset from SVG table header");
  }
  if (doc_index_offset == 0 || doc_index_offset >= length) {
    NONFATAL_FAILURE("Invalid doc index offset");
  }

  uint32_t color_palettes_offset;
  if (!table.ReadU32(&color_palettes_offset)) {
    NONFATAL_FAILURE("Couldn't read color palettes offset from SVG table header");
  }
  if (color_palettes_offset >= length) {
    NONFATAL_FAILURE("Invalid doc index offset");
  }

  uint16_t start_glyph;
  uint16_t end_glyph;
  uint32_t doc_offset;
  uint32_t doc_length;
  uint16_t last_end_glyph = 0;

  table.set_offset(doc_index_offset);
  uint16_t index_length;
  if (!table.ReadU16(&index_length)) {
    NONFATAL_FAILURE("Couldn't read SVG documents index");
  }
  if (index_length == 0) {
    NONFATAL_FAILURE("Zero-length documents index");
  }

  for (uint16_t i = 0; i < index_length; i++) {
    if (!table.ReadU16(&start_glyph) ||
        !table.ReadU16(&end_glyph) ||
        !table.ReadU32(&doc_offset) ||
        !table.ReadU32(&doc_length)) {
      NONFATAL_FAILURE("Couldn't read SVG table index");
    }

    if (end_glyph < start_glyph) {
      NONFATAL_FAILURE("Bad SVG table index range");
    }

    if (last_end_glyph && start_glyph <= last_end_glyph) {
      NONFATAL_FAILURE("SVG table index range overlapping or not sorted");
    }

    if (doc_offset > 1024 * 1024 * 1024 ||
        doc_length > 1024 * 1024 * 1024) {
      NONFATAL_FAILURE("Bad SVG document length");
    }

    if (uint64_t(doc_index_offset) + doc_offset + doc_length > length) {
      NONFATAL_FAILURE("SVG table document overflows table");
    }

    last_end_glyph = end_glyph;
  }

  svg->data = data;
  svg->length = length;

  return true;
}

bool ots_svg_serialise(OTSStream *out, OpenTypeFile *file) {
  OpenTypeSVG *svg = file->svg;

  if (!out->Write(svg->data, svg->length)) {
    return OTS_FAILURE();
  }

  return true;
}

bool ots_svg_should_serialise(OpenTypeFile *file) {
  return file->svg;
}

void ots_svg_free(OpenTypeFile *file) {
  delete file->svg;
}

}
