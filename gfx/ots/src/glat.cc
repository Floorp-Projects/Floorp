// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glat.h"

#include "gloc.h"
#include "mozilla/Compression.h"
#include <list>

namespace ots {

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v1
// -----------------------------------------------------------------------------

bool OpenTypeGLAT_v1::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  OpenTypeGLOC* gloc = static_cast<OpenTypeGLOC*>(
      GetFont()->GetTypedTable(OTS_TAG_GLOC));
  if (!gloc) {
    return DropGraphite("Required Gloc table is missing");
  }

  if (!table.ReadU32(&this->version) || this->version >> 16 != 1) {
    return DropGraphite("Failed to read version");
  }

  const std::vector<uint32_t>& locations = gloc->GetLocations();
  if (locations.empty()) {
    return DropGraphite("No locations from Gloc table");
  }
  std::list<uint32_t> unverified(locations.begin(), locations.end());
  while (table.remaining()) {
    GlatEntry entry(this);
    if (table.offset() > unverified.front()) {
      return DropGraphite("Offset check failed for a GlatEntry");
    }
    if (table.offset() == unverified.front()) {
      unverified.pop_front();
    }
    if (unverified.empty()) {
      return DropGraphite("Expected more locations");
    }
    if (!entry.ParsePart(table)) {
      return DropGraphite("Failed to read a GlatEntry");
    }
    this->entries.push_back(entry);
  }

