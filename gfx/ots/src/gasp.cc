// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gasp.h"

// gasp - Grid-fitting And Scan-conversion Procedure
// http://www.microsoft.com/typography/otspec/gasp.htm

namespace ots {

bool OpenTypeGASP::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint16_t num_ranges = 0;
  if (!table.ReadU16(&this->version) ||
      !table.ReadU16(&num_ranges)) {
    return Error("Failed to read table header");
  }

  if (this->version > 1) {
    // Lots of Linux fonts have bad version numbers...
    return Drop("Unsupported version: %u", this->version);
  }

  if (num_ranges == 0) {
    return Drop("numRanges is zero");
  }

  this->gasp_ranges.reserve(num_ranges);
  for (unsigned i = 0; i < num_ranges; ++i) {
    uint16_t max_ppem = 0;
    uint16_t behavior = 0;
    if (!table.ReadU16(&max_ppem) ||
        !table.ReadU16(&behavior)) {
      return Error("Failed to read GASPRANGE %d", i);
    }
    if ((i > 0) && (this->gasp_ranges[i - 1].first >= max_ppem)) {
      // The records in the gaspRange[] array must be sorted in order of
      // increasing rangeMaxPPEM value.
      return Drop("Ranges are not sorted");
    }
    if ((i == num_ranges - 1u) &&  // never underflow.
        (max_ppem != 0xffffu)) {
      return Drop("The last record should be 0xFFFF as a sentinel value "
                  "for rangeMaxPPEM");
    }

    if (behavior >> 8) {
      Warning("Undefined bits are used: %x", behavior);
      // mask undefined bits.
      behavior &= 0x000fu;
    }

    if (this->version == 0 && (behavior >> 2) != 0) {
      Warning("Changed the version number to 1");
      this->version = 1;
    }

    this->gasp_ranges.push_back(std::make_pair(max_ppem, behavior));
  }

  return true;
}

bool OpenTypeGASP::Serialize(OTSStream *out) {
  const uint16_t num_ranges = static_cast<uint16_t>(this->gasp_ranges.size());
  if (num_ranges != this->gasp_ranges.size() ||
      !out->WriteU16(this->version) ||
      !out->WriteU16(num_ranges)) {
    return Error("Failed to write table header");
  }

  for (uint16_t i = 0; i < num_ranges; ++i) {
    if (!out->WriteU16(this->gasp_ranges[i].first) ||
        !out->WriteU16(this->gasp_ranges[i].second)) {
      return Error("Failed to write GASPRANGE %d", i);
    }
  }

  return true;
}

}  // namespace ots
