// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hhea.h"

#include "head.h"
#include "maxp.h"

// hhea - Horizontal Header
// http://www.microsoft.com/typography/otspec/hhea.htm

namespace ots {

bool OpenTypeHHEA::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return Error("Failed to read table version");
  }
  if (this->version >> 16 != 1) {
    return Error("Unsupported majorVersion: %d", this->version >> 16);
  }

  return OpenTypeMetricsHeader::Parse(data, length);
}

bool OpenTypeHHEA::Serialize(OTSStream *out) {
  return OpenTypeMetricsHeader::Serialize(out);
}

}  // namespace ots
