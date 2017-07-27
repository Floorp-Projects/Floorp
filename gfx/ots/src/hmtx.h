// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_HMTX_H_
#define OTS_HMTX_H_

#include "metrics.h"
#include "hhea.h"
#include "ots.h"

namespace ots {

class OpenTypeHMTX : public OpenTypeMetricsTable {
 public:
  explicit OpenTypeHMTX(Font *font, uint32_t tag)
      : OpenTypeMetricsTable(font, tag, tag, OTS_TAG_HHEA) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
};

}  // namespace ots

#endif  // OTS_HMTX_H_
