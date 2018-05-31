// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hvar.h"

#include "variations.h"

#define TABLE_NAME "HVAR"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeHVAR
// -----------------------------------------------------------------------------

bool OpenTypeHVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);

  uint16_t majorVersion;
  uint16_t minorVersion;
  uint32_t itemVariationStoreOffset;
  uint32_t advanceWidthMappingOffset;
  uint32_t lsbMappingOffset;
  uint32_t rsbMappingOffset;

  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion) ||
      !table.ReadU32(&itemVariationStoreOffset) ||
      !table.ReadU32(&advanceWidthMappingOffset) ||
      !table.ReadU32(&lsbMappingOffset) ||
      !table.ReadU32(&rsbMappingOffset)) {
    return DropVariations("Failed to read table header");
  }

  if (majorVersion != 1) {
    return DropVariations("Unknown table version");
  }

  if (itemVariationStoreOffset > length ||
      advanceWidthMappingOffset > length ||
      lsbMappingOffset > length ||
      rsbMappingOffset > length) {
    return DropVariations("Invalid subtable offset");
  }

  if (!ParseItemVariationStore(GetFont(), data + itemVariationStoreOffset,
                               length - itemVariationStoreOffset)) {
    return DropVariations("Failed to parse item variation store");
  }

  if (advanceWidthMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + advanceWidthMappingOffset,
                               length - advanceWidthMappingOffset)) {
      return DropVariations("Failed to parse advance width mappings");
    }
  }

  if (lsbMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + lsbMappingOffset,
                               length - lsbMappingOffset)) {
      return DropVariations("Failed to parse LSB mappings");
    }
  }

  if (rsbMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + rsbMappingOffset,
                               length - rsbMappingOffset)) {
      return DropVariations("Failed to parse RSB mappings");
    }
  }

  this->m_data = data;
  this->m_length = length;

  return true;
}

bool OpenTypeHVAR::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write HVAR table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
