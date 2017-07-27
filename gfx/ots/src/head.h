// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HEAD_H_
#define OTS_HEAD_H_

#include "ots.h"

namespace ots {

class OpenTypeHEAD : public Table {
 public:
  explicit OpenTypeHEAD(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

  uint32_t revision;
  uint16_t flags;
  uint16_t upem;
  uint64_t created;
  uint64_t modified;

  int16_t xmin, xmax;
  int16_t ymin, ymax;

  uint16_t mac_style;
  uint16_t min_ppem;
  int16_t index_to_loc_format;
};

}  // namespace ots

#endif  // OTS_HEAD_H_
