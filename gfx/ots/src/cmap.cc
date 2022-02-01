// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cmap.h"

#include <algorithm>
#include <set>
#include <utility>
#include <vector>

#include "maxp.h"
#include "os2.h"

// cmap - Character To Glyph Index Mapping Table
// http://www.microsoft.com/typography/otspec/cmap.htm

namespace {

struct CMAPSubtableHeader {
  uint16_t platform;
  uint16_t encoding;
  uint32_t offset;
  uint16_t format;
  uint32_t length;
  uint32_t language;
};

struct Subtable314Range {
  uint16_t start_range;
  uint16_t end_range;
  int16_t id_delta;
  uint16_t id_range_offset;
  uint32_t id_range_offset_offset;
};

// Glyph array size for the Mac Roman (format 0) table.
const size_t kFormat0ArraySize = 256;

// The upper limit of the Unicode code point.
const uint32_t kUnicodeUpperLimit = 0x10FFFF;

// The maximum number of UVS records (See below).
const uint32_t kMaxCMAPSelectorRecords = 259;
// The range of UVSes are:
//   0x180B-0x180D (3 code points)
//   0xFE00-0xFE0F (16 code points)
//   0xE0100-0xE01EF (240 code points)
const uint32_t kMongolianVSStart = 0x180B;
const uint32_t kMongolianVSEnd = 0x180D;
const uint32_t kVSStart = 0xFE00;
const uint32_t kVSEnd = 0xFE0F;
const uint32_t kIVSStart = 0xE0100;
const uint32_t kIVSEnd = 0xE01EF;
const uint32_t kUVSUpperLimit = 0xFFFFFF;

} // namespace

