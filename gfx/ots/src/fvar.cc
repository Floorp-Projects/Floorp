// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fvar.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeFVAR
// -----------------------------------------------------------------------------

bool OpenTypeFVAR::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  if (!table.ReadU16(&this->majorVersion) ||
      !table.ReadU16(&this->minorVersion) ||
      !table.ReadU16(&this->axesArrayOffset) ||
      !table.ReadU16(&this->reserved) ||
      !table.ReadU16(&this->axisCount) ||
      !table.ReadU16(&this->axisSize) ||
      !table.ReadU16(&this->instanceCount) ||
      !table.ReadU16(&this->instanceSize)) {
    return DropVariations("Failed to read table header");
  }
  if (this->majorVersion != 1) {
    return DropVariations("Unknown table version");
  }
  if (this->minorVersion > 0) {
    Warning("Downgrading minor version to 0");
    this->minorVersion = 0;
  }
  if (this->axesArrayOffset > length || this->axesArrayOffset < table.offset()) {
    return DropVariations("Bad axesArrayOffset");
  }
  if (this->reserved != 2) {
    Warning("Expected reserved=2");
    this->reserved = 2;
  }
  if (this->axisCount == 0) {
    return DropVariations("No variation axes");
  }
  if (this->axisSize != 20) {
    return DropVariations("Invalid axisSize");
  }
  // instanceCount is not validated
  if (this->instanceSize == this->axisCount * sizeof(Fixed) + 6) {
    this->instancesHavePostScriptNameID = true;
  } else if (this->instanceSize == this->axisCount * sizeof(Fixed) + 4) {
    this->instancesHavePostScriptNameID = false;
  } else {
    return DropVariations("Invalid instanceSize");
  }

  // When we serialize, the axes array will go here, even if it was
  // originally at a different offset. So we update the axesArrayOffset
  // field for the header.
  uint32_t origAxesArrayOffset = this->axesArrayOffset;
  this->axesArrayOffset = table.offset();

  table.set_offset(origAxesArrayOffset);
  for (unsigned i = 0; i < this->axisCount; i++) {
    this->axes.emplace_back();
    auto& axis = this->axes[i];
    if (!table.ReadU32(&axis.axisTag) ||
        !table.ReadS32(&axis.minValue) ||
        !table.ReadS32(&axis.defaultValue) ||
        !table.ReadS32(&axis.maxValue) ||
        !table.ReadU16(&axis.flags) ||
        !table.ReadU16(&axis.axisNameID)) {
      return DropVariations("Failed to read axis record");
    }
    if (!CheckTag(axis.axisTag)) {
      return DropVariations("Bad axis tag");
    }
    if (!(axis.minValue <= axis.defaultValue && axis.defaultValue <= axis.maxValue)) {
      return DropVariations("Bad axis value range");
    }
    if ((axis.flags & 0xFFFEu) != 0) {
      Warning("Discarding unknown axis flags");
      axis.flags &= ~0xFFFEu;
    }
    if (axis.axisNameID <= 255 || axis.axisNameID >= 32768) {
      Warning("Axis nameID out of range");
      // We don't check that the name actually exists -- assume the client can handle
      // a missing name when it tries to read the table.
    }
  }

  for (unsigned i = 0; i < this->instanceCount; i++) {
    this->instances.emplace_back();
    auto& inst = this->instances[i];
    if (!table.ReadU16(&inst.subfamilyNameID) ||
        !table.ReadU16(&inst.flags)) {
      return DropVariations("Failed to read instance record");
    }
    inst.coordinates.reserve(this->axisCount);
    for (unsigned j = 0; j < this->axisCount; j++) {
      inst.coordinates.emplace_back();
      auto& coord = inst.coordinates[j];
      if (!table.ReadS32(&coord)) {
        return DropVariations("Failed to read instance coordinates");
      }
    }
    if (this->instancesHavePostScriptNameID) {
      if (!table.ReadU16(&inst.postScriptNameID)) {
        return DropVariations("Failed to read instance psname ID");
      }
    }
  }

  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }

  return true;
}

bool OpenTypeFVAR::Serialize(OTSStream* out) {
  if (!out->WriteU16(this->majorVersion) ||
      !out->WriteU16(this->minorVersion) ||
      !out->WriteU16(this->axesArrayOffset) ||
      !out->WriteU16(this->reserved) ||
      !out->WriteU16(this->axisCount) ||
      !out->WriteU16(this->axisSize) ||
      !out->WriteU16(this->instanceCount) ||
      !out->WriteU16(this->instanceSize)) {
    return Error("Failed to write table");
  }

  for (unsigned i = 0; i < this->axisCount; i++) {
    const auto& axis = this->axes[i];
    if (!out->WriteU32(axis.axisTag) ||
        !out->WriteS32(axis.minValue) ||
        !out->WriteS32(axis.defaultValue) ||
        !out->WriteS32(axis.maxValue) ||
        !out->WriteU16(axis.flags) ||
        !out->WriteU16(axis.axisNameID)) {
      return Error("Failed to write table");
    }
  }

  for (unsigned i = 0; i < this->instanceCount; i++) {
    const auto& inst = this->instances[i];
    if (!out->WriteU16(inst.subfamilyNameID) ||
        !out->WriteU16(inst.flags)) {
      return Error("Failed to write table");
    }
    for (unsigned j = 0; j < this->axisCount; j++) {
      const auto& coord = inst.coordinates[j];
      if (!out->WriteS32(coord)) {
        return Error("Failed to write table");
      }
    }
    if (this->instancesHavePostScriptNameID) {
      if (!out->WriteU16(inst.postScriptNameID)) {
        return Error("Failed to write table");
      }
    }
  }

  return true;
}

}  // namespace ots
