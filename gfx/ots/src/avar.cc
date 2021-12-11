// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "avar.h"

#include "fvar.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeAVAR
// -----------------------------------------------------------------------------

bool OpenTypeAVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  if (!table.ReadU16(&this->majorVersion) ||
      !table.ReadU16(&this->minorVersion) ||
      !table.ReadU16(&this->reserved) ||
      !table.ReadU16(&this->axisCount)) {
    return Drop("Failed to read table header");
  }
  if (this->majorVersion != 1) {
    return Drop("Unknown table version");
  }
  if (this->minorVersion > 0) {
    // we only know how to serialize version 1.0
    Warning("Downgrading minor version to 0");
    this->minorVersion = 0;
  }
  if (this->reserved != 0) {
    Warning("Expected reserved=0");
    this->reserved = 0;
  }

  OpenTypeFVAR* fvar = static_cast<OpenTypeFVAR*>(
      GetFont()->GetTypedTable(OTS_TAG_FVAR));
  if (!fvar) {
    return DropVariations("Required fvar table is missing");
  }
  if (axisCount != fvar->AxisCount()) {
    return Drop("Axis count mismatch");
  }

  for (size_t i = 0; i < this->axisCount; i++) {
    this->axisSegmentMaps.emplace_back();
    uint16_t positionMapCount;
    if (!table.ReadU16(&positionMapCount)) {
      return Drop("Failed to read position map count");
    }
    int foundRequiredMappings = 0;
    for (size_t j = 0; j < positionMapCount; j++) {
      AxisValueMap map;
      if (!table.ReadS16(&map.fromCoordinate) ||
          !table.ReadS16(&map.toCoordinate)) {
        return Drop("Failed to read axis value map");
      }
      if (map.fromCoordinate < -0x4000 ||
          map.fromCoordinate > 0x4000 ||
          map.toCoordinate < -0x4000 ||
          map.toCoordinate > 0x4000) {
        return Drop("Axis value map coordinate out of range");
      }
      if (j > 0) {
        if (map.fromCoordinate <= this->axisSegmentMaps[i].back().fromCoordinate ||
            map.toCoordinate < this->axisSegmentMaps[i].back().toCoordinate) {
          return Drop("Axis value map out of order");
        }
      }
      if ((map.fromCoordinate == -0x4000 && map.toCoordinate == -0x4000) ||
          (map.fromCoordinate == 0 && map.toCoordinate == 0) ||
          (map.fromCoordinate == 0x4000 && map.toCoordinate == 0x4000)) {
        ++foundRequiredMappings;
      }
      this->axisSegmentMaps[i].push_back(map);
    }
    if (positionMapCount > 0 && foundRequiredMappings != 3) {
      return Drop("A required mapping (for -1, 0 or 1) is missing");
    }
  }

  return true;
}

bool OpenTypeAVAR::Serialize(OTSStream* out) {
  if (!out->WriteU16(this->majorVersion) ||
      !out->WriteU16(this->minorVersion) ||
      !out->WriteU16(this->reserved) ||
      !out->WriteU16(this->axisCount)) {
    return Error("Failed to write table");
  }

  for (size_t i = 0; i < this->axisCount; i++) {
    const auto& axisValueMap = this->axisSegmentMaps[i];
    if (!out->WriteU16(axisValueMap.size())) {
      return Error("Failed to write table");
    }
    for (size_t j = 0; j < axisValueMap.size(); j++) {
      if (!out->WriteS16(axisValueMap[j].fromCoordinate) ||
          !out->WriteS16(axisValueMap[j].toCoordinate)) {
        return Error("Failed to write table");
      }
    }
  }

  return true;
}

}  // namespace ots