namespace ots {

// Parses Format 4 tables
bool OpenTypeCMAP::ParseFormat4(int platform, int encoding,
              const uint8_t *data, size_t length, uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // 0.3.4, 3.0.4 or 3.1.4 subtables are complex and, rather than expanding the
  // whole thing and recompacting it, we validate it and include it verbatim
  // in the output.

  OpenTypeOS2 *os2 = static_cast<OpenTypeOS2*>(
      GetFont()->GetTypedTable(OTS_TAG_OS2));
  if (!os2) {
    return Error("Required OS/2 table missing");
  }

  if (!subtable.Skip(4)) {
    return Error("Can't read 4 bytes at start of cmap format 4 subtable");
  }
  uint16_t language = 0;
  if (!subtable.ReadU16(&language)) {
    return Error("Can't read language");
  }
  if (language) {
    // Platform ID 3 (windows) subtables should have language '0'.
    return Error("Languages should be 0 (%d)", language);
  }

  uint16_t segcountx2, search_range, entry_selector, range_shift;
  segcountx2 = search_range = entry_selector = range_shift = 0;
  if (!subtable.ReadU16(&segcountx2) ||
      !subtable.ReadU16(&search_range) ||
      !subtable.ReadU16(&entry_selector) ||
      !subtable.ReadU16(&range_shift)) {
    return Error("Failed to read subcmap structure");
  }

  if (segcountx2 & 1 || search_range & 1) {
    return Error("Bad subcmap structure");
  }
  const uint16_t segcount = segcountx2 >> 1;
  // There must be at least one segment according the spec.
  if (segcount < 1) {
    return Error("Segcount < 1 (%d)", segcount);
  }

  // log2segcount is the maximal x s.t. 2^x < segcount
  unsigned log2segcount = 0;
  while (1u << (log2segcount + 1) <= segcount) {
    log2segcount++;
  }

  const uint16_t expected_search_range = 2 * 1u << log2segcount;
  if (expected_search_range != search_range) {
    return Error("expected search range != search range (%d != %d)", expected_search_range, search_range);
  }

  if (entry_selector != log2segcount) {
    return Error("entry selector != log2(segement count) (%d != %d)", entry_selector, log2segcount);
  }

  const uint16_t expected_range_shift = segcountx2 - search_range;
  if (range_shift != expected_range_shift) {
    return Error("unexpected range shift (%d != %d)", range_shift, expected_range_shift);
  }

  std::vector<Subtable314Range> ranges(segcount);

  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadU16(&ranges[i].end_range)) {
      return Error("Failed to read segment %d", i);
    }
  }

  uint16_t padding;
  if (!subtable.ReadU16(&padding)) {
    return Error("Failed to read cmap subtable segment padding");
  }
  if (padding) {
    return Error("Non zero cmap subtable segment padding (%d)", padding);
  }

  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadU16(&ranges[i].start_range)) {
      return Error("Failed to read segment start range %d", i);
    }
  }
  for (unsigned i = 0; i < segcount; ++i) {
    if (!subtable.ReadS16(&ranges[i].id_delta)) {
      return Error("Failed to read segment delta %d", i);
    }
  }
  for (unsigned i = 0; i < segcount; ++i) {
    ranges[i].id_range_offset_offset = subtable.offset();
    if (!subtable.ReadU16(&ranges[i].id_range_offset)) {
      return Error("Failed to read segment range offset %d", i);
    }

    if (ranges[i].id_range_offset & 1) {
      // Some font generators seem to put 65535 on id_range_offset
      // for 0xFFFF-0xFFFF range.
      // (e.g., many fonts in http://www.princexml.com/fonts/)
      if (i == segcount - 1u) {
        Warning("bad id_range_offset");
        ranges[i].id_range_offset = 0;
        // The id_range_offset value in the transcoded font will not change
        // since this table is not actually "transcoded" yet.
      } else {
        return Error("Bad segment offset (%d)", ranges[i].id_range_offset);
      }
    }
  }

  // ranges must be ascending order, based on the end_code. Ranges may not
  // overlap.
  for (unsigned i = 1; i < segcount; ++i) {
    if ((i == segcount - 1u) &&
        (ranges[i - 1].start_range == 0xffff) &&
        (ranges[i - 1].end_range == 0xffff) &&
        (ranges[i].start_range == 0xffff) &&
        (ranges[i].end_range == 0xffff)) {
      // Some fonts (e.g., Germania.ttf) have multiple 0xffff terminators.
      // We'll accept them as an exception.
      Warning("multiple 0xffff terminators found");
      continue;
    }

    // Note: some Linux fonts (e.g., LucidaSansOblique.ttf, bsmi00lp.ttf) have
    // unsorted table...
    if (ranges[i].end_range <= ranges[i - 1].end_range) {
      return Error("Out of order end range (%d <= %d)", ranges[i].end_range, ranges[i-1].end_range);
    }
    if (ranges[i].start_range <= ranges[i - 1].end_range) {
      return Error("out of order start range (%d <= %d)", ranges[i].start_range, ranges[i-1].end_range);
    }

    // On many fonts, the value of {first, last}_char_index are incorrect.
    // Fix them.
    if (os2->table.first_char_index != 0xFFFF &&
        ranges[i].start_range != 0xFFFF &&
        os2->table.first_char_index > ranges[i].start_range) {
      os2->table.first_char_index = ranges[i].start_range;
    }
    if (os2->table.last_char_index != 0xFFFF &&
        ranges[i].end_range != 0xFFFF &&
        os2->table.last_char_index < ranges[i].end_range) {
      os2->table.last_char_index = ranges[i].end_range;
    }
  }

  // The last range must end at 0xffff
  if (ranges[segcount - 1].start_range != 0xffff || ranges[segcount - 1].end_range != 0xffff) {
    return Error("Final segment start and end must be 0xFFFF (0x%04X-0x%04X)",
                           ranges[segcount - 1].start_range, ranges[segcount - 1].end_range);
  }

  // A format 4 CMAP subtable is complex. To be safe we simulate a lookup of
  // each code-point defined in the table and make sure that they are all valid
  // glyphs and that we don't access anything out-of-bounds.
  for (unsigned i = 0; i < segcount; ++i) {
    for (unsigned cp = ranges[i].start_range; cp <= ranges[i].end_range; ++cp) {
      const uint16_t code_point = static_cast<uint16_t>(cp);
      if (ranges[i].id_range_offset == 0) {
        // this is explictly allowed to overflow in the spec
        const uint16_t glyph = code_point + ranges[i].id_delta;
        if (glyph >= num_glyphs) {
          return Error("Range glyph reference too high (%d > %d)", glyph, num_glyphs - 1);
        }
      } else {
        const uint16_t range_delta = code_point - ranges[i].start_range;
        // this might seem odd, but it's true. The offset is relative to the
        // location of the offset value itself.
        const uint32_t glyph_id_offset = ranges[i].id_range_offset_offset +
                                         ranges[i].id_range_offset +
                                         range_delta * 2;
        // We need to be able to access a 16-bit value from this offset
        if (glyph_id_offset + 1 >= length) {
          return Error("bad glyph id offset (%d > %ld)", glyph_id_offset, length);
        }
        uint16_t glyph;
        std::memcpy(&glyph, data + glyph_id_offset, 2);
        glyph = ots_ntohs(glyph);
        if (glyph >= num_glyphs) {
          return Error("Range glyph reference too high (%d > %d)", glyph, num_glyphs - 1);
        }
      }
    }
  }

  // We accept the table.
  // TODO(yusukes): transcode the subtable.
  if (platform == 3 && encoding == 0) {
    this->subtable_3_0_4_data = data;
    this->subtable_3_0_4_length = length;
  } else if (platform == 3 && encoding == 1) {
    this->subtable_3_1_4_data = data;
    this->subtable_3_1_4_length = length;
  } else if (platform == 0 && encoding == 3) {
    this->subtable_0_3_4_data = data;
    this->subtable_0_3_4_length = length;
  } else {
    return Error("Unknown cmap subtable type (platform=%d, encoding=%d)", platform, encoding);
  }

  return true;
}

