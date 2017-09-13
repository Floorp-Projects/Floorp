// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sill.h"

#include "feat.h"
#include <cmath>
#include <unordered_set>

namespace ots {

bool OpenTypeSILL::Parse(const uint8_t* data, size_t length) {
  if (GetFont()->dropped_graphite) {
    return Drop("Skipping Graphite table");
  }
  Buffer table(data, length);

  if (!table.ReadU32(&this->version) || this->version >> 16 != 1) {
    return Drop("Failed to read valid version");
  }
  if (!table.ReadU16(&this->numLangs)) {
    return Drop("Failed to read numLangs");
  }

  // The following three fields are deprecated and ignored. We fix them up here
  // just for internal consistency, but the Graphite engine doesn't care.
  if (!table.ReadU16(&this->searchRange) ||
      !table.ReadU16(&this->entrySelector) ||
      !table.ReadU16(&this->rangeShift)) {
    return Drop("Failed to read searchRange..rangeShift");
  }
  if (this->numLangs == 0) {
    if (this->searchRange != 0 || this->entrySelector != 0 || this->rangeShift != 0) {
      this->searchRange = this->entrySelector = this->rangeShift = 0;
    }
  } else {
    unsigned floorLog2 = std::floor(std::log2(this->numLangs));
    if (this->searchRange != (unsigned)std::pow(2, floorLog2) ||
        this->entrySelector != floorLog2 ||
        this->rangeShift != this->numLangs - this->searchRange) {
      this->searchRange = (unsigned)std::pow(2, floorLog2);
      this->entrySelector = floorLog2;
      this->rangeShift = this->numLangs - this->searchRange;
    }
  }

  std::unordered_set<size_t> unverified;
  //this->entries.resize(static_cast<unsigned long>(this->numLangs) + 1, this);
  for (unsigned long i = 0; i <= this->numLangs; ++i) {
    this->entries.emplace_back(this);
    LanguageEntry& entry = this->entries[i];
    if (!entry.ParsePart(table)) {
      return Drop("Failed to read entries[%u]", i);
    }
    for (unsigned j = 0; j < entry.numSettings; ++j) {
      size_t offset = entry.offset + j * 8;
      if (offset < entry.offset || offset > length) {
        return DropGraphite("Invalid LangFeatureSetting offset %zu/%zu",
                            offset, length);
      }
      unverified.insert(offset);
        // need to verify that this LanguageEntry points to valid
        // LangFeatureSetting
    }
  }

  while (table.remaining()) {
    unverified.erase(table.offset());
    LangFeatureSetting setting(this);
    if (!setting.ParsePart(table)) {
      return Drop("Failed to read a LangFeatureSetting");
    }
    settings.push_back(setting);
  }

  if (!unverified.empty()) {
    return Drop("%zu incorrect offsets into settings", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeSILL::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !out->WriteU16(this->numLangs) ||
      !out->WriteU16(this->searchRange) ||
      !out->WriteU16(this->entrySelector) ||
      !out->WriteU16(this->rangeShift) ||
      !SerializeParts(this->entries, out) ||
      !SerializeParts(this->settings, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeSILL::LanguageEntry::ParsePart(Buffer& table) {
  if (!table.ReadU8(&this->langcode[0]) ||
      !table.ReadU8(&this->langcode[1]) ||
      !table.ReadU8(&this->langcode[2]) ||
      !table.ReadU8(&this->langcode[3])) {
    return parent->Error("LanguageEntry: Failed to read langcode");
  }
  if (!table.ReadU16(&this->numSettings)) {
    return parent->Error("LanguageEntry: Failed to read numSettings");
  }
  if (!table.ReadU16(&this->offset)) {
    return parent->Error("LanguageEntry: Failed to read offset");
  }
  return true;
}

bool OpenTypeSILL::LanguageEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->langcode[0]) ||
      !out->WriteU8(this->langcode[1]) ||
      !out->WriteU8(this->langcode[2]) ||
      !out->WriteU8(this->langcode[3]) ||
      !out->WriteU16(this->numSettings) ||
      !out->WriteU16(this->offset)) {
    return parent->Error("LanguageEntry: Failed to write");
  }
  return true;
}

bool OpenTypeSILL::LangFeatureSetting::ParsePart(Buffer& table) {
  OpenTypeFEAT* feat = static_cast<OpenTypeFEAT*>(
      parent->GetFont()->GetTypedTable(OTS_TAG_FEAT));
  if (!feat) {
    return parent->Error("FeatureDefn: Required Feat table is missing");
  }

  if (!table.ReadU32(&this->featureId) ||
      !feat->IsValidFeatureId(this->featureId)) {
    return parent->Error("LangFeatureSetting: Failed to read valid featureId");
  }
  if (!table.ReadS16(&this->value)) {
    return parent->Error("LangFeatureSetting: Failed to read value");
  }
  if (!table.ReadU16(&this->reserved)) {
    return parent->Error("LangFeatureSetting: Failed to read reserved");
  }
  if (this->reserved != 0) {
    parent->Warning("LangFeatureSetting: Nonzero reserved");
  }
  return true;
}

bool OpenTypeSILL::LangFeatureSetting::SerializePart(OTSStream* out) const {
  if (!out->WriteU32(this->featureId) ||
      !out->WriteS16(this->value) ||
      !out->WriteU16(this->reserved)) {
    return parent->Error("LangFeatureSetting: Failed to read reserved");
  }
  return true;
}

}  // namespace ots
