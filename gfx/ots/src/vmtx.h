// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OTS_VMTX_H_
#define OTS_VMTX_H_

#include "metrics.h"
#include "vhea.h"
#include "ots.h"

namespace ots {

struct OpenTypeVMTX : public OpenTypeMetricsTable {
 public:
  explicit OpenTypeVMTX(Font *font, uint32_t tag)
      : OpenTypeMetricsTable(font, tag, tag, OTS_TAG_VHEA) { }

  bool Parse(const uint8_t *data, size_t length);
  bool Serialize(OTSStream *out);
  bool ShouldSerialize();
};

}  // namespace ots

#endif  // OTS_VMTX_H_