bool OpenTypeCMAP::Parse31012(const uint8_t *data, size_t length,
                              uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Format 12 tables are simple. We parse these and fully serialise them
  // later.

  if (!subtable.Skip(8)) {
    return Error("failed to skip the first 8 bytes of format 12 subtable");
  }
  uint32_t language = 0;
  if (!subtable.ReadU32(&language)) {
    return Error("can't read format 12 subtable language");
  }
  if (language) {
    return Error("format 12 subtable language should be zero (%d)", language);
  }

  uint32_t num_groups = 0;
  if (!subtable.ReadU32(&num_groups)) {
    return Error("can't read number of format 12 subtable groups");
  }
  if (num_groups == 0 || subtable.remaining() / 12 < num_groups) {
    return Error("Bad format 12 subtable group count %d", num_groups);
  }

  std::vector<ots::OpenTypeCMAPSubtableRange> &groups
      = this->subtable_3_10_12;
  groups.resize(num_groups);

  for (unsigned i = 0; i < num_groups; ++i) {
    if (!subtable.ReadU32(&groups[i].start_range) ||
        !subtable.ReadU32(&groups[i].end_range) ||
        !subtable.ReadU32(&groups[i].start_glyph_id)) {
      return Error("can't read format 12 subtable group");
    }

    if (groups[i].start_range > kUnicodeUpperLimit ||
        groups[i].end_range > kUnicodeUpperLimit ||
        groups[i].start_glyph_id > 0xFFFF) {
      return Error("bad format 12 subtable group (startCharCode=0x%4X, endCharCode=0x%4X, startGlyphID=%d)",
                             groups[i].start_range, groups[i].end_range, groups[i].start_glyph_id);
    }

    // We assert that the glyph value is within range. Because of the range
    // limits, above, we don't need to worry about overflow.
    if (groups[i].end_range < groups[i].start_range) {
      return Error("format 12 subtable group endCharCode before startCharCode (0x%4X < 0x%4X)",
                             groups[i].end_range, groups[i].start_range);
    }
    if ((groups[i].end_range - groups[i].start_range) +
        groups[i].start_glyph_id > num_glyphs) {
      return Error("bad format 12 subtable group startGlyphID (%d)", groups[i].start_glyph_id);
    }
  }

  // the groups must be sorted by start code and may not overlap
  for (unsigned i = 1; i < num_groups; ++i) {
    if (groups[i].start_range <= groups[i - 1].start_range) {
      return Error("out of order format 12 subtable group (startCharCode=0x%4X <= startCharCode=0x%4X of previous group)",
                             groups[i].start_range, groups[i-1].start_range);
    }
    if (groups[i].start_range <= groups[i - 1].end_range) {
      return Error("overlapping format 12 subtable groups (startCharCode=0x%4X <= endCharCode=0x%4X of previous group)",
                             groups[i].start_range, groups[i-1].end_range);
    }
  }

  return true;
}

bool OpenTypeCMAP::Parse31013(const uint8_t *data, size_t length,
                              uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Format 13 tables are simple. We parse these and fully serialise them
  // later.

  if (!subtable.Skip(8)) {
    return Error("Bad cmap subtable length");
  }
  uint32_t language = 0;
  if (!subtable.ReadU32(&language)) {
    return Error("Can't read cmap subtable language");
  }
  if (language) {
    return Error("Cmap subtable language should be zero but is %d", language);
  }

  uint32_t num_groups = 0;
  if (!subtable.ReadU32(&num_groups)) {
    return Error("Can't read number of groups in a cmap subtable");
  }

  // We limit the number of groups in the same way as in 3.10.12 tables. See
  // the comment there in
  if (num_groups == 0 || subtable.remaining() / 12 < num_groups) {
    return Error("Bad format 13 subtable group count %d", num_groups);
  }

  std::vector<ots::OpenTypeCMAPSubtableRange> &groups = this->subtable_3_10_13;
  groups.resize(num_groups);

  for (unsigned i = 0; i < num_groups; ++i) {
    if (!subtable.ReadU32(&groups[i].start_range) ||
        !subtable.ReadU32(&groups[i].end_range) ||
        !subtable.ReadU32(&groups[i].start_glyph_id)) {
      return Error("Can't read subrange structure in a cmap subtable");
    }

    // We conservatively limit all of the values to protect some parsers from
    // overflows
    if (groups[i].start_range > kUnicodeUpperLimit ||
        groups[i].end_range > kUnicodeUpperLimit ||
        groups[i].start_glyph_id > 0xFFFF) {
      return Error("Bad subrange with start_range=%d, end_range=%d, start_glyph_id=%d", groups[i].start_range, groups[i].end_range, groups[i].start_glyph_id);
    }

    if (groups[i].start_glyph_id >= num_glyphs) {
      return Error("Subrange starting glyph id too high (%d > %d)", groups[i].start_glyph_id, num_glyphs);
    }
  }

  // the groups must be sorted by start code and may not overlap
  for (unsigned i = 1; i < num_groups; ++i) {
    if (groups[i].start_range <= groups[i - 1].start_range) {
      return Error("Overlapping subrange starts (%d >= %d)", groups[i]. start_range, groups[i-1].start_range);
    }
    if (groups[i].start_range <= groups[i - 1].end_range) {
      return Error("Overlapping subranges (%d <= %d)", groups[i].start_range, groups[i-1].end_range);
    }
  }

  return true;
}

