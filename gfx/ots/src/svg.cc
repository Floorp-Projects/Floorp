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

  std::map<uint32_t, uint32_t> doc_locations;
  typedef std::map<uint32_t, uint32_t>::iterator lociter_t;

  uint16_t version;
  uint16_t index_length;
  if (!table.ReadU16(&version) ||
      !table.ReadU16(&index_length)) {
    NONFATAL_FAILURE("Couldn't read SVG table header");
  }

  if (version != 0) {
    NONFATAL_FAILURE("Unknown SVG table version");
  }

  uint32_t max_address = 0;
  uint32_t total_docs_length = 0;

  uint16_t start_glyph;
  uint16_t end_glyph;
  uint32_t doc_offset;
  uint32_t doc_length;
  uint16_t last_end_glyph = 0;

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

    if (doc_locations.find(doc_offset) != doc_locations.end()) {
      if (doc_locations[doc_offset] != doc_length) {
        NONFATAL_FAILURE("SVG table contains overlapping document range");
      }
    } else {
      doc_locations[doc_offset] = doc_length;
      total_docs_length += doc_length;
      if (doc_offset + doc_length > max_address) {
        max_address = doc_offset + doc_length;
      }
    }

    if (doc_offset > 1024 * 1024 * 1024 ||
        doc_length > 1024 * 1024 * 1024 ||
        total_docs_length > 1024 * 1024 * 1024) {
      NONFATAL_FAILURE("Bad SVG document length");
    }

    last_end_glyph = end_glyph;
  }

  uint32_t last_end = 4 + 12 * index_length;
  for (lociter_t iter = doc_locations.begin();
       iter != doc_locations.end(); ++iter) {
    if (iter->first != last_end) {
      NONFATAL_FAILURE("SVG table contains overlapping document range");
    }
    last_end = iter->first + iter->second;
  }

  if (max_address != length) {
    NONFATAL_FAILURE("Bad SVG document length");
  }

  if (!table.Skip(total_docs_length)) {
    NONFATAL_FAILURE("SVG table is too short");
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
