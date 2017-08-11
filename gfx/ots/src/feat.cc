// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "feat.h"

#include "name.h"

namespace ots {

bool OpenTypeFEAT::Parse(const uint8_t* data, size_t length) {
  if (GetFont()->dropped_graphite) {
    return Drop("Skipping Graphite table");
  }
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return DropGraphite("Failed to read version");
  }
  if (this->version >> 16 != 1 && this->version >> 16 != 2) {
    return DropGraphite("Unsupported table version: %u", this->version >> 16);
  }
  if (!table.ReadU16(&this->numFeat)) {
    return DropGraphite("Failed to read numFeat");
  }
  if (!table.ReadU16(&this->reserved)) {
    return DropGraphite("Failed to read reserved");
  }
  if (this->reserved != 0) {
    Warning("Nonzero reserved");
  }
  if (!table.ReadU32(&this->reserved2)) {
    return DropGraphite("Failed to read valid reserved2");
  }
  if (this->reserved2 != 0) {
    Warning("Nonzero reserved2");
  }

  std::unordered_set<size_t> unverified;
  //this->features.resize(this->numFeat, this);
  for (unsigned i = 0; i < this->numFeat; ++i) {
    this->features.emplace_back(this);
    FeatureDefn& feature = this->features[i];
    if (!feature.ParsePart(table)) {
      return DropGraphite("Failed to read features[%u]", i);
    }
    this->feature_ids.insert(feature.id);
    for (unsigned j = 0; j < feature.numSettings; ++j) {
      size_t offset = feature.offset + j * 4;
      if (offset < feature.offset || offset > length) {
        return DropGraphite("Invalid FeatSettingDefn offset %zu/%zu",
                            offset, length);
      }
      unverified.insert(offset);
        // need to verify that this FeatureDefn points to valid
        // FeatureSettingDefn
    }
  }

  while (table.remaining()) {
    bool used = unverified.erase(table.offset());
    FeatureSettingDefn featSetting(this);
    if (!featSetting.ParsePart(table, used)) {
      return DropGraphite("Failed to read a FeatureSettingDefn");
    }
    featSettings.push_back(featSetting);
  }

  if (!unverified.empty()) {
    return DropGraphite("%zu incorrect offsets into featSettings",
                        unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeFEAT::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !out->WriteU16(this->numFeat) ||
      !out->WriteU16(this->reserved) ||
      !out->WriteU32(this->reserved2) ||
      !SerializeParts(this->features, out) ||
      !SerializeParts(this->featSettings, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeFEAT::IsValidFeatureId(uint32_t id) const {
  return feature_ids.count(id);
}

bool OpenTypeFEAT::FeatureDefn::ParsePart(Buffer& table) {
  OpenTypeNAME* name = static_cast<OpenTypeNAME*>(
      parent->GetFont()->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return parent->Error("FeatureDefn: Required name table is missing");
  }

  if (parent->version >> 16 >= 2 && !table.ReadU32(&this->id)) {
    return parent->Error("FeatureDefn: Failed to read id");
  }
  if (parent->version >> 16 == 1)  {
    uint16_t id;
    if (!table.ReadU16(&id)) {
      return parent->Error("FeatureDefn: Failed to read id");
    }
    this->id = id;
  }
  if (!table.ReadU16(&this->numSettings)) {
    return parent->Error("FeatureDefn: Failed to read numSettings");
  }
  if (parent->version >> 16 >= 2) {
    if (!table.ReadU16(&this->reserved)) {
      return parent->Error("FeatureDefn: Failed to read reserved");
    }
    if (this->reserved != 0) {
      parent->Warning("FeatureDefn: Nonzero reserved");
    }
  }
  if (!table.ReadU32(&this->offset)) {
    return parent->Error("FeatureDefn: Failed to read offset");
  }  // validity of offset verified in OpenTypeFEAT::Parse
  if (!table.ReadU16(&this->flags)) {
    return parent->Error("FeatureDefn: Failed to read flags");
  }
  if ((this->flags & RESERVED) != 0) {
    this->flags &= ~RESERVED;
    parent->Warning("FeatureDefn: Nonzero (flags & 0x%x) repaired", RESERVED);
  }
  if (this->flags & HAS_DEFAULT_SETTING &&
      (this->flags & DEFAULT_SETTING) >= this->numSettings) {
    return parent->Error("FeatureDefn: (flags & 0x%x) is set but (flags & 0x%x "
                         "is not a valid setting index", HAS_DEFAULT_SETTING,
                         DEFAULT_SETTING);
  }
  if (!table.ReadU16(&this->label)) {
    return parent->Error("FeatureDefn: Failed to read label");
  }
  if (!name->IsValidNameId(this->label)) {
    if (this->id == 1 && name->IsValidNameId(this->label, true)) {
      parent->Warning("FeatureDefn: Missing NameRecord repaired for feature"
                      " with id=%u, label=%u", this->id, this->label);
    }
    else {
      return parent->Error("FeatureDefn: Invalid label");
    }
  }
  return true;
}

bool OpenTypeFEAT::FeatureDefn::SerializePart(OTSStream* out) const {
  if ((parent->version >> 16 >= 2 && !out->WriteU32(this->id)) ||
      (parent->version >> 16 == 1 &&
       !out->WriteU16(static_cast<uint16_t>(this->id))) ||
      !out->WriteU16(this->numSettings) ||
      (parent->version >> 16 >= 2 && !out->WriteU16(this->reserved)) ||
      !out->WriteU32(this->offset) ||
      !out->WriteU16(this->flags) ||
      !out->WriteU16(this->label)) {
    return parent->Error("FeatureDefn: Failed to write");
  }
  return true;
}

bool OpenTypeFEAT::FeatureSettingDefn::ParsePart(Buffer& table, bool used) {
  OpenTypeNAME* name = static_cast<OpenTypeNAME*>(
      parent->GetFont()->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return parent->Error("FeatureSettingDefn: Required name table is missing");
  }

  if (!table.ReadS16(&this->value)) {
    return parent->Error("FeatureSettingDefn: Failed to read value");
  }
  if (!table.ReadU16(&this->label) ||
      (used && !name->IsValidNameId(this->label))) {
    return parent->Error("FeatureSettingDefn: Failed to read valid label");
  }
  return true;
}

bool OpenTypeFEAT::FeatureSettingDefn::SerializePart(OTSStream* out) const {
  if (!out->WriteS16(this->value) ||
      !out->WriteU16(this->label)) {
    return parent->Error("FeatureSettingDefn: Failed to write");
  }
  return true;
}

}  // namespace ots
