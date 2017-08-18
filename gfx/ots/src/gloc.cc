// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gloc.h"

#include "name.h"

namespace ots {

bool OpenTypeGLOC::Parse(const uint8_t* data, size_t length) {
  if (GetFont()->dropped_graphite) {
    return Drop("Skipping Graphite table");
  }
  Buffer table(data, length);
  OpenTypeNAME* name = static_cast<OpenTypeNAME*>(
      GetFont()->GetTypedTable(OTS_TAG_NAME));
  if (!name) {
    return DropGraphite("Required name table is missing");
  }

  if (!table.ReadU32(&this->version)) {
    return DropGraphite("Failed to read version");
  }
  if (this->version >> 16 != 1) {
    return DropGraphite("Unsupported table version: %u", this->version >> 16);
  }
  if (!table.ReadU16(&this->flags) || this->flags > 0b11) {
    return DropGraphite("Failed to read valid flags");
  }
  if (!table.ReadU16(&this->numAttribs)) {
    return DropGraphite("Failed to read numAttribs");
  }

  if (this->flags & ATTRIB_IDS && this->numAttribs * sizeof(uint16_t) >
                                  table.remaining()) {
    return DropGraphite("Failed to calulate length of locations");
  }
  size_t locations_len = (table.remaining() -
    (this->flags & ATTRIB_IDS ? this->numAttribs * sizeof(uint16_t) : 0)) /
    (this->flags & LONG_FORMAT ? sizeof(uint32_t) : sizeof(uint16_t));
  //this->locations.resize(locations_len);
  if (this->flags & LONG_FORMAT) {
    unsigned long last_location = 0;
    for (size_t i = 0; i < locations_len; ++i) {
      this->locations.emplace_back();
      uint32_t& location = this->locations[i];
      if (!table.ReadU32(&location) || location < last_location) {
        return DropGraphite("Failed to read valid locations[%lu]", i);
      }
      last_location = location;
    }
  } else {  // short (16-bit) offsets
    unsigned last_location = 0;
    for (size_t i = 0; i < locations_len; ++i) {
      uint16_t location;
      if (!table.ReadU16(&location) || location < last_location) {
        return DropGraphite("Failed to read valid locations[%lu]", i);
      }
      last_location = location;
      this->locations.push_back(static_cast<uint32_t>(location));
    }
  }
  if (this->locations.empty()) {
    return DropGraphite("No locations");
  }

  if (this->flags & ATTRIB_IDS) {  // attribIds array present
    //this->attribIds.resize(numAttribs);
    for (unsigned i = 0; i < this->numAttribs; ++i) {
      this->attribIds.emplace_back();
      if (!table.ReadU16(&this->attribIds[i]) ||
          !name->IsValidNameId(this->attribIds[i])) {
        return DropGraphite("Failed to read valid attribIds[%u]", i);
      }
    }
  }

  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLOC::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !out->WriteU16(this->flags) ||
      !out->WriteU16(this->numAttribs) ||
      (this->flags & LONG_FORMAT ? !SerializeParts(this->locations, out) :
       ![&] {
         for (uint32_t location : this->locations) {
           if (!out->WriteU16(static_cast<uint16_t>(location))) {
             return false;
           }
         }
         return true;
       }()) ||
      (this->flags & ATTRIB_IDS && !SerializeParts(this->attribIds, out))) {
    return Error("Failed to write table");
  }
  return true;
}

const std::vector<uint32_t>& OpenTypeGLOC::GetLocations() {
  return this->locations;
}

}  // namespace ots
