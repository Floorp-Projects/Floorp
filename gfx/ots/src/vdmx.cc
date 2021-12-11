// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "vdmx.h"

// VDMX - Vertical Device Metrics
// http://www.microsoft.com/typography/otspec/vdmx.htm

namespace ots {

bool OpenTypeVDMX::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU16(&this->version) ||
      !table.ReadU16(&this->num_recs) ||
      !table.ReadU16(&this->num_ratios)) {
    return Error("Failed to read table header");
  }

  if (this->version > 1) {
    return Drop("Unsupported table version: %u", this->version);
  }

  this->rat_ranges.reserve(this->num_ratios);
  for (unsigned i = 0; i < this->num_ratios; ++i) {
    OpenTypeVDMXRatioRecord rec;

    if (!table.ReadU8(&rec.charset) ||
        !table.ReadU8(&rec.x_ratio) ||
        !table.ReadU8(&rec.y_start_ratio) ||
        !table.ReadU8(&rec.y_end_ratio)) {
      return Error("Failed to read RatioRange record %d", i);
    }

    if (rec.charset > 1) {
      return Drop("Unsupported character set: %u", rec.charset);
    }

    if (rec.y_start_ratio > rec.y_end_ratio) {
      return Drop("Bad y ratio");
    }

    // All values set to zero signal the default grouping to use;
    // if present, this must be the last Ratio group in the table.
    if ((i < this->num_ratios - 1u) &&
        (rec.x_ratio == 0) &&
        (rec.y_start_ratio == 0) &&
        (rec.y_end_ratio == 0)) {
      // workaround for fonts which have 2 or more {0, 0, 0} terminators.
      return Drop("Superfluous terminator found");
    }

    this->rat_ranges.push_back(rec);
  }

  this->offsets.reserve(this->num_ratios);
  const size_t current_offset = table.offset();
  // current_offset is less than (2 bytes * 3) + (4 bytes * USHRT_MAX) = 256k.
  for (unsigned i = 0; i < this->num_ratios; ++i) {
    uint16_t offset;
    if (!table.ReadU16(&offset)) {
      return Error("Failed to read ratio offset %d", i);
    }
    if (current_offset + offset >= length) {  // thus doesn't overflow.
      return Error("Bad ratio offset %d for ration %d", offset, i);
    }

    this->offsets.push_back(offset);
  }

  this->groups.reserve(this->num_recs);
  for (unsigned i = 0; i < this->num_recs; ++i) {
    OpenTypeVDMXGroup group;
    if (!table.ReadU16(&group.recs) ||
        !table.ReadU8(&group.startsz) ||
        !table.ReadU8(&group.endsz)) {
      return Error("Failed to read record header %d", i);
    }
    group.entries.reserve(group.recs);
    for (unsigned j = 0; j < group.recs; ++j) {
      OpenTypeVDMXVTable vt;
      if (!table.ReadU16(&vt.y_pel_height) ||
          !table.ReadS16(&vt.y_max) ||
          !table.ReadS16(&vt.y_min)) {
        return Error("Failed to read reacord %d group %d", i, j);
      }
      if (vt.y_max < vt.y_min) {
        return Drop("bad y min/max");
      }

      // This table must appear in sorted order (sorted by yPelHeight),
      // but need not be continuous.
      if ((j != 0) && (group.entries[j - 1].y_pel_height >= vt.y_pel_height)) {
        return Drop("The table is not sorted");
      }

      group.entries.push_back(vt);
    }
    this->groups.push_back(group);
  }

  return true;
}

bool OpenTypeVDMX::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

bool OpenTypeVDMX::Serialize(OTSStream *out) {
  if (!out->WriteU16(this->version) ||
      !out->WriteU16(this->num_recs) ||
      !out->WriteU16(this->num_ratios)) {
    return Error("Failed to write table header");
  }

  for (unsigned i = 0; i < this->rat_ranges.size(); ++i) {
    const OpenTypeVDMXRatioRecord& rec = this->rat_ranges[i];
    if (!out->Write(&rec.charset, 1) ||
        !out->Write(&rec.x_ratio, 1) ||
        !out->Write(&rec.y_start_ratio, 1) ||
        !out->Write(&rec.y_end_ratio, 1)) {
      return Error("Failed to write RatioRange record %d", i);
    }
  }

  for (unsigned i = 0; i < this->offsets.size(); ++i) {
    if (!out->WriteU16(this->offsets[i])) {
      return Error("Failed to write ratio offset %d", i);
    }
  }

  for (unsigned i = 0; i < this->groups.size(); ++i) {
    const OpenTypeVDMXGroup& group = this->groups[i];
    if (!out->WriteU16(group.recs) ||
        !out->Write(&group.startsz, 1) ||
        !out->Write(&group.endsz, 1)) {
      return Error("Failed to write group %d", i);
    }
    for (unsigned j = 0; j < group.entries.size(); ++j) {
      const OpenTypeVDMXVTable& vt = group.entries[j];
      if (!out->WriteU16(vt.y_pel_height) ||
          !out->WriteS16(vt.y_max) ||
          !out->WriteS16(vt.y_min)) {
        return Error("Failed to write group %d entry %d", i, j);
      }
    }
  }

  return true;
}

}  // namespace ots
