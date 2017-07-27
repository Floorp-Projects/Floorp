// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_VHEA_H_
#define OTS_VHEA_H_

#include "metrics.h"
#include "ots.h"

namespace ots {

class OpenTypeVHEA : public OpenTypeMetricsHeader {
 public:
  explicit OpenTypeVHEA(Font *font, uint32_t tag)
      : OpenTypeMetricsHeader(font, tag, tag) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();
};

}  // namespace ots

#endif  // OTS_VHEA_H_

