// Copyright (c) 2022 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cpal.h"
#include "name.h"

// CPAL - Color Palette Table
// http://www.microsoft.com/typography/otspec/cpal.htm

#define TABLE_NAME "CPAL"

namespace {

// Caller has sized the colorRecords array, so we know how much to try and read.
bool ParseColorRecordsArray(const ots::Font* font,
                            const uint8_t* data, size_t length,
                            std::vector<uint32_t>* colorRecords)
{
  ots::Buffer subtable(data, length);

  for (auto& color : *colorRecords) {
    if (!subtable.ReadU32(&color)) {
      return OTS_FAILURE_MSG("Failed to read color record");
    }
  }

  return true;
}

// Caller has sized the paletteTypes array, so we know how much to try and read.
bool ParsePaletteTypesArray(const ots::Font* font,
                            const uint8_t* data, size_t length,
                            std::vector<uint32_t>* paletteTypes)
{
  ots::Buffer subtable(data, length);

  constexpr uint32_t USABLE_WITH_LIGHT_BACKGROUND = 0x0001;
  constexpr uint32_t USABLE_WITH_DARK_BACKGROUND = 0x0002;
  constexpr uint32_t RESERVED = ~(USABLE_WITH_LIGHT_BACKGROUND | USABLE_WITH_DARK_BACKGROUND);

  for (auto& type : *paletteTypes) {
    if (!subtable.ReadU32(&type)) {
      return OTS_FAILURE_MSG("Failed to read palette type");
    }
    if (type & RESERVED) {
      // Should we treat this as failure? For now, just a warning; seems unlikely
      // to be dangerous.
      OTS_WARNING("Invalid (reserved) palette type flags %08x", type);
      type &= ~RESERVED;
    }
  }

  return true;
}

// Caller has sized the labels array, so we know how much to try and read.
bool ParseLabelsArray(const ots::Font* font,
                      const uint8_t* data, size_t length,
                      std::vector<uint16_t>* labels,
                      const char* labelType)
{
  ots::Buffer subtable(data, length);

  auto* name = static_cast<ots::OpenTypeNAME*>(font->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return OTS_FAILURE_MSG("Required name table missing");
  }

  for (auto& nameID : *labels) {
    if (!subtable.ReadU16(&nameID)) {
      return OTS_FAILURE_MSG("Failed to read %s label ID", labelType);
    }
    if (nameID != 0xffff) {
      if (!name->IsValidNameId(nameID)) {
        OTS_WARNING("Label ID %u for %s missing from name table", nameID, labelType);
        nameID = 0xffff;
      }
    }
  }

  return true;
}

}  // namespace