bool OpenTypeCMAP::Parse0514(const uint8_t *data, size_t length) {
  // Unicode Variation Selector table
  ots::Buffer subtable(data, length);

  // Format 14 tables are simple. We parse these and fully serialise them
  // later.

  // Skip format (USHORT) and length (ULONG)
  if (!subtable.Skip(6)) {
    return Error("Can't read start of cmap subtable");
  }

  uint32_t num_records = 0;
  if (!subtable.ReadU32(&num_records)) {
    return Error("Can't read number of records in cmap subtable");
  }
  if (num_records == 0 || num_records > kMaxCMAPSelectorRecords) {
    return Error("Bad format 14 subtable records count %d", num_records);
  }

  std::vector<ots::OpenTypeCMAPSubtableVSRecord>& records
      = this->subtable_0_5_14;
  records.resize(num_records);

  for (unsigned i = 0; i < num_records; ++i) {
    if (!subtable.ReadU24(&records[i].var_selector) ||
        !subtable.ReadU32(&records[i].default_offset) ||
        !subtable.ReadU32(&records[i].non_default_offset)) {
      return Error("Can't read record structure of record %d in cmap subtale", i);
    }
    // Checks the value of variation selector
    if (!((records[i].var_selector >= kMongolianVSStart &&
           records[i].var_selector <= kMongolianVSEnd) ||
          (records[i].var_selector >= kVSStart &&
           records[i].var_selector <= kVSEnd) ||
          (records[i].var_selector >= kIVSStart &&
           records[i].var_selector <= kIVSEnd))) {
      return Error("Bad record variation selector (%04X) in record %i", records[i].var_selector, i);
    }
    if (i > 0 &&
        records[i-1].var_selector >= records[i].var_selector) {
      return Error("Out of order variation selector (%04X >= %04X) in record %d", records[i-1].var_selector, records[i].var_selector, i);
    }

    // Checks offsets
    if (!records[i].default_offset && !records[i].non_default_offset) {
      return Error("No default aoffset in variation selector record %d", i);
    }
    if (records[i].default_offset &&
        records[i].default_offset >= length) {
      return Error("Default offset too high (%d >= %ld) in record %d", records[i].default_offset, length, i);
    }
    if (records[i].non_default_offset &&
        records[i].non_default_offset >= length) {
      return Error("Non default offset too high (%d >= %ld) in record %d", records[i].non_default_offset, length, i);
    }
  }

  for (unsigned i = 0; i < num_records; ++i) {
    // Checks default UVS table
    if (records[i].default_offset) {
      subtable.set_offset(records[i].default_offset);
      uint32_t num_ranges = 0;
      if (!subtable.ReadU32(&num_ranges)) {
        return Error("Can't read number of ranges in record %d", i);
      }
      if (num_ranges == 0 || subtable.remaining() / 4 < num_ranges) {
        return Error("Bad number of ranges (%d) in record %d", num_ranges, i);
      }

      uint32_t last_unicode_value = 0;
      std::vector<ots::OpenTypeCMAPSubtableVSRange>& ranges
          = records[i].ranges;
      ranges.resize(num_ranges);

      for (unsigned j = 0; j < num_ranges; ++j) {
        if (!subtable.ReadU24(&ranges[j].unicode_value) ||
            !subtable.ReadU8(&ranges[j].additional_count)) {
          return Error("Can't read range info in variation selector record %d", i);
        }
        const uint32_t check_value =
            ranges[j].unicode_value + ranges[j].additional_count;
        if (ranges[j].unicode_value == 0 ||
            ranges[j].unicode_value > kUnicodeUpperLimit ||
            check_value > kUVSUpperLimit ||
            (last_unicode_value &&
             ranges[j].unicode_value <= last_unicode_value)) {
          return Error("Bad Unicode value *%04X) in variation selector range %d record %d", ranges[j].unicode_value, j, i);
        }
        last_unicode_value = check_value;
      }
    }

    // Checks non default UVS table
    if (records[i].non_default_offset) {
      subtable.set_offset(records[i].non_default_offset);
      uint32_t num_mappings = 0;
      if (!subtable.ReadU32(&num_mappings)) {
        return Error("Can't read number of mappings in variation selector record %d", i);
      }
      if (num_mappings == 0 || subtable.remaining() / 5 < num_mappings) {
        return Error("Bad number of mappings (%d) in variation selector record %d", num_mappings, i);
      }

      uint32_t last_unicode_value = 0;
      std::vector<ots::OpenTypeCMAPSubtableVSMapping>& mappings
          = records[i].mappings;
      mappings.resize(num_mappings);

      for (unsigned j = 0; j < num_mappings; ++j) {
        if (!subtable.ReadU24(&mappings[j].unicode_value) ||
            !subtable.ReadU16(&mappings[j].glyph_id)) {
          return Error("Can't read mapping %d in variation selector record %d", j, i);
        }
        if (mappings[j].glyph_id == 0 || mappings[j].unicode_value == 0) {
          return Error("Bad mapping (%04X -> %d) in mapping %d of variation selector %d", mappings[j].unicode_value, mappings[j].glyph_id, j, i);
        }
        if (mappings[j].unicode_value > kUnicodeUpperLimit) {
          return Error("Invalid Unicode value (%04X > %04X) in mapping %d of variation selector %d", mappings[j].unicode_value, kUnicodeUpperLimit, j, i);
        }
        if (last_unicode_value &&
            mappings[j].unicode_value <= last_unicode_value) {
          return Error("Out of order Unicode value (%04X <= %04X) in mapping %d of variation selector %d", mappings[j].unicode_value, last_unicode_value, j, i);
        }
        last_unicode_value = mappings[j].unicode_value;
      }
    }
  }

  if (subtable.offset() != length) {
    return Error("Bad subtable offset (%ld != %ld)", subtable.offset(), length);
  }
  this->subtable_0_5_14_length = subtable.offset();
  return true;
}

