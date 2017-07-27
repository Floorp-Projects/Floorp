// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hmtx.h"

#include "hhea.h"
#include "maxp.h"

// hmtx - Horizontal Metrics
// http://www.microsoft.com/typography/otspec/hmtx.htm

namespace ots {

bool OpenTypeHMTX::Parse(const uint8_t *data, size_t length) {
  return OpenTypeMetricsTable::Parse(data, length);
}

bool OpenTypeHMTX::Serialize(OTSStream *out) {
  return OpenTypeMetricsTable::Serialize(out);
}

}  // namespace ots
