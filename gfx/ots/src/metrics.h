// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_METRICS_H_
#define OTS_METRICS_H_

#include <new>
#include <utility>
#include <vector>

#include "ots.h"

namespace ots {

class OpenTypeMetricsHeader : public Table {
 public:
  explicit OpenTypeMetricsHeader(Font *font, uint32_t tag, uint32_t type)
      : Table(font, tag, type) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  uint32_t version;
  int16_t ascent;
  int16_t descent;
  int16_t linegap;
  uint16_t adv_width_max;
  int16_t min_sb1;
  int16_t min_sb2;
  int16_t max_extent;
  int16_t caret_slope_rise;
  int16_t caret_slope_run;
  int16_t caret_offset;
  uint16_t num_metrics;
};

struct OpenTypeMetricsTable : public Table {
 public:
  explicit OpenTypeMetricsTable(Font *font, uint32_t tag, uint32_t type,
                                uint32_t header_tag)
      : Table(font, tag, type), m_header_tag(header_tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  uint32_t m_header_tag;

  std::vector<std::pair<uint16_t, int16_t> > entries;
  std::vector<int16_t> sbs;
};

}  // namespace ots

#endif  // OTS_METRICS_H_