bool OpenTypeCMAP::Parse100(const uint8_t *data, size_t length) {
  // Mac Roman table
  ots::Buffer subtable(data, length);

  if (!subtable.Skip(4)) {
    return Error("Bad cmap subtable");
  }
  uint16_t language = 0;
  if (!subtable.ReadU16(&language)) {
    return Error("Can't read language in cmap subtable");
  }
  if (language) {
    // simsun.ttf has non-zero language id.
    Warning("language id should be zero: %u", language);
  }

  this->subtable_1_0_0.reserve(kFormat0ArraySize);
  for (size_t i = 0; i < kFormat0ArraySize; ++i) {
    uint8_t glyph_id = 0;
    if (!subtable.ReadU8(&glyph_id)) {
      return Error("Can't read glyph id at array[%ld] in cmap subtable", i);
    }
    this->subtable_1_0_0.push_back(glyph_id);
  }

  return true;
}

bool OpenTypeCMAP::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  uint16_t version = 0;
  uint16_t num_tables = 0;
  if (!table.ReadU16(&version) ||
      !table.ReadU16(&num_tables)) {
    return Error("Can't read structure of cmap");
  }

  if (version != 0) {
    return Error("Non zero cmap version (%d)", version);
  }
  if (!num_tables) {
    return Error("No subtables in cmap!");
  }

  std::vector<CMAPSubtableHeader> subtable_headers;

  // read the subtable headers
  subtable_headers.reserve(num_tables);
  for (unsigned i = 0; i < num_tables; ++i) {
    CMAPSubtableHeader subt;

    if (!table.ReadU16(&subt.platform) ||
        !table.ReadU16(&subt.encoding) ||
        !table.ReadU32(&subt.offset)) {
      return Error("Can't read subtable information cmap subtable %d", i);
    }

    subtable_headers.push_back(subt);
  }

  const size_t data_offset = table.offset();

  // make sure that all the offsets are valid.
  for (unsigned i = 0; i < num_tables; ++i) {
    if (subtable_headers[i].offset > 1024 * 1024 * 1024) {
      return Error("Bad subtable offset in cmap subtable %d", i);
    }
    if (subtable_headers[i].offset < data_offset ||
        subtable_headers[i].offset >= length) {
      return Error("Bad subtable offset (%d) in cmap subtable %d", subtable_headers[i].offset, i);
    }
  }

  // the format of the table is the first couple of bytes in the table. The
  // length of the table is stored in a format-specific way.
  for (unsigned i = 0; i < num_tables; ++i) {
    table.set_offset(subtable_headers[i].offset);
    if (!table.ReadU16(&subtable_headers[i].format)) {
      return Error("Can't read cmap subtable header format %d", i);
    }

    uint16_t len = 0;
    uint16_t lang = 0;
    switch (subtable_headers[i].format) {
      case 0:
      case 4:
        if (!table.ReadU16(&len)) {
          return Error("Can't read cmap subtable %d length", i);
        }
        if (!table.ReadU16(&lang)) {
          return Error("Can't read cmap subtable %d language", i);
        }
        subtable_headers[i].length = len;
        subtable_headers[i].language = lang;
        break;
      case 12:
      case 13:
        if (!table.Skip(2)) {
          return Error("Bad cmap subtable %d structure", i);
        }
        if (!table.ReadU32(&subtable_headers[i].length)) {
          return Error("Can read cmap subtable %d length", i);
        }
        if (!table.ReadU32(&subtable_headers[i].language)) {
          return Error("Can't read cmap subtable %d language", i);
        }
        break;
      case 14:
        if (!table.ReadU32(&subtable_headers[i].length)) {
          return Error("Can't read cmap subtable %d length", i);
        }
        subtable_headers[i].language = 0;
        break;
      default:
        subtable_headers[i].length = 0;
        subtable_headers[i].language = 0;
        break;
    }
  }

  // check if the table is sorted first by platform ID, then by encoding ID.
  for (unsigned i = 1; i < num_tables; ++i) {
    if (subtable_headers[i - 1].platform > subtable_headers[i].platform ||
        (subtable_headers[i - 1].platform == subtable_headers[i].platform &&
         (subtable_headers[i - 1].encoding > subtable_headers[i].encoding ||
          (subtable_headers[i - 1].encoding == subtable_headers[i].encoding &&
           subtable_headers[i - 1].language > subtable_headers[i].language))))
      Warning("subtable %d with platform ID %d, encoding ID %d, language ID %d "
                  "following subtable with platform ID %d, encoding ID %d, language ID %d",
                  i,
                  subtable_headers[i].platform,
                  subtable_headers[i].encoding,
                  subtable_headers[i].language,
                  subtable_headers[i - 1].platform,
                  subtable_headers[i - 1].encoding,
                  subtable_headers[i - 1].language);
  }

  // Now, verify that all the lengths are sane
  for (unsigned i = 0; i < num_tables; ++i) {
    if (!subtable_headers[i].length) continue;
    if (subtable_headers[i].length > 1024 * 1024 * 1024) {
      return Error("Bad cmap subtable %d length", i);
    }
    // We know that both the offset and length are < 1GB, so the following
    // addition doesn't overflow
    const uint32_t end_byte
        = subtable_headers[i].offset + subtable_headers[i].length;
    if (end_byte > length) {
      return Error("Over long cmap subtable %d @ %d for %d", i, subtable_headers[i].offset, subtable_headers[i].length);
    }
  }

  // check that the cmap subtables are not overlapping.
  std::set<std::pair<uint32_t, uint32_t> > uniq_checker;
  std::vector<std::pair<uint32_t, uint8_t> > overlap_checker;
  for (unsigned i = 0; i < num_tables; ++i) {
    const uint32_t end_byte
        = subtable_headers[i].offset + subtable_headers[i].length;

    if (!uniq_checker.insert(std::make_pair(subtable_headers[i].offset,
                                            end_byte)).second) {
      // Sometimes Unicode table and MS table share exactly the same data.
      // We'll allow this.
      continue;
    }
    overlap_checker.push_back(
        std::make_pair(subtable_headers[i].offset,
                       static_cast<uint8_t>(1) /* start */));
    overlap_checker.push_back(
        std::make_pair(end_byte, static_cast<uint8_t>(0) /* end */));
  }
  std::sort(overlap_checker.begin(), overlap_checker.end());
  int overlap_count = 0;
  for (unsigned i = 0; i < overlap_checker.size(); ++i) {
    overlap_count += (overlap_checker[i].second ? 1 : -1);
    if (overlap_count > 1) {
      return Error("Excessive overlap count %d", overlap_count);
    }
  }

  // we grab the number of glyphs in the file from the maxp table to make sure
  // that the character map isn't referencing anything beyound this range.
  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("No maxp table in font! Needed by cmap.");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;

  // We only support a subset of the possible character map tables. Microsoft
  // 'strongly recommends' that everyone supports the Unicode BMP table with
  // the UCS-4 table for non-BMP glyphs. We'll pass the following subtables:
  //   Platform ID   Encoding ID  Format
  //   0             0            4       (Unicode Default)
  //   0             1            4       (Unicode 1.1)
  //   0             3            4       (Unicode BMP)
  //   0             3            12      (Unicode UCS-4)
  //   0             5            14      (Unicode Variation Sequences)
  //   1             0            0       (Mac Roman)
  //   3             0            4       (MS Symbol)
  //   3             1            4       (MS Unicode BMP)
  //   3             10           12      (MS Unicode UCS-4)
  //   3             10           13      (MS UCS-4 Fallback mapping)
  //
  // Note:
  //  * 0-0-4 and 0-1-4 tables are (usually) written as a 3-1-4 table. If 3-1-4 table
  //    also exists, the 0-0-4 or 0-1-4 tables are ignored.
  //  * Unlike 0-0-4 table, 0-3-4 table is written as a 0-3-4 table.
  //    Some fonts which include 0-5-14 table seems to be required 0-3-4
  //    table. The 0-3-4 table will be wriiten even if 3-1-4 table also exists.
  //  * 0-3-12 table is written as a 3-10-12 table. If 3-10-12 table also
  //    exists, the 0-3-12 table is ignored.
  //

  for (unsigned i = 0; i < num_tables; ++i) {
    if (subtable_headers[i].platform == 0) {
      // Unicode platform

      if ((subtable_headers[i].encoding == 0 || subtable_headers[i].encoding == 1) &&
          (subtable_headers[i].format == 4)) {
        // parse and output the 0-0-4 and 0-1-4 tables as 3-1-4 table. Sometimes the 0-0-4
        // table actually points to MS symbol data and thus should be parsed as
        // 3-0-4 table (e.g., marqueem.ttf and quixotic.ttf). This error will be
        // recovered in ots_cmap_serialise().
        if (!ParseFormat4(3, 1, data + subtable_headers[i].offset,
                      subtable_headers[i].length, num_glyphs)) {
          return Error("Failed to parse format 4 cmap subtable %d", i);
        }
      } else if ((subtable_headers[i].encoding == 3) &&
                 (subtable_headers[i].format == 4)) {
        // parse and output the 0-3-4 table as 0-3-4 table.
        if (!ParseFormat4(0, 3, data + subtable_headers[i].offset,
                      subtable_headers[i].length, num_glyphs)) {
          return Error("Failed to parse format 4 cmap subtable %d", i);
        }
      } else if ((subtable_headers[i].encoding == 3 ||
                  subtable_headers[i].encoding == 4) &&
                 (subtable_headers[i].format == 12)) {
        // parse and output the 0-3-12 or 0-4-12 tables as 3-10-12 table.
        if (!Parse31012(data + subtable_headers[i].offset,
                        subtable_headers[i].length, num_glyphs)) {
          return Error("Failed to parse format 12 cmap subtable %d", i);
        }
      } else if ((subtable_headers[i].encoding == 5) &&
                 (subtable_headers[i].format == 14)) {
        if (!Parse0514(data + subtable_headers[i].offset,
                       subtable_headers[i].length)) {
          return Error("Failed to parse format 14 cmap subtable %d", i);
        }
      }
    } else if (subtable_headers[i].platform == 1) {
      // Mac platform

      if ((subtable_headers[i].encoding == 0) &&
          (subtable_headers[i].format == 0)) {
        // parse and output the 1-0-0 table.
        if (!Parse100(data + subtable_headers[i].offset,
                      subtable_headers[i].length)) {
          return OTS_FAILURE();
        }
      }
    } else if (subtable_headers[i].platform == 3) {
      // MS platform

      switch (subtable_headers[i].encoding) {
        case 0:
        case 1:
          if (subtable_headers[i].format == 4) {
            // parse 3-0-4 or 3-1-4 table.
            if (!ParseFormat4(subtable_headers[i].platform,
                          subtable_headers[i].encoding,
                          data + subtable_headers[i].offset,
                          subtable_headers[i].length, num_glyphs)) {
              return OTS_FAILURE();
            }
          }
          break;
        case 10:
          if (subtable_headers[i].format == 12) {
            this->subtable_3_10_12.clear();
            if (!Parse31012(data + subtable_headers[i].offset,
                            subtable_headers[i].length, num_glyphs)) {
              return OTS_FAILURE();
            }
          } else if (subtable_headers[i].format == 13) {
            this->subtable_3_10_13.clear();
            if (!Parse31013(data + subtable_headers[i].offset,
                            subtable_headers[i].length, num_glyphs)) {
              return OTS_FAILURE();
            }
          }
          break;
      }
    }
  }

  return true;
}

