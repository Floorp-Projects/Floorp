// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_MAXP_H_
#define OTS_MAXP_H_

#include "ots.h"

namespace ots {

class OpenTypeMAXP : public Table {
 public:
  explicit OpenTypeMAXP(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  uint16_t num_glyphs;
  bool version_1;

  uint16_t max_points;
  uint16_t max_contours;
  uint16_t max_c_points;
  uint16_t max_c_contours;

  uint16_t max_zones;
  uint16_t max_t_points;
  uint16_t max_storage;
  uint16_t max_fdefs;
  uint16_t max_idefs;
  uint16_t max_stack;
  uint16_t max_size_glyf_instructions;

  uint16_t max_c_components;
  uint16_t max_c_depth;
};

}  // namespace ots

#endif  // OTS_MAXP_H_
