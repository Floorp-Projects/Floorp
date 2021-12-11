// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cvar.h"

#include "fvar.h"
#include "variations.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeCVAR
// -----------------------------------------------------------------------------

bool OpenTypeCVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);

  uint16_t majorVersion;
  uint16_t minorVersion;

  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion)) {
    return Drop("Failed to read table header");
  }

  if (majorVersion != 1) {
    return Drop("Unknown table version");
  }

  OpenTypeFVAR* fvar = static_cast<OpenTypeFVAR*>(
      GetFont()->GetTypedTable(OTS_TAG_FVAR));
  if (!fvar) {
    return DropVariations("Required fvar table is missing");
  }

  if (!ParseVariationData(GetFont(), data + table.offset(), length - table.offset(),
                          fvar->AxisCount(), 0)) {
    return Drop("Failed to parse variation data");
  }

  this->m_data = data;
  this->m_length = length;

  return true;
}

bool OpenTypeCVAR::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write cvar table");
  }

  return true;
}

}  // namespace ots
