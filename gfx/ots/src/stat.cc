// Copyright (c) 2018 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stat.h"
#include "name.h"

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeSTAT
// -----------------------------------------------------------------------------

bool OpenTypeSTAT::ValidateNameId(uint16_t nameid) {
  OpenTypeNAME* name = static_cast<OpenTypeNAME*>(
      GetFont()->GetTypedTable(OTS_TAG_NAME));

  if (!name || !name->IsValidNameId(nameid)) {
    Drop("Invalid nameID: %d", nameid);
    return false;
  }

  if ((nameid >= 26 && nameid <= 255) || nameid >= 32768) {
    Warning("nameID out of range: %d", nameid);
    return true;
  }

  return  true;
}

bool OpenTypeSTAT::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  if (!table.ReadU16(&this->majorVersion) ||
      !table.ReadU16(&this->minorVersion) ||
      !table.ReadU16(&this->designAxisSize) ||
      !table.ReadU16(&this->designAxisCount) ||
      !table.ReadU32(&this->designAxesOffset) ||
      !table.ReadU16(&this->axisValueCount) ||
      !table.ReadU32(&this->offsetToAxisValueOffsets) ||
      !(this->minorVersion < 1 || table.ReadU16(&this->elidedFallbackNameID))) {
    return Drop("Failed to read table header");
  }
  if (this->majorVersion != 1) {
    return Drop("Unknown table version");
  }
  if (this->minorVersion > 2) {
    Warning("Unknown minor version, downgrading to 2");
    this->minorVersion = 2;
  }

  if (this->designAxisSize < sizeof(AxisRecord)) {
    return Drop("Invalid designAxisSize");
  }

  size_t headerEnd = table.offset();

  if (this->designAxisCount == 0) {
    if (this->designAxesOffset != 0) {
      Warning("Unexpected non-zero designAxesOffset");
      this->designAxesOffset = 0;
    }
  } else {
    if (this->designAxesOffset < headerEnd ||
        size_t(this->designAxesOffset) +
          size_t(this->designAxisCount) * size_t(this->designAxisSize) > length) {
      return Drop("Invalid designAxesOffset");
    }
  }

  for (size_t i = 0; i < this->designAxisCount; i++) {
    table.set_offset(this->designAxesOffset + i * this->designAxisSize);
    this->designAxes.emplace_back();
    auto& axis = this->designAxes[i];
    if (!table.ReadU32(&axis.axisTag) ||
        !table.ReadU16(&axis.axisNameID) ||
        !table.ReadU16(&axis.axisOrdering)) {
      return Drop("Failed to read design axis");
    }
    if (!CheckTag(axis.axisTag)) {
      return Drop("Bad design axis tag");
    }
    if (!ValidateNameId(axis.axisNameID)) {
      return true;
    }
  }

  // TODO
  // - check that all axes defined in fvar are covered by STAT
  // - check that axisOrdering values are not duplicated (warn only)

  if (this->axisValueCount == 0) {
    if (this->offsetToAxisValueOffsets != 0) {
      Warning("Unexpected non-zero offsetToAxisValueOffsets");
      this->offsetToAxisValueOffsets = 0;
    }
  } else {
    if (this->offsetToAxisValueOffsets < headerEnd ||
        size_t(this->offsetToAxisValueOffsets) +
          size_t(this->axisValueCount) * sizeof(uint16_t) > length) {
      return Drop("Invalid offsetToAxisValueOffsets");
    }
  }

  for (size_t i = 0; i < this->axisValueCount; i++) {
    table.set_offset(this->offsetToAxisValueOffsets + i * sizeof(uint16_t));
    uint16_t axisValueOffset;
    if (!table.ReadU16(&axisValueOffset)) {
      return Drop("Failed to read axis value offset");
    }
    if (this->offsetToAxisValueOffsets + axisValueOffset > length) {
      return Drop("Invalid axis value offset");
    }
    table.set_offset(this->offsetToAxisValueOffsets + axisValueOffset);
    uint16_t format;
    if (!table.ReadU16(&format)) {
      return Drop("Failed to read axis value format");
    }
    this->axisValues.emplace_back(format);
    auto& axisValue = axisValues[i];
    switch (format) {
    case 1:
      if (!table.ReadU16(&axisValue.format1.axisIndex) ||
          !table.ReadU16(&axisValue.format1.flags) ||
          !table.ReadU16(&axisValue.format1.valueNameID) ||
          !table.ReadS32(&axisValue.format1.value)) {
        return Drop("Failed to read axis value (format 1)");
      }
      if (axisValue.format1.axisIndex >= this->designAxisCount) {
        return Drop("Axis index out of range");
      }
      if ((axisValue.format1.flags & 0xFFFCu) != 0) {
        Warning("Unexpected axis value flags");
        axisValue.format1.flags &= ~0xFFFCu;
      }
      if (!ValidateNameId(axisValue.format1.valueNameID)) {
        return true;
      }
      break;
    case 2:
      if (!table.ReadU16(&axisValue.format2.axisIndex) ||
          !table.ReadU16(&axisValue.format2.flags) ||
          !table.ReadU16(&axisValue.format2.valueNameID) ||
          !table.ReadS32(&axisValue.format2.nominalValue) ||
          !table.ReadS32(&axisValue.format2.rangeMinValue) ||
          !table.ReadS32(&axisValue.format2.rangeMaxValue)) {
        return Drop("Failed to read axis value (format 2)");
      }
      if (axisValue.format2.axisIndex >= this->designAxisCount) {
        return Drop("Axis index out of range");
      }
      if ((axisValue.format2.flags & 0xFFFCu) != 0) {
        Warning("Unexpected axis value flags");
        axisValue.format1.flags &= ~0xFFFCu;
      }
      if (!ValidateNameId(axisValue.format2.valueNameID)) {
        return true;
      }
      if (!(axisValue.format2.rangeMinValue <= axisValue.format2.nominalValue &&
            axisValue.format2.nominalValue <= axisValue.format2.rangeMaxValue)) {
        Warning("Bad axis value range or nominal value");
      }
      break;
    case 3:
      if (!table.ReadU16(&axisValue.format3.axisIndex) ||
          !table.ReadU16(&axisValue.format3.flags) ||
          !table.ReadU16(&axisValue.format3.valueNameID) ||
          !table.ReadS32(&axisValue.format3.value) ||
          !table.ReadS32(&axisValue.format3.linkedValue)) {
        return Drop("Failed to read axis value (format 3)");
      }
      if (axisValue.format3.axisIndex >= this->designAxisCount) {
        return Drop("Axis index out of range");
      }
      if ((axisValue.format3.flags & 0xFFFCu) != 0) {
        Warning("Unexpected axis value flags");
        axisValue.format3.flags &= ~0xFFFCu;
      }
      if (!ValidateNameId(axisValue.format3.valueNameID)) {
        return true;
      }
      break;
    case 4:
      if (this->minorVersion < 2) {
        return Drop("Invalid table minorVersion for format 4 axis values: %d", this->minorVersion);
      }
      if (!table.ReadU16(&axisValue.format4.axisCount) ||
          !table.ReadU16(&axisValue.format4.flags) ||
          !table.ReadU16(&axisValue.format4.valueNameID)) {
        return Drop("Failed to read axis value (format 4)");
      }
      if (axisValue.format4.axisCount > this->designAxisCount) {
        return Drop("Axis count out of range");
      }
      if ((axisValue.format4.flags & 0xFFFCu) != 0) {
        Warning("Unexpected axis value flags");
        axisValue.format4.flags &= ~0xFFFCu;
      }
      if (!ValidateNameId(axisValue.format4.valueNameID)) {
        return true;
      }
      for (unsigned j = 0; j < axisValue.format4.axisCount; j++) {
        axisValue.format4.axisValues.emplace_back();
        auto& v = axisValue.format4.axisValues[j];
        if (!table.ReadU16(&v.axisIndex) ||
            !table.ReadS32(&v.value)) {
          return Drop("Failed to read axis value");
        }
        if (v.axisIndex >= this->designAxisCount) {
          return Drop("Axis index out of range");
        }
      }
      break;
    default:
      return Drop("Unknown axis value format");
    }
  }

  return true;
}

