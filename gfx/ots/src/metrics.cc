// Copyright (c) 2011-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "metrics.h"

#include "head.h"
#include "maxp.h"

// OpenType horizontal and vertical common header format
// http://www.microsoft.com/typography/otspec/hhea.htm
// http://www.microsoft.com/typography/otspec/vhea.htm

namespace ots {

bool OpenTypeMetricsHeader::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // Skip already read version.
  if (!table.Skip(4)) {
    return false;
  }

  if (!table.ReadS16(&this->ascent) ||
      !table.ReadS16(&this->descent) ||
      !table.ReadS16(&this->linegap) ||
      !table.ReadU16(&this->adv_width_max) ||
      !table.ReadS16(&this->min_sb1) ||
      !table.ReadS16(&this->min_sb2) ||
      !table.ReadS16(&this->max_extent) ||
      !table.ReadS16(&this->caret_slope_rise) ||
      !table.ReadS16(&this->caret_slope_run) ||
      !table.ReadS16(&this->caret_offset)) {
    return Error("Failed to read table");
  }

  if (this->ascent < 0) {
    Warning("bad ascent: %d", this->ascent);
    this->ascent = 0;
  }
  if (this->linegap < 0) {
    Warning("bad linegap: %d", this->linegap);
    this->linegap = 0;
  }

  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));
  if (!head) {
    return Error("Missing head font table");
  }

  // if the font is non-slanted, caret_offset should be zero.
  if (!(head->mac_style & 2) &&
      (this->caret_offset != 0)) {
    Warning("bad caret offset: %d", this->caret_offset);
    this->caret_offset = 0;
  }

  // skip the reserved bytes
  if (!table.Skip(8)) {
    return Error("Failed to read reserverd bytes");
  }

  int16_t data_format;
  if (!table.ReadS16(&data_format)) {
    return Error("Failed to read metricDataFormat");
  }
  if (data_format) {
    return Error("Bad metricDataFormat: %d", data_format);
  }

  if (!table.ReadU16(&this->num_metrics)) {
    return Error("Failed to read number of metrics");
  }

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Missing maxp font table");
  }

  if (this->num_metrics > maxp->num_glyphs) {
    return Error("Bad number of metrics %d", this->num_metrics);
  }

  return true;
}

bool OpenTypeMetricsHeader::Serialize(OTSStream *out) {
  if (!out->WriteU32(this->version) ||
      !out->WriteS16(this->ascent) ||
      !out->WriteS16(this->descent) ||
      !out->WriteS16(this->linegap) ||
      !out->WriteU16(this->adv_width_max) ||
      !out->WriteS16(this->min_sb1) ||
      !out->WriteS16(this->min_sb2) ||
      !out->WriteS16(this->max_extent) ||
      !out->WriteS16(this->caret_slope_rise) ||
      !out->WriteS16(this->caret_slope_run) ||
      !out->WriteS16(this->caret_offset) ||
      !out->WriteR64(0) ||  // reserved
      !out->WriteS16(0) ||  // metric data format
      !out->WriteU16(this->num_metrics)) {
    return Error("Failed to write metrics");
  }

  return true;
}

bool OpenTypeMetricsTable::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  // OpenTypeMetricsHeader is a superclass of both 'hhea' and 'vhea',
  // so the cast here is OK, whichever m_header_tag we have.
  OpenTypeMetricsHeader *header = static_cast<OpenTypeMetricsHeader*>(
      GetFont()->GetTypedTable(m_header_tag));
  if (!header) {
    return Error("Required %c%c%c%c table missing", OTS_UNTAG(m_header_tag));
  }
  // |num_metrics| is a uint16_t, so it's bounded < 65536. This limits that
  // amount of memory that we'll allocate for this to a sane amount.
  const unsigned num_metrics = header->num_metrics;

  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  if (num_metrics > maxp->num_glyphs) {
    return Error("Bad number of metrics %d", num_metrics);
  }
  if (!num_metrics) {
    return Error("No metrics!");
  }
  const unsigned num_sbs = maxp->num_glyphs - num_metrics;

  this->entries.reserve(num_metrics);
  for (unsigned i = 0; i < num_metrics; ++i) {
    uint16_t adv = 0;
    int16_t sb = 0;
    if (!table.ReadU16(&adv) || !table.ReadS16(&sb)) {
      return Error("Failed to read metric %d", i);
    }
    this->entries.push_back(std::make_pair(adv, sb));
  }

  this->sbs.reserve(num_sbs);
  for (unsigned i = 0; i < num_sbs; ++i) {
    int16_t sb;
    if (!table.ReadS16(&sb)) {
      // Some Japanese fonts (e.g., mona.ttf) fail this test.
      return Error("Failed to read side bearing %d", i + num_metrics);
    }
    this->sbs.push_back(sb);
  }

  return true;
}

bool OpenTypeMetricsTable::Serialize(OTSStream *out) {
  for (unsigned i = 0; i < this->entries.size(); ++i) {
    if (!out->WriteU16(this->entries[i].first) ||
        !out->WriteS16(this->entries[i].second)) {
      return Error("Failed to write metric %d", i);
    }
  }

  for (unsigned i = 0; i < this->sbs.size(); ++i) {
    if (!out->WriteS16(this->sbs[i])) {
      return Error("Failed to write side bearing %ld", i + this->entries.size());
    }
  }

  return true;
}

}  // namespace ots