bool OpenTypeCMAP::Serialize(OTSStream *out) {
  const bool have_034 = this->subtable_0_3_4_data != NULL;
  const bool have_0514 = this->subtable_0_5_14.size() != 0;
  const bool have_100 = this->subtable_1_0_0.size() != 0;
  const bool have_304 = this->subtable_3_0_4_data != NULL;
  // MS Symbol and MS Unicode tables should not co-exist.
  // See the comment above in 0-0-4 parser.
  const bool have_314 = (!have_304) && this->subtable_3_1_4_data;
  const bool have_31012 = this->subtable_3_10_12.size() != 0;
  const bool have_31013 = this->subtable_3_10_13.size() != 0;
  const uint16_t num_subtables = static_cast<uint16_t>(have_034) +
                                 static_cast<uint16_t>(have_0514) +
                                 static_cast<uint16_t>(have_100) +
                                 static_cast<uint16_t>(have_304) +
                                 static_cast<uint16_t>(have_314) +
                                 static_cast<uint16_t>(have_31012) +
                                 static_cast<uint16_t>(have_31013);
  const off_t table_start = out->Tell();

  // Some fonts don't have 3-0-4 MS Symbol nor 3-1-4 Unicode BMP tables
  // (e.g., old fonts for Mac). We don't support them.
  if (!have_304 && !have_314 && !have_034 && !have_31012 && !have_31013) {
    return Error("no supported subtables were found");
  }

  if (!out->WriteU16(0) ||
      !out->WriteU16(num_subtables)) {
    return OTS_FAILURE();
  }

  const off_t record_offset = out->Tell();
  if (!out->Pad(num_subtables * 8)) {
    return OTS_FAILURE();
  }

  const off_t offset_034 = out->Tell();
  if (have_034) {
    if (!out->Write(this->subtable_0_3_4_data,
                    this->subtable_0_3_4_length)) {
      return OTS_FAILURE();
    }
  }

  const off_t offset_0514 = out->Tell();
  if (have_0514) {
    const std::vector<ots::OpenTypeCMAPSubtableVSRecord> &records
        = this->subtable_0_5_14;
    const unsigned num_records = records.size();
    if (!out->WriteU16(14) ||
        !out->WriteU32(this->subtable_0_5_14_length) ||
        !out->WriteU32(num_records)) {
      return OTS_FAILURE();
    }
    for (unsigned i = 0; i < num_records; ++i) {
      if (!out->WriteU24(records[i].var_selector) ||
          !out->WriteU32(records[i].default_offset) ||
          !out->WriteU32(records[i].non_default_offset)) {
        return OTS_FAILURE();
      }
    }
    for (unsigned i = 0; i < num_records; ++i) {
      if (records[i].default_offset) {
        const std::vector<ots::OpenTypeCMAPSubtableVSRange> &ranges
            = records[i].ranges;
        const unsigned num_ranges = ranges.size();
        if (!out->Seek(records[i].default_offset + offset_0514) ||
            !out->WriteU32(num_ranges)) {
          return OTS_FAILURE();
        }
        for (unsigned j = 0; j < num_ranges; ++j) {
          if (!out->WriteU24(ranges[j].unicode_value) ||
              !out->WriteU8(ranges[j].additional_count)) {
            return OTS_FAILURE();
          }
        }
      }
      if (records[i].non_default_offset) {
        const std::vector<ots::OpenTypeCMAPSubtableVSMapping> &mappings
            = records[i].mappings;
        const unsigned num_mappings = mappings.size();
        if (!out->Seek(records[i].non_default_offset + offset_0514) ||
            !out->WriteU32(num_mappings)) {
          return OTS_FAILURE();
        }
        for (unsigned j = 0; j < num_mappings; ++j) {
          if (!out->WriteU24(mappings[j].unicode_value) ||
              !out->WriteU16(mappings[j].glyph_id)) {
            return OTS_FAILURE();
          }
        }
      }
    }
  }

  const off_t offset_100 = out->Tell();
  if (have_100) {
    if (!out->WriteU16(0) ||  // format
        !out->WriteU16(6 + kFormat0ArraySize) ||  // length
        !out->WriteU16(0)) {  // language
      return OTS_FAILURE();
    }
    if (!out->Write(&(this->subtable_1_0_0[0]), kFormat0ArraySize)) {
      return OTS_FAILURE();
    }
  }

  const off_t offset_304 = out->Tell();
  if (have_304) {
    if (!out->Write(this->subtable_3_0_4_data,
                    this->subtable_3_0_4_length)) {
      return OTS_FAILURE();
    }
  }

  const off_t offset_314 = out->Tell();
  if (have_314) {
    if (!out->Write(this->subtable_3_1_4_data,
                    this->subtable_3_1_4_length)) {
      return OTS_FAILURE();
    }
  }

  const off_t offset_31012 = out->Tell();
  if (have_31012) {
    std::vector<OpenTypeCMAPSubtableRange> &groups
        = this->subtable_3_10_12;
    const unsigned num_groups = groups.size();
    if (!out->WriteU16(12) ||
        !out->WriteU16(0) ||
        !out->WriteU32(num_groups * 12 + 16) ||
        !out->WriteU32(0) ||
        !out->WriteU32(num_groups)) {
      return OTS_FAILURE();
    }

    for (unsigned i = 0; i < num_groups; ++i) {
      if (!out->WriteU32(groups[i].start_range) ||
          !out->WriteU32(groups[i].end_range) ||
          !out->WriteU32(groups[i].start_glyph_id)) {
        return OTS_FAILURE();
      }
    }
  }

  const off_t offset_31013 = out->Tell();
  if (have_31013) {
    std::vector<OpenTypeCMAPSubtableRange> &groups
        = this->subtable_3_10_13;
    const unsigned num_groups = groups.size();
    if (!out->WriteU16(13) ||
        !out->WriteU16(0) ||
        !out->WriteU32(num_groups * 12 + 16) ||
        !out->WriteU32(0) ||
        !out->WriteU32(num_groups)) {
      return OTS_FAILURE();
    }

    for (unsigned i = 0; i < num_groups; ++i) {
      if (!out->WriteU32(groups[i].start_range) ||
          !out->WriteU32(groups[i].end_range) ||
          !out->WriteU32(groups[i].start_glyph_id)) {
        return OTS_FAILURE();
      }
    }
  }

  const off_t table_end = out->Tell();

  // Now seek back and write the table of offsets
  if (!out->Seek(record_offset)) {
    return OTS_FAILURE();
  }

  if (have_034) {
    if (!out->WriteU16(0) ||
        !out->WriteU16(3) ||
        !out->WriteU32(offset_034 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_0514) {
    if (!out->WriteU16(0) ||
        !out->WriteU16(5) ||
        !out->WriteU32(offset_0514 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_100) {
    if (!out->WriteU16(1) ||
        !out->WriteU16(0) ||
        !out->WriteU32(offset_100 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_304) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(0) ||
        !out->WriteU32(offset_304 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_314) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(1) ||
        !out->WriteU32(offset_314 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_31012) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(10) ||
        !out->WriteU32(offset_31012 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (have_31013) {
    if (!out->WriteU16(3) ||
        !out->WriteU16(10) ||
        !out->WriteU32(offset_31013 - table_start)) {
      return OTS_FAILURE();
    }
  }

  if (!out->Seek(table_end)) {
    return OTS_FAILURE();
  }

  return true;
}

}  // namespace ots