  if (unverified.size() != 1 || unverified.front() != table.offset()) {
    return DropGraphite("%zu location(s) could not be verified", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLAT_v1::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !SerializeParts(this->entries, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeGLAT_v1::GlatEntry::ParsePart(Buffer& table) {
  if (!table.ReadU8(&this->attNum)) {
    return parent->Error("GlatEntry: Failed to read attNum");
  }
  if (!table.ReadU8(&this->num)) {
    return parent->Error("GlatEntry: Failed to read num");
  }

  //this->attributes.resize(this->num);
  for (int i = 0; i < this->num; ++i) {
    this->attributes.emplace_back();
    if (!table.ReadS16(&this->attributes[i])) {
      return parent->Error("GlatEntry: Failed to read attribute %u", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v1::GlatEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->attNum) ||
      !out->WriteU8(this->num) ||
      !SerializeParts(this->attributes, out)) {
    return parent->Error("GlatEntry: Failed to write");
  }
  return true;
}

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v2
// -----------------------------------------------------------------------------

bool OpenTypeGLAT_v2::Parse(const uint8_t* data, size_t length) {
  Buffer table(data, length);
  OpenTypeGLOC* gloc = static_cast<OpenTypeGLOC*>(
      GetFont()->GetTypedTable(OTS_TAG_GLOC));
  if (!gloc) {
    return DropGraphite("Required Gloc table is missing");
  }

  if (!table.ReadU32(&this->version) || this->version >> 16 != 1) {
    return DropGraphite("Failed to read version");
  }

  const std::vector<uint32_t>& locations = gloc->GetLocations();
  if (locations.empty()) {
    return DropGraphite("No locations from Gloc table");
  }
  std::list<uint32_t> unverified(locations.begin(), locations.end());
  while (table.remaining()) {
    GlatEntry entry(this);
    if (table.offset() > unverified.front()) {
      return DropGraphite("Offset check failed for a GlatEntry");
    }
    if (table.offset() == unverified.front()) {
      unverified.pop_front();
    }
    if (unverified.empty()) {
      return DropGraphite("Expected more locations");
    }
    if (!entry.ParsePart(table)) {
      return DropGraphite("Failed to read a GlatEntry");
    }
    this->entries.push_back(entry);
  }

  if (unverified.size() != 1 || unverified.front() != table.offset()) {
    return DropGraphite("%zu location(s) could not be verified", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLAT_v2::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !SerializeParts(this->entries, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeGLAT_v2::GlatEntry::ParsePart(Buffer& table) {
  if (!table.ReadS16(&this->attNum)) {
    return parent->Error("GlatEntry: Failed to read attNum");
  }
  if (!table.ReadS16(&this->num) || this->num < 0) {
    return parent->Error("GlatEntry: Failed to read valid num");
  }

  //this->attributes.resize(this->num);
  for (int i = 0; i < this->num; ++i) {
    this->attributes.emplace_back();
    if (!table.ReadS16(&this->attributes[i])) {
      return parent->Error("GlatEntry: Failed to read attribute %u", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v2::GlatEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteS16(this->attNum) ||
      !out->WriteS16(this->num) ||
      !SerializeParts(this->attributes, out)) {
    return parent->Error("GlatEntry: Failed to write");
  }
  return true;
}

// -----------------------------------------------------------------------------
// OpenTypeGLAT_v3
// -----------------------------------------------------------------------------

bool OpenTypeGLAT_v3::Parse(const uint8_t* data, size_t length,
                            bool prevent_decompression) {
  Buffer table(data, length);
  OpenTypeGLOC* gloc = static_cast<OpenTypeGLOC*>(
      GetFont()->GetTypedTable(OTS_TAG_GLOC));
  if (!gloc) {
    return DropGraphite("Required Gloc table is missing");
  }

  if (!table.ReadU32(&this->version) || this->version >> 16 != 3) {
    return DropGraphite("Failed to read version");
  }
  if (!table.ReadU32(&this->compHead)) {
    return DropGraphite("Failed to read compression header");
  }
  switch ((this->compHead & SCHEME) >> 27) {
    case 0:  // uncompressed
      break;
    case 1: {  // lz4
      if (prevent_decompression) {
        return DropGraphite("Illegal nested compression");
      }
      std::vector<uint8_t> decompressed(this->compHead & FULL_SIZE);
      size_t outputSize = 0;
      bool ret = mozilla::Compression::LZ4::decompressPartial(
          reinterpret_cast<const char*>(data + table.offset()),
          table.remaining(),  // input buffer size (input size + padding)
          reinterpret_cast<char*>(decompressed.data()),
          decompressed.size(),  // target output size
          &outputSize);  // return output size
      if (!ret || outputSize != decompressed.size()) {
        return DropGraphite("Decompression failed");
      }
      return this->Parse(decompressed.data(), decompressed.size(), true);
    }
    default:
      return DropGraphite("Unknown compression scheme");
  }
  if (this->compHead & RESERVED) {
    Warning("Nonzero reserved");
  }

  const std::vector<uint32_t>& locations = gloc->GetLocations();
  if (locations.empty()) {
    return DropGraphite("No locations from Gloc table");
  }
  std::list<uint32_t> unverified(locations.begin(), locations.end());
  //this->entries.resize(locations.size() - 1, this);
  for (size_t i = 0; i < locations.size() - 1; ++i) {
    this->entries.emplace_back(this);
    if (table.offset() != unverified.front()) {
      return DropGraphite("Offset check failed for a GlyphAttrs");
    }
    unverified.pop_front();
    if (!this->entries[i].ParsePart(table,
                                    unverified.front() - table.offset())) {
        // unverified.front() is guaranteed to exist because of the number of
        // iterations of this loop
      return DropGraphite("Failed to read a GlyphAttrs");
    }
  }

  if (unverified.size() != 1 || unverified.front() != table.offset()) {
    return DropGraphite("%zu location(s) could not be verified", unverified.size());
  }
  if (table.remaining()) {
    return Warning("%zu bytes unparsed", table.remaining());
  }
  return true;
}

bool OpenTypeGLAT_v3::Serialize(OTSStream* out) {
  if (!out->WriteU32(this->version) ||
      !out->WriteU32(this->compHead) ||
      !SerializeParts(this->entries, out)) {
    return Error("Failed to write table");
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::ParsePart(Buffer& table, const size_t size) {
  size_t init_offset = table.offset();
  if (parent->compHead & OCTABOXES && !octabox.ParsePart(table)) {
    // parent->flags & 0b1: octaboxes are present flag
    return parent->Error("GlyphAttrs: Failed to read octabox");
  }

  while (table.offset() < init_offset + size) {
    GlatEntry entry(parent);
    if (!entry.ParsePart(table)) {
      return parent->Error("GlyphAttrs: Failed to read a GlatEntry");
    }
    this->entries.push_back(entry);
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::SerializePart(OTSStream* out) const {
  if ((parent->compHead & OCTABOXES && !octabox.SerializePart(out)) ||
      !SerializeParts(this->entries, out)) {
    return parent->Error("GlyphAttrs: Failed to write");
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::
OctaboxMetrics::ParsePart(Buffer& table) {
  if (!table.ReadU16(&this->subbox_bitmap)) {
    return parent->Error("OctaboxMetrics: Failed to read subbox_bitmap");
  }
  if (!table.ReadU8(&this->diag_neg_min)) {
    return parent->Error("OctaboxMetrics: Failed to read diag_neg_min");
  }
  if (!table.ReadU8(&this->diag_neg_max) ||
      this->diag_neg_max < this->diag_neg_min) {
    return parent->Error("OctaboxMetrics: Failed to read valid diag_neg_max");
  }
  if (!table.ReadU8(&this->diag_pos_min)) {
    return parent->Error("OctaboxMetrics: Failed to read diag_pos_min");
  }
  if (!table.ReadU8(&this->diag_pos_max) ||
      this->diag_pos_max < this->diag_pos_min) {
    return parent->Error("OctaboxMetrics: Failed to read valid diag_pos_max");
  }

  unsigned subboxes_len = 0;  // count of 1's in this->subbox_bitmap
  for (uint16_t i = this->subbox_bitmap; i; i >>= 1) {
    if (i & 0b1) {
      ++subboxes_len;
    }
  }
  //this->subboxes.resize(subboxes_len, parent);
  for (unsigned i = 0; i < subboxes_len; i++) {
    this->subboxes.emplace_back(parent);
    if (!this->subboxes[i].ParsePart(table)) {
      return parent->Error("OctaboxMetrics: Failed to read subbox[%u]", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::
OctaboxMetrics::SerializePart(OTSStream* out) const {
  if (!out->WriteU16(this->subbox_bitmap) ||
      !out->WriteU8(this->diag_neg_min) ||
      !out->WriteU8(this->diag_neg_max) ||
      !out->WriteU8(this->diag_pos_min) ||
      !out->WriteU8(this->diag_pos_max) ||
      !SerializeParts(this->subboxes, out)) {
    return parent->Error("OctaboxMetrics: Failed to write");
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::OctaboxMetrics::
SubboxEntry::ParsePart(Buffer& table) {
  if (!table.ReadU8(&this->left)) {
    return parent->Error("SubboxEntry: Failed to read left");
  }
  if (!table.ReadU8(&this->right) || this->right < this->left) {
    return parent->Error("SubboxEntry: Failed to read valid right");
  }
  if (!table.ReadU8(&this->bottom)) {
    return parent->Error("SubboxEntry: Failed to read bottom");
  }
  if (!table.ReadU8(&this->top) || this->top < this->bottom) {
    return parent->Error("SubboxEntry: Failed to read valid top");
  }
  if (!table.ReadU8(&this->diag_pos_min)) {
    return parent->Error("SubboxEntry: Failed to read diag_pos_min");
  }
  if (!table.ReadU8(&this->diag_pos_max) ||
      this->diag_pos_max < this->diag_pos_min) {
    return parent->Error("SubboxEntry: Failed to read valid diag_pos_max");
  }
  if (!table.ReadU8(&this->diag_neg_min)) {
    return parent->Error("SubboxEntry: Failed to read diag_neg_min");
  }
  if (!table.ReadU8(&this->diag_neg_max) ||
      this->diag_neg_max < this->diag_neg_min) {
    return parent->Error("SubboxEntry: Failed to read valid diag_neg_max");
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::OctaboxMetrics::
SubboxEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteU8(this->left) ||
      !out->WriteU8(this->right) ||
      !out->WriteU8(this->bottom) ||
      !out->WriteU8(this->top) ||
      !out->WriteU8(this->diag_pos_min) ||
      !out->WriteU8(this->diag_pos_max) ||
      !out->WriteU8(this->diag_neg_min) ||
      !out->WriteU8(this->diag_neg_max)) {
    return parent->Error("SubboxEntry: Failed to write");
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::
GlatEntry::ParsePart(Buffer& table) {
  if (!table.ReadS16(&this->attNum)) {
    return parent->Error("GlatEntry: Failed to read attNum");
  }
  if (!table.ReadS16(&this->num) || this->num < 0) {
    return parent->Error("GlatEntry: Failed to read valid num");
  }

  //this->attributes.resize(this->num);
  for (int i = 0; i < this->num; ++i) {
    this->attributes.emplace_back();
    if (!table.ReadS16(&this->attributes[i])) {
      return parent->Error("GlatEntry: Failed to read attribute %u", i);
    }
  }
  return true;
}

bool OpenTypeGLAT_v3::GlyphAttrs::
GlatEntry::SerializePart(OTSStream* out) const {
  if (!out->WriteS16(this->attNum) ||
      !out->WriteS16(this->num) ||
      !SerializeParts(this->attributes, out)) {
    return parent->Error("GlatEntry: Failed to write");
  }
  return true;
}

// -----------------------------------------------------------------------------
// OpenTypeGLAT
// -----------------------------------------------------------------------------

bool OpenTypeGLAT::Parse(const uint8_t* data, size_t length) {
  if (GetFont()->dropped_graphite) {
    return Drop("Skipping Graphite table");
  }
  Buffer table(data, length);
  uint32_t version;
  if (!table.ReadU32(&version)) {
    return DropGraphite("Failed to read version");
  }
  switch (version >> 16) {
    case 1:
      this->handler = new OpenTypeGLAT_v1(this->font, this->tag);
      break;
    case 2:
      this->handler = new OpenTypeGLAT_v2(this->font, this->tag);
      break;
    case 3: {
      this->handler = new OpenTypeGLAT_v3(this->font, this->tag);
      break;
    }
    default:
      return DropGraphite("Unsupported table version: %u", version >> 16);
  }
  return this->handler->Parse(data, length);
}

bool OpenTypeGLAT::Serialize(OTSStream* out) {
  if (!this->handler) {
    return Error("No Glat table parsed");
  }
  return this->handler->Serialize(out);
}

}  // namespace ots
