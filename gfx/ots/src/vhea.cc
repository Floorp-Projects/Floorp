// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vhea.h"

#include "head.h"
#include "maxp.h"

// vhea - Vertical Header Table
// http://www.microsoft.com/typography/otspec/vhea.htm

namespace ots {

bool OpenTypeVHEA::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return Error("Failed to read version");
  }
  if (this->version != 0x00010000 &&
      this->version != 0x00011000) {
    return Error("Unsupported table version: 0x%x", this->version);
  }

  return OpenTypeMetricsHeader::Parse(data, length);
}

bool OpenTypeVHEA::Serialize(OTSStream *out) {
  return OpenTypeMetricsHeader::Serialize(out);
}

bool OpenTypeVHEA::ShouldSerialize() {
  return OpenTypeMetricsHeader::ShouldSerialize() &&
         // vhea shouldn't serialise when vmtx doesn't exist.
         GetFont()->GetTable(OTS_TAG_VMTX) != NULL;
}

}  // namespace ots
