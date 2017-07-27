// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_VORG_H_
#define OTS_VORG_H_

#include <vector>

#include "ots.h"

namespace ots {

struct OpenTypeVORGMetrics {
  uint16_t glyph_index;
  int16_t vert_origin_y;
};

class OpenTypeVORG : public Table {
 public:
  explicit OpenTypeVORG(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();

 private:
  uint16_t major_version;
  uint16_t minor_version;
  int16_t default_vert_origin_y;
  std::vector<OpenTypeVORGMetrics> metrics;
};

}  // namespace ots

#endif  // OTS_VORG_H_
