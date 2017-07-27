// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vmtx.h"

#include "maxp.h"
#include "vhea.h"

// vmtx - Vertical Metrics Table
// http://www.microsoft.com/typography/otspec/vmtx.htm

namespace ots {

bool OpenTypeVMTX::Parse(const uint8_t *data, size_t length) {
  return OpenTypeMetricsTable::Parse(data, length);
}

bool OpenTypeVMTX::Serialize(OTSStream *out) {
  return OpenTypeMetricsTable::Serialize(out);
}

bool OpenTypeVMTX::ShouldSerialize() {
  return OpenTypeMetricsTable::ShouldSerialize() &&
         // vmtx should serialise when vhea is preserved.
         GetFont()->GetTable(OTS_TAG_VHEA) != NULL;
}

}  // namespace ots
