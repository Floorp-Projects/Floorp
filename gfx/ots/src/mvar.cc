// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mvar.h"

#include "variations.h"

#define TABLE_NAME "MVAR"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeMVAR
// -----------------------------------------------------------------------------

bool OpenTypeMVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);

  uint16_t majorVersion;
  uint16_t minorVersion;
  uint16_t reserved;
  uint16_t valueRecordSize;
  uint16_t valueRecordCount;
  uint16_t itemVariationStoreOffset;

  if (!table.ReadU16(&majorVersion) ||
      !table.ReadU16(&minorVersion) ||
      !table.ReadU16(&reserved) ||
      !table.ReadU16(&valueRecordSize) ||
      !table.ReadU16(&valueRecordCount) ||
      !table.ReadU16(&itemVariationStoreOffset)) {
    return DropVariations("Failed to read table header");
  }

  if (majorVersion != 1) {
    return DropVariations("Unknown table version");
  }

  if (reserved != 0) {
    Warning("Expected reserved=0");
  }

  // The spec says that valueRecordSize "must be greater than zero",
  // but we don't enforce this in the case where valueRecordCount
  // is zero.
  // The minimum size for a valueRecord to be valid is 8, for the
  // three fields currently defined in the record (see below).
  if (valueRecordSize < 8) {
    if (valueRecordCount != 0) {
      return DropVariations("Value record size too small");
    }
  }

  if (valueRecordCount == 0) {
    if (itemVariationStoreOffset != 0) {
      // The spec says "if valueRecordCount is zero, set to zero",
      // but having a variation store even when record count is zero
      // should be harmless -- it just won't be useful for anything.
      // But we don't need to reject altogether.
      Warning("Unexpected item variation store");
    }
  } else {
    if (itemVariationStoreOffset < table.offset() || itemVariationStoreOffset > length) {
      return DropVariations("Invalid item variation store offset");
    }
    if (!ParseItemVariationStore(GetFont(), data + itemVariationStoreOffset,
                                 length - itemVariationStoreOffset)) {
      return DropVariations("Failed to parse item variation store");
    }
  }

  uint32_t prevTag = 0;
  size_t offset = table.offset();
  for (unsigned i = 0; i < valueRecordCount; i++) {
    uint32_t tag;
    uint16_t deltaSetOuterIndex, deltaSetInnerIndex;
    if (!table.ReadU32(&tag) ||
        !table.ReadU16(&deltaSetOuterIndex) ||
        !table.ReadU16(&deltaSetInnerIndex)) {
      return DropVariations("Failed to read value record");
    }
    if (tag <= prevTag) {
      return DropVariations("Invalid or out-of-order value tag");
    }
    prevTag = tag;
    // Adjust offset in case additional fields have been added to the
    // valueRecord by a new minor version (allowed by spec).
    offset += valueRecordSize;
    table.set_offset(offset);
  }

  this->m_data = data;
  this->m_length = length;

  return true;
}

bool OpenTypeMVAR::Serialize(OTSStream* out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return Error("Failed to write MVAR table");
  }

  return true;
}

}  // namespace ots

#undef TABLE_NAME
