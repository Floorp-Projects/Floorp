// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kern.h"

// kern - Kerning
// http://www.microsoft.com/typography/otspec/kern.htm

namespace ots {

bool OpenTypeKERN::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint16_t num_tables = 0;
  if (!table.ReadU16(&this->version) ||
      !table.ReadU16(&num_tables)) {
    return Error("Failed to read table header");
  }

  if (this->version > 0) {
    return Drop("Unsupported table version: %d", this->version);
  }

  if (num_tables == 0) {
    return Drop("nTables is zero");
  }

  this->subtables.reserve(num_tables);
  for (unsigned i = 0; i < num_tables; ++i) {
    OpenTypeKERNFormat0 subtable;
    uint16_t sub_length = 0;

    if (!table.ReadU16(&subtable.version) ||
        !table.ReadU16(&sub_length)) {
      return Error("Failed to read subtable %d header", i);
    }

    if (subtable.version > 0) {
      Warning("Ignoring subtable %d with unsupported version: %d",
              i, subtable.version);
      continue;
    }

    const size_t current_offset = table.offset();
    if (current_offset - 4 + sub_length > length) {
      return Error("Bad subtable %d offset %ld", i, current_offset);
    }

    if (!table.ReadU16(&subtable.coverage)) {
      return Error("Failed to read subtable %d coverage", i);
    }

    if (!(subtable.coverage & 0x1)) {
      Warning(
          "We don't support vertical data as the renderer doesn't support it.");
      continue;
    }
    if (subtable.coverage & 0xF0) {
      return Drop("Reserved fields should be zero");
    }
    const uint32_t format = (subtable.coverage & 0xFF00) >> 8;
    if (format != 0) {
      Warning("Ignoring subtable %d with unsupported format: %d", i, format);
      continue;
    }

    // Parse the format 0 field.
    uint16_t num_pairs = 0;
    if (!table.ReadU16(&num_pairs) ||
        !table.ReadU16(&subtable.search_range) ||
        !table.ReadU16(&subtable.entry_selector) ||
        !table.ReadU16(&subtable.range_shift)) {
      return Error("Failed to read subtable %d format 0 fields", i);
    }

    if (!num_pairs) {
      return Drop("Zero length subtable is found");
    }

    // Sanity checks for search_range, entry_selector, and range_shift. See the
    // comment in ots.cc for details.
    const size_t kFormat0PairSize = 6;  // left, right, and value. 2 bytes each.
    if (num_pairs > (65536 / kFormat0PairSize)) {
      // Some fonts (e.g. calibri.ttf, pykes_peak_zero.ttf) have pairs >= 10923.
      return Drop("Too large subtable");
    }
    unsigned max_pow2 = 0;
    while (1u << (max_pow2 + 1) <= num_pairs) {
      ++max_pow2;
    }
    const uint16_t expected_search_range = (1u << max_pow2) * kFormat0PairSize;
    if (subtable.search_range != expected_search_range) {
      Warning("bad search range");
      subtable.search_range = expected_search_range;
    }
    if (subtable.entry_selector != max_pow2) {
      return Error("Bad subtable %d entry selector %d", i, subtable.entry_selector);
    }
    const uint16_t expected_range_shift =
        kFormat0PairSize * num_pairs - subtable.search_range;
    if (subtable.range_shift != expected_range_shift) {
      Warning("bad range shift");
      subtable.range_shift = expected_range_shift;
    }

    // Read kerning pairs.
    subtable.pairs.reserve(num_pairs);
    uint32_t last_pair = 0;
    for (unsigned j = 0; j < num_pairs; ++j) {
      OpenTypeKERNFormat0Pair kerning_pair;
      if (!table.ReadU16(&kerning_pair.left) ||
          !table.ReadU16(&kerning_pair.right) ||
          !table.ReadS16(&kerning_pair.value)) {
        return Error("Failed to read subtable %d kerning pair %d", i, j);
      }
      const uint32_t current_pair
          = (kerning_pair.left << 16) + kerning_pair.right;
      if (j != 0 && current_pair <= last_pair) {
        // Many free fonts don't follow this rule, so we don't call OTS_FAILURE
        // in order to support these fonts.
        return Drop("Kerning pairs are not sorted");
      }
      last_pair = current_pair;
      subtable.pairs.push_back(kerning_pair);
    }

    this->subtables.push_back(subtable);
  }

  if (!this->subtables.size()) {
    return Drop("All subtables were removed");
  }

  return true;
}

bool OpenTypeKERN::Serialize(OTSStream *out) {
  const uint16_t num_subtables = static_cast<uint16_t>(this->subtables.size());
  if (num_subtables != this->subtables.size() ||
      !out->WriteU16(this->version) ||
      !out->WriteU16(num_subtables)) {
    return Error("Failed to write kern table header");
  }

  for (uint16_t i = 0; i < num_subtables; ++i) {
    const size_t length = 14 + (6 * this->subtables[i].pairs.size());
    if (length > std::numeric_limits<uint16_t>::max() ||
        !out->WriteU16(this->subtables[i].version) ||
        !out->WriteU16(static_cast<uint16_t>(length)) ||
        !out->WriteU16(this->subtables[i].coverage) ||
        !out->WriteU16(
            static_cast<uint16_t>(this->subtables[i].pairs.size())) ||
        !out->WriteU16(this->subtables[i].search_range) ||
        !out->WriteU16(this->subtables[i].entry_selector) ||
        !out->WriteU16(this->subtables[i].range_shift)) {
      return Error("Failed to write kern subtable %d", i);
    }
    for (unsigned j = 0; j < this->subtables[i].pairs.size(); ++j) {
      if (!out->WriteU16(this->subtables[i].pairs[j].left) ||
          !out->WriteU16(this->subtables[i].pairs[j].right) ||
          !out->WriteS16(this->subtables[i].pairs[j].value)) {
        return Error("Failed to write kern pair %d for subtable %d", j, i);
      }
    }
  }

  return true;
}

bool OpenTypeKERN::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

}  // namespace ots