namespace ots {

bool OpenTypeCPAL::Parse(const uint8_t *data, size_t length) {
  Font *font = GetFont();
  Buffer table(data, length);

  // Header fields common to versions 0 and 1. These are recomputed
  // from the array sizes during serialization.
  uint16_t numPalettes;
  uint16_t numColorRecords;
  uint32_t colorRecordsArrayOffset;

  if (!table.ReadU16(&this->version) ||
      !table.ReadU16(&this->num_palette_entries) ||
      !table.ReadU16(&numPalettes) ||
      !table.ReadU16(&numColorRecords) ||
      !table.ReadU32(&colorRecordsArrayOffset)) {
    return Error("Failed to read CPAL table header");
  }

  if (this->version > 1) {
    return Error("Unknown CPAL table version %u", this->version);
  }

  if (!this->num_palette_entries || !numPalettes || !numColorRecords) {
    return Error("Empty CPAL is not valid");
  }

  if (this->num_palette_entries > numColorRecords) {
    return Error("Not enough color records for a complete palette");
  }

  uint32_t headerSize = 4 * sizeof(uint16_t) + sizeof(uint32_t) +
      numPalettes * sizeof(uint16_t);

  // uint16_t colorRecordIndices[numPalettes]
  this->colorRecordIndices.resize(numPalettes);
  for (auto& colorRecordIndex : this->colorRecordIndices) {
    if (!table.ReadU16(&colorRecordIndex)) {
      return Error("Failed to read color record index");
    }
    if (colorRecordIndex > numColorRecords - this->num_palette_entries) {
      return Error("Palette overflows color records array");
    }
  }

  uint32_t paletteTypesArrayOffset = 0;
  uint32_t paletteLabelsArrayOffset = 0;
  uint32_t paletteEntryLabelsArrayOffset = 0;
  if (this->version == 1) {
    if (!table.ReadU32(&paletteTypesArrayOffset) ||
        !table.ReadU32(&paletteLabelsArrayOffset) ||
        !table.ReadU32(&paletteEntryLabelsArrayOffset)) {
      return Error("Failed to read CPAL v.1 table header");
    }
    headerSize += 3 * sizeof(uint32_t);
  }

  // The following arrays may occur in any order, as they're independently referenced
  // by offsets in the header.

  if (colorRecordsArrayOffset < headerSize || colorRecordsArrayOffset >= length) {
    return Error("Bad color records array offset in table header");
  }
  this->colorRecords.resize(numColorRecords);
  if (!ParseColorRecordsArray(font, data + colorRecordsArrayOffset, length - colorRecordsArrayOffset,
                              &this->colorRecords)) {
    return Error("Failed to parse color records array");
  }

  if (paletteTypesArrayOffset) {
    if (paletteTypesArrayOffset < headerSize || paletteTypesArrayOffset >= length) {
      return Error("Bad palette types array offset in table header");
    }
    this->paletteTypes.resize(numPalettes);
    if (!ParsePaletteTypesArray(font, data + paletteTypesArrayOffset, length - paletteTypesArrayOffset,
                                &this->paletteTypes)) {
      return Error("Failed to parse palette types array");
    }
  }

  if (paletteLabelsArrayOffset) {
    if (paletteLabelsArrayOffset < headerSize || paletteLabelsArrayOffset >= length) {
      return Error("Bad palette labels array offset in table header");
    }
    this->paletteLabels.resize(numPalettes);
    if (!ParseLabelsArray(font, data + paletteLabelsArrayOffset, length - paletteLabelsArrayOffset,
                          &this->paletteLabels, "palette")) {
      return Error("Failed to parse palette labels array");
    }
  }

  if (paletteEntryLabelsArrayOffset) {
    if (paletteEntryLabelsArrayOffset < headerSize || paletteEntryLabelsArrayOffset >= length) {
      return Error("Bad palette entry labels array offset in table header");
    }
    this->paletteEntryLabels.resize(this->num_palette_entries);
    if (!ParseLabelsArray(font, data + paletteEntryLabelsArrayOffset, length - paletteEntryLabelsArrayOffset,
                          &this->paletteEntryLabels, "palette entry")) {
      return Error("Failed to parse palette entry labels array");
    }
  }

  return true;
}

bool OpenTypeCPAL::Serialize(OTSStream *out) {
  uint16_t numPalettes = this->colorRecordIndices.size();
  uint16_t numColorRecords = this->colorRecords.size();

#ifndef NDEBUG
  off_t start = out->Tell();
#endif

  size_t colorRecordsArrayOffset = 4 * sizeof(uint16_t) + sizeof(uint32_t) +
      numPalettes * sizeof(uint16_t);
  if (this->version == 1) {
    colorRecordsArrayOffset += 3 * sizeof(uint32_t);
  }

  size_t totalLen = colorRecordsArrayOffset + numColorRecords * sizeof(uint32_t);

  if (!out->WriteU16(this->version) ||
      !out->WriteU16(this->num_palette_entries) ||
      !out->WriteU16(numPalettes) ||
      !out->WriteU16(numColorRecords) ||
      !out->WriteU32(colorRecordsArrayOffset)) {
    return Error("Failed to write CPAL header");
  }

  for (auto i : this->colorRecordIndices) {
    if (!out->WriteU16(i)) {
      return Error("Failed to write color record indices");
    }
  }

  if (this->version == 1) {
    size_t paletteTypesArrayOffset = 0;
    if (!this->paletteTypes.empty()) {
      assert(paletteTypes.size() == numPalettes);
      paletteTypesArrayOffset = totalLen;
      totalLen += numPalettes * sizeof(uint32_t);
    }

    size_t paletteLabelsArrayOffset = 0;
    if (!this->paletteLabels.empty()) {
      assert(paletteLabels.size() == numPalettes);
      paletteLabelsArrayOffset = totalLen;
      totalLen += numPalettes * sizeof(uint16_t);
    }

    size_t paletteEntryLabelsArrayOffset = 0;
    if (!this->paletteEntryLabels.empty()) {
      assert(paletteEntryLabels.size() == this->num_palette_entries);
      paletteEntryLabelsArrayOffset = totalLen;
      totalLen += this->num_palette_entries * sizeof(uint16_t);
    }

    if (!out->WriteU32(paletteTypesArrayOffset) ||
        !out->WriteU32(paletteLabelsArrayOffset) ||
        !out->WriteU32(paletteEntryLabelsArrayOffset)) {
      return Error("Failed to write CPAL v.1 header");
    }
  }

  for (auto i : this->colorRecords) {
    if (!out->WriteU32(i)) {
      return Error("Failed to write color records");
    }
  }

  if (this->version == 1) {
    for (auto i : this->paletteTypes) {
      if (!out->WriteU32(i)) {
        return Error("Failed to write palette types");
      }
    }

    for (auto i : this->paletteLabels) {
      if (!out->WriteU16(i)) {
        return Error("Failed to write palette labels");
      }
    }

    for (auto i : this->paletteEntryLabels) {
      if (!out->WriteU16(i)) {
        return Error("Failed to write palette entry labels");
      }
    }
  }

  assert(size_t(out->Tell() - start) == totalLen);

  return true;
}

}  // namespace ots

#undef TABLE_NAME
