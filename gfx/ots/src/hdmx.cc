// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "hdmx.h"
#include "head.h"
#include "maxp.h"

// hdmx - Horizontal Device Metrics
// http://www.microsoft.com/typography/otspec/hdmx.htm

namespace ots {

bool OpenTypeHDMX::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));
  if (!head || !maxp) {
    return Error("Missing maxp or head tables in font, needed by hdmx");
  }

  if ((head->flags & 0x14) == 0) {
    // http://www.microsoft.com/typography/otspec/recom.htm#hdmx
    return Drop("the table should not be present when bit 2 and 4 of the "
                "head->flags are not set");
  }

  int16_t num_recs;
  if (!table.ReadU16(&this->version) ||
      !table.ReadS16(&num_recs) ||
      !table.ReadS32(&this->size_device_record)) {
    return Error("Failed to read table header");
  }
  if (this->version != 0) {
    return Drop("Unsupported version: %u", this->version);
  }
  if (num_recs <= 0) {
    return Drop("Bad numRecords: %d", num_recs);
  }
  const int32_t actual_size_device_record = maxp->num_glyphs + 2;
  if (this->size_device_record < actual_size_device_record) {
    return Drop("Bad sizeDeviceRecord: %d", this->size_device_record);
  }

  this->pad_len = this->size_device_record - actual_size_device_record;
  if (this->pad_len > 3) {
    return Error("Bad DeviceRecord padding %d", this->pad_len);
  }

  uint8_t last_pixel_size = 0;
  this->records.reserve(num_recs);
  for (int i = 0; i < num_recs; ++i) {
    OpenTypeHDMXDeviceRecord rec;

    if (!table.ReadU8(&rec.pixel_size) ||
        !table.ReadU8(&rec.max_width)) {
      return Error("Failed to read DeviceRecord %d", i);
    }
    if ((i != 0) &&
        (rec.pixel_size <= last_pixel_size)) {
      return Drop("DeviceRecord's are not sorted");
    }
    last_pixel_size = rec.pixel_size;

    rec.widths.reserve(maxp->num_glyphs);
    for (unsigned j = 0; j < maxp->num_glyphs; ++j) {
      uint8_t width;
      if (!table.ReadU8(&width)) {
        return Error("Failed to read glyph width %d in DeviceRecord %d", j, i);
      }
      rec.widths.push_back(width);
    }

    if ((this->pad_len > 0) &&
        !table.Skip(this->pad_len)) {
      return Error("DeviceRecord %d should be padded by %d", i, this->pad_len);
    }

    this->records.push_back(rec);
  }

  return true;
}

bool OpenTypeHDMX::ShouldSerialize() {
  return Table::ShouldSerialize() &&
         // this table is not for CFF fonts.
         GetFont()->GetTable(OTS_TAG_GLYF) != NULL;
}

bool OpenTypeHDMX::Serialize(OTSStream *out) {
  const int16_t num_recs = static_cast<int16_t>(this->records.size());
  if (this->records.size() >
          static_cast<size_t>(std::numeric_limits<int16_t>::max()) ||
      !out->WriteU16(this->version) ||
      !out->WriteS16(num_recs) ||
      !out->WriteS32(this->size_device_record)) {
    return Error("Failed to write table header");
  }

  for (int16_t i = 0; i < num_recs; ++i) {
    const OpenTypeHDMXDeviceRecord& rec = this->records[i];
    if (!out->Write(&rec.pixel_size, 1) ||
        !out->Write(&rec.max_width, 1) ||
        !out->Write(&rec.widths[0], rec.widths.size())) {
      return Error("Failed to write DeviceRecord %d", i);
    }
    if ((this->pad_len > 0) &&
        !out->Write((const uint8_t *)"\x00\x00\x00", this->pad_len)) {
      return Error("Failed to write padding of length %d", this->pad_len);
    }
  }

  return true;
}

}  // namespace ots
