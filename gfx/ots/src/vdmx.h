// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_VDMX_H_
#define OTS_VDMX_H_

#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeVDMXRatioRecord {
  uint8_t charset;
  uint8_t x_ratio;
  uint8_t y_start_ratio;
  uint8_t y_end_ratio;
};

struct OpenTypeVDMXVTable {
  uint16_t y_pel_height;
  int16_t y_max;
  int16_t y_min;
};

struct OpenTypeVDMXGroup {
  uint16_t recs;
  uint8_t startsz;
  uint8_t endsz;
  std::vector<OpenTypeVDMXVTable> entries;
};

class OpenTypeVDMX : public Table {
 public:
  explicit OpenTypeVDMX(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  uint16_t version;
  uint16_t num_recs;
  uint16_t num_ratios;
  std::vector<OpenTypeVDMXRatioRecord> rat_ranges;
  std::vector<uint16_t> offsets;
  std::vector<OpenTypeVDMXGroup> groups;
};

}  // namespace ots

#endif  // OTS_VDMX_H_
