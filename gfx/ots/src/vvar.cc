// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vvar.h"

#include "variations.h"

#define TABLE_NAME "VVAR"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeVVAR
// -----------------------------------------------------------------------------

bool OpenTypeVVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);

  uint16_t majorVersion;
  uint16_t minorVersion;
  uint32_t itemVariationStoreOffset;
  uint32_t advanceHeightMappingOffset;
  uint32_t tsbMappingOffset;
  uint32_t bsbMappingOffset;
  uint32_t vOrgMappingOffset;

  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion) ||
      !table.ReadU32(&itemVariationStoreOffset) ||
      !table.ReadU32(&advanceHeightMappingOffset) ||
      !table.ReadU32(&tsbMappingOffset) ||
      !table.ReadU32(&bsbMappingOffset) ||
      !table.ReadU32(&vOrgMappingOffset)) {
    return DropVariations("Failed to read table header");
  }

  if (majorVersion != 1) {
    return DropVariations("Unknown table version");
  }

  if (itemVariationStoreOffset > length ||
      advanceHeightMappingOffset > length ||
      tsbMappingOffset > length ||
      bsbMappingOffset > length ||
      vOrgMappingOffset > length) {
    return DropVariations("Invalid subtable offset");
  }

  if (!ParseItemVariationStore(GetFont(), data + itemVariationStoreOffset,
                               length - itemVariationStoreOffset)) {
    return DropVariations("Failed to parse item variation store");
  }

  if (advanceHeightMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + advanceHeightMappingOffset,
                               length - advanceHeightMappingOffset)) {
      return DropVariations("Failed to parse advance height mappings");
    }
  }

  if (tsbMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + tsbMappingOffset,
                               length - tsbMappingOffset)) {
      return DropVariations("Failed to parse TSB mappings");
    }
  }

  if (bsbMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + bsbMappingOffset,
                               length - bsbMappingOffset)) {
      return DropVariations("Failed to parse BSB mappings");
    }
  }

  if (vOrgMappingOffset) {
    if (!ParseDeltaSetIndexMap(GetFont(), data + vOrgMappingOffset,
                               length - vOrgMappingOffset)) {
      return DropVariations("Failed to parse vOrg mappings");
    }
  }

  this->m_data = data;
  this->m_length = length;

  return true;
}

bool OpenTypeVVAR::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write VVAR table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
