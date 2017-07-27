// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_GASP_H_
#define OTS_GASP_H_

#include <new>
#include <utility>
#include <vector>

#include "ots.h"

namespace ots {

class OpenTypeGASP : public Table {
 public:
  explicit OpenTypeGASP(Font *font, uint32_t tag)
      : Table(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);

 private:
  uint16_t version;
  // A array of (max PPEM, GASP behavior) pairs.
  std::vector<std::pair<uint16_t, uint16_t> > gasp_ranges;
};

}  // namespace ots

#endif  // OTS_GASP_H_