bool OpenTypeSTAT::Serialize(OTSStream* out) {
  off_t tableStart = out->Tell();

  size_t headerSize = 5 * sizeof(uint16_t) + 2 * sizeof(uint32_t);
  if (this->minorVersion >= 1) {
    headerSize += sizeof(uint16_t);
  }

  if (this->designAxisCount == 0) {
    this->designAxesOffset = 0;
  } else {
    this->designAxesOffset = headerSize;
  }

  this->designAxisSize = sizeof(AxisRecord);

  if (this->axisValueCount == 0) {
    this->offsetToAxisValueOffsets = 0;
  } else {
    if (this->designAxesOffset == 0) {
      this->offsetToAxisValueOffsets = headerSize;
    } else {
      this->offsetToAxisValueOffsets = this->designAxesOffset + this->designAxisCount * this->designAxisSize;
    }
  }

  if (!out->WriteU16(this->majorVersion) ||
      !out->WriteU16(this->minorVersion) ||
      !out->WriteU16(this->designAxisSize) ||
      !out->WriteU16(this->designAxisCount) ||
      !out->WriteU32(this->designAxesOffset) ||
      !out->WriteU16(this->axisValueCount) ||
      !out->WriteU32(this->offsetToAxisValueOffsets) ||
      !(this->minorVersion < 1 || out->WriteU16(this->elidedFallbackNameID))) {
    return Error("Failed to write table header");
  }

  if (this->designAxisCount > 0) {
    if (out->Tell() - tableStart != this->designAxesOffset) {
      return Error("Error computing designAxesOffset");
    }
  }

  for (unsigned i = 0; i < this->designAxisCount; i++) {
    const auto& axis = this->designAxes[i];
    if (!out->WriteU32(axis.axisTag) ||
        !out->WriteU16(axis.axisNameID) ||
        !out->WriteU16(axis.axisOrdering)) {
      return Error("Failed to write design axis");
    }
  }

  if (this->axisValueCount > 0) {
    if (out->Tell() - tableStart != this->offsetToAxisValueOffsets) {
      return Error("Error computing offsetToAxisValueOffsets");
    }
  }

  uint32_t axisValueOffset = this->axisValueCount * sizeof(uint16_t);
  for (unsigned i = 0; i < this->axisValueCount; i++) {
    const auto& value = this->axisValues[i];
    if (!out->WriteU16(axisValueOffset)) {
      return Error("Failed to write axis value offset");
    }
    axisValueOffset += value.Length();
  }
  for (unsigned i = 0; i < this->axisValueCount; i++) {
    const auto& value = this->axisValues[i];
    if (!out->WriteU16(value.format)) {
      return Error("Failed to write axis value");
    }
    switch (value.format) {
    case 1:
      if (!out->WriteU16(value.format1.axisIndex) ||
          !out->WriteU16(value.format1.flags) ||
          !out->WriteU16(value.format1.valueNameID) ||
          !out->WriteS32(value.format1.value)) {
        return Error("Failed to write axis value");
      }
      break;
    case 2:
      if (!out->WriteU16(value.format2.axisIndex) ||
          !out->WriteU16(value.format2.flags) ||
          !out->WriteU16(value.format2.valueNameID) ||
          !out->WriteS32(value.format2.nominalValue) ||
          !out->WriteS32(value.format2.rangeMinValue) ||
          !out->WriteS32(value.format2.rangeMaxValue)) {
        return Error("Failed to write axis value");
      }
      break;
    case 3:
      if (!out->WriteU16(value.format3.axisIndex) ||
          !out->WriteU16(value.format3.flags) ||
          !out->WriteU16(value.format3.valueNameID) ||
          !out->WriteS32(value.format3.value) ||
          !out->WriteS32(value.format3.linkedValue)) {
        return Error("Failed to write axis value");
      }
      break;
    case 4:
      if (!out->WriteU16(value.format4.axisCount) ||
          !out->WriteU16(value.format4.flags) ||
          !out->WriteU16(value.format4.valueNameID)) {
        return Error("Failed to write axis value");
      }
      for (unsigned j = 0; j < value.format4.axisValues.size(); j++) {
        if (!out->WriteU16(value.format4.axisValues[j].axisIndex) ||
            !out->WriteS32(value.format4.axisValues[j].value)) {
          return Error("Failed to write axis value");
        }
      }
      break;
    default:
      return Error("Bad value format");
    }
  }

  return true;
}

}  // namespace ots
