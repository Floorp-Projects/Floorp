// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ots.h"

#include <sys/types.h>
#include <zlib.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <vector>

#include "../RLBoxWOFF2Host.h"

// The OpenType Font File
// http://www.microsoft.com/typography/otspec/otff.htm

#include "avar.h"
#include "cff.h"
#include "cmap.h"
#include "cvar.h"
#include "cvt.h"
#include "fpgm.h"
#include "fvar.h"
#include "gasp.h"
#include "gdef.h"
#include "glyf.h"
#include "gpos.h"
#include "gsub.h"
#include "gvar.h"
#include "hdmx.h"
#include "head.h"
#include "hhea.h"
#include "hmtx.h"
#include "hvar.h"
#include "kern.h"
#include "loca.h"
#include "ltsh.h"
#include "math_.h"
#include "maxp.h"
#include "mvar.h"
#include "name.h"
#include "os2.h"
#include "ots.h"
#include "post.h"
#include "prep.h"
#include "stat.h"
#include "vdmx.h"
#include "vhea.h"
#include "vmtx.h"
#include "vorg.h"
#include "vvar.h"

// Graphite tables
#ifdef OTS_GRAPHITE
#include "feat.h"
#include "glat.h"
#include "gloc.h"
#include "sile.h"
#include "silf.h"
#include "sill.h"
#endif

namespace ots {

struct Arena {
 public:
  ~Arena() {
    for (auto& hunk : hunks_) {
      delete[] hunk;
    }
  }

  uint8_t* Allocate(size_t length) {
    uint8_t* p = new uint8_t[length];
    hunks_.push_back(p);
    return p;
  }

 private:
  std::vector<uint8_t*> hunks_;
};

bool CheckTag(uint32_t tag_value) {
  for (unsigned i = 0; i < 4; ++i) {
    const uint32_t check = tag_value & 0xff;
    if (check < 32 || check > 126) {
      return false;  // non-ASCII character found.
    }
    tag_value >>= 8;
  }
  return true;
}

}; // namespace ots

namespace {

#define OTS_MSG_TAG_(level,otf_,msg_,tag_) \
  (OTS_MESSAGE_(level,otf_,"%c%c%c%c: %s", OTS_UNTAG(tag_), msg_), false)

// Generate a message with or without a table tag, when 'header' is the FontFile pointer
#define OTS_FAILURE_MSG_TAG(msg_,tag_) OTS_MSG_TAG_(0, header, msg_, tag_)
#define OTS_FAILURE_MSG_HDR(...)       OTS_FAILURE_MSG_(header, __VA_ARGS__)
#define OTS_WARNING_MSG_HDR(...)       OTS_WARNING_MSG_(header, __VA_ARGS__)


const struct {
  uint32_t tag;
  bool required;
} supported_tables[] = {
  { OTS_TAG_MAXP, true },
  { OTS_TAG_HEAD, true },
  { OTS_TAG_OS2,  true },
  { OTS_TAG_CMAP, true },
  { OTS_TAG_HHEA, true },
  { OTS_TAG_HMTX, true },
  { OTS_TAG_NAME, true },
  { OTS_TAG_POST, true },
  { OTS_TAG_LOCA, false },
  { OTS_TAG_GLYF, false },
  { OTS_TAG_CFF,  false },
  { OTS_TAG_VDMX, false },
  { OTS_TAG_HDMX, false },
  { OTS_TAG_GASP, false },
  { OTS_TAG_CVT,  false },
  { OTS_TAG_FPGM, false },
  { OTS_TAG_PREP, false },
  { OTS_TAG_LTSH, false },
  { OTS_TAG_VORG, false },
  { OTS_TAG_KERN, false },
  // We need to parse fvar table before other tables that may need to know
  // the number of variation axes (if any)
  { OTS_TAG_FVAR, false },
  { OTS_TAG_AVAR, false },
  { OTS_TAG_CVAR, false },
  { OTS_TAG_GVAR, false },
  { OTS_TAG_HVAR, false },
  { OTS_TAG_MVAR, false },
  { OTS_TAG_STAT, false },
  { OTS_TAG_VVAR, false },
  { OTS_TAG_CFF2, false },
  // We need to parse GDEF table in advance of parsing GSUB/GPOS tables
  // because they could refer GDEF table.
  { OTS_TAG_GDEF, false },
  { OTS_TAG_GPOS, false },
  { OTS_TAG_GSUB, false },
  { OTS_TAG_VHEA, false },
  { OTS_TAG_VMTX, false },
  { OTS_TAG_MATH, false },
  // Graphite tables
#ifdef OTS_GRAPHITE
  { OTS_TAG_GLOC, false },
  { OTS_TAG_GLAT, false },
  { OTS_TAG_FEAT, false },
  { OTS_TAG_SILF, false },
  { OTS_TAG_SILE, false },
  { OTS_TAG_SILL, false },
#endif
  { 0, false },
};

bool ValidateVersionTag(ots::Font *font) {
  switch (font->version) {
    case 0x000010000:
    case OTS_TAG('O','T','T','O'):
      return true;
    case OTS_TAG('t','r','u','e'):
      font->version = 0x000010000;
      return true;
    default:
      return false;
  }
}

bool ProcessGeneric(ots::FontFile *header,
                    ots::Font *font,
                    uint32_t signature,
                    ots::OTSStream *output,
                    const uint8_t *data, size_t length,
                    const std::vector<ots::TableEntry>& tables,
                    ots::Buffer& file);

bool ProcessTTF(ots::FontFile *header,
                ots::Font *font,
                ots::OTSStream *output, const uint8_t *data, size_t length,
                uint32_t offset = 0) {
  ots::Buffer file(data + offset, length - offset);

  if (offset > length) {
    return OTS_FAILURE_MSG_HDR("offset beyond end of file");
  }

  // we disallow all files > 1GB in size for sanity.
  if (length > 1024 * 1024 * 1024) {
    return OTS_FAILURE_MSG_HDR("file exceeds 1GB");
  }

  if (!file.ReadU32(&font->version)) {
    return OTS_FAILURE_MSG_HDR("error reading sfntVersion");
  }
  if (!ValidateVersionTag(font)) {
    return OTS_FAILURE_MSG_HDR("invalid sfntVersion: %d", font->version);
  }

  if (!file.ReadU16(&font->num_tables) ||
      !file.ReadU16(&font->search_range) ||
      !file.ReadU16(&font->entry_selector) ||
      !file.ReadU16(&font->range_shift)) {
    return OTS_FAILURE_MSG_HDR("error reading table directory search header");
  }

  // search_range is (Maximum power of 2 <= numTables) x 16. Thus, to avoid
  // overflow num_tables is, at most, 2^16 / 16 = 2^12
  if (font->num_tables >= 4096 || font->num_tables < 1) {
    return OTS_FAILURE_MSG_HDR("excessive (or zero) number of tables");
  }

  unsigned max_pow2 = 0;
  while (1u << (max_pow2 + 1) <= font->num_tables) {
    max_pow2++;
  }
  const uint16_t expected_search_range = (1u << max_pow2) << 4;

  // Don't call ots_failure() here since ~25% of fonts (250+ fonts) in
  // http://www.princexml.com/fonts/ have unexpected search_range value.
  if (font->search_range != expected_search_range) {
    OTS_WARNING_MSG_HDR("bad table directory searchRange");
    font->search_range = expected_search_range;  // Fix the value.
  }

  // entry_selector is Log2(maximum power of 2 <= numTables)
  if (font->entry_selector != max_pow2) {
    OTS_WARNING_MSG_HDR("bad table directory entrySelector");
    font->entry_selector = max_pow2;  // Fix the value.
  }

  // range_shift is NumTables x 16-searchRange. We know that 16*num_tables
  // doesn't over flow because we range checked it above. Also, we know that
  // it's > font->search_range by construction of search_range.
  const uint16_t expected_range_shift =
      16 * font->num_tables - font->search_range;
  if (font->range_shift != expected_range_shift) {
    OTS_WARNING_MSG_HDR("bad table directory rangeShift");
    font->range_shift = expected_range_shift;  // the same as above.
  }

  // Next up is the list of tables.
  std::vector<ots::TableEntry> tables;

  for (unsigned i = 0; i < font->num_tables; ++i) {
    ots::TableEntry table;
    if (!file.ReadU32(&table.tag) ||
        !file.ReadU32(&table.chksum) ||
        !file.ReadU32(&table.offset) ||
        !file.ReadU32(&table.length)) {
      return OTS_FAILURE_MSG_HDR("error reading table directory");
    }

    table.uncompressed_length = table.length;
    tables.push_back(table);
  }

  return ProcessGeneric(header, font, font->version, output, data, length,
                        tables, file);
}

bool ProcessTTC(ots::FontFile *header,
                ots::OTSStream *output,
                const uint8_t *data,
                size_t length,
                uint32_t index) {
  ots::Buffer file(data, length);

  // we disallow all files > 1GB in size for sanity.
  if (length > 1024 * 1024 * 1024) {
    return OTS_FAILURE_MSG_HDR("file exceeds 1GB");
  }

  uint32_t ttc_tag;
  if (!file.ReadU32(&ttc_tag)) {
    return OTS_FAILURE_MSG_HDR("Error reading TTC tag");
  }
  if (ttc_tag != OTS_TAG('t','t','c','f')) {
    return OTS_FAILURE_MSG_HDR("Invalid TTC tag");
  }

  uint32_t ttc_version;
  if (!file.ReadU32(&ttc_version)) {
    return OTS_FAILURE_MSG_HDR("Error reading TTC version");
  }
  if (ttc_version != 0x00010000 && ttc_version != 0x00020000) {
    return OTS_FAILURE_MSG_HDR("Invalid TTC version");
  }

  uint32_t num_fonts;
  if (!file.ReadU32(&num_fonts)) {
    return OTS_FAILURE_MSG_HDR("Error reading number of TTC fonts");
  }
  // Limit the allowed number of subfonts to have same memory allocation.
  if (num_fonts > 0x10000) {
    return OTS_FAILURE_MSG_HDR("Too many fonts in TTC");
  }

  std::vector<uint32_t> offsets(num_fonts);
  for (unsigned i = 0; i < num_fonts; i++) {
    if (!file.ReadU32(&offsets[i])) {
      return OTS_FAILURE_MSG_HDR("Error reading offset to OffsetTable");
    }
  }

  if (ttc_version == 0x00020000) {
    // We don't care about these fields of the header:
    // uint32_t dsig_tag, dsig_length, dsig_offset
    if (!file.Skip(3 * 4)) {
      return OTS_FAILURE_MSG_HDR("Error reading DSIG offset and length in TTC font");
    }
  }

  if (index == static_cast<uint32_t>(-1)) {
    if (!output->WriteU32(ttc_tag) ||
        !output->WriteU32(0x00010000) ||
        !output->WriteU32(num_fonts) ||
        !output->Seek((3 + num_fonts) * 4)) {
      return OTS_FAILURE_MSG_HDR("Error writing output");
    }

    // Keep references to the fonts processed in the loop below, as we need
    // them for reused tables.
    std::vector<ots::Font> fonts(num_fonts, ots::Font(header));

    for (unsigned i = 0; i < num_fonts; i++) {
      uint32_t out_offset = output->Tell();
      if (!output->Seek((3 + i) * 4) ||
          !output->WriteU32(out_offset) ||
          !output->Seek(out_offset)) {
        return OTS_FAILURE_MSG_HDR("Error writing output");
      }
      if (!ProcessTTF(header, &fonts[i], output, data, length, offsets[i])) {
        return false;
      }
    }

    return true;
  } else {
    if (index >= num_fonts) {
      return OTS_FAILURE_MSG_HDR("Requested font index is bigger than the number of fonts in the TTC file");
    }

    ots::Font font(header);
    return ProcessTTF(header, &font, output, data, length, offsets[index]);
  }
}

bool ProcessWOFF(ots::FontFile *header,
                 ots::Font *font,
                 ots::OTSStream *output, const uint8_t *data, size_t length) {
  ots::Buffer file(data, length);

  // we disallow all files > 1GB in size for sanity.
  if (length > 1024 * 1024 * 1024) {
    return OTS_FAILURE_MSG_HDR("file exceeds 1GB");
  }

  uint32_t woff_tag;
  if (!file.ReadU32(&woff_tag)) {
    return OTS_FAILURE_MSG_HDR("error reading WOFF marker");
  }

  if (woff_tag != OTS_TAG('w','O','F','F')) {
    return OTS_FAILURE_MSG_HDR("invalid WOFF marker");
  }

  if (!file.ReadU32(&font->version)) {
    return OTS_FAILURE_MSG_HDR("error reading sfntVersion");
  }
  if (!ValidateVersionTag(font)) {
    return OTS_FAILURE_MSG_HDR("invalid sfntVersion: %d", font->version);
  }

  uint32_t reported_length;
  if (!file.ReadU32(&reported_length) || length != reported_length) {
    return OTS_FAILURE_MSG_HDR("incorrect file size in WOFF header");
  }

  if (!file.ReadU16(&font->num_tables) || !font->num_tables) {
    return OTS_FAILURE_MSG_HDR("error reading number of tables");
  }

  uint16_t reserved_value;
  if (!file.ReadU16(&reserved_value) || reserved_value) {
    return OTS_FAILURE_MSG_HDR("error in reserved field of WOFF header");
  }

  uint32_t reported_total_sfnt_size;
  if (!file.ReadU32(&reported_total_sfnt_size)) {
    return OTS_FAILURE_MSG_HDR("error reading total sfnt size");
  }

  // We don't care about these fields of the header:
  //   uint16_t major_version, minor_version
  if (!file.Skip(2 * 2)) {
    return OTS_FAILURE_MSG_HDR("Failed to read 'majorVersion' or 'minorVersion'");
  }

  // Checks metadata block size.
  uint32_t meta_offset;
  uint32_t meta_length;
  uint32_t meta_length_orig;
  if (!file.ReadU32(&meta_offset) ||
      !file.ReadU32(&meta_length) ||
      !file.ReadU32(&meta_length_orig)) {
    return OTS_FAILURE_MSG_HDR("Failed to read header metadata block fields");
  }
  if (meta_offset) {
    if (meta_offset >= length || length - meta_offset < meta_length) {
      return OTS_FAILURE_MSG_HDR("Invalid metadata block offset or length");
    }
  }

  // Checks private data block size.
  uint32_t priv_offset;
  uint32_t priv_length;
  if (!file.ReadU32(&priv_offset) ||
      !file.ReadU32(&priv_length)) {
    return OTS_FAILURE_MSG_HDR("Failed to read header private block fields");
  }
  if (priv_offset) {
    if (priv_offset >= length || length - priv_offset < priv_length) {
      return OTS_FAILURE_MSG_HDR("Invalid private block offset or length");
    }
  }

  // Next up is the list of tables.
  std::vector<ots::TableEntry> tables;

  uint32_t first_index = 0;
  uint32_t last_index = 0;
  // Size of sfnt header plus size of table records.
  uint64_t total_sfnt_size = 12 + 16 * font->num_tables;
  for (unsigned i = 0; i < font->num_tables; ++i) {
    ots::TableEntry table;
    if (!file.ReadU32(&table.tag) ||
        !file.ReadU32(&table.offset) ||
        !file.ReadU32(&table.length) ||
        !file.ReadU32(&table.uncompressed_length) ||
        !file.ReadU32(&table.chksum)) {
      return OTS_FAILURE_MSG_HDR("error reading table directory");
    }

    total_sfnt_size += ots::Round4(table.uncompressed_length);
    if (total_sfnt_size > std::numeric_limits<uint32_t>::max()) {
      return OTS_FAILURE_MSG_HDR("sfnt size overflow");
    }
    tables.push_back(table);
    if (i == 0 || tables[first_index].offset > table.offset)
      first_index = i;
    if (i == 0 || tables[last_index].offset < table.offset)
      last_index = i;
  }

  if (reported_total_sfnt_size != total_sfnt_size) {
    return OTS_FAILURE_MSG_HDR("uncompressed sfnt size mismatch");
  }

  // Table data must follow immediately after the header.
  if (tables[first_index].offset != ots::Round4(file.offset())) {
    return OTS_FAILURE_MSG_HDR("junk before tables in WOFF file");
  }

  if (tables[last_index].offset >= length ||
      length - tables[last_index].offset < tables[last_index].length) {
    return OTS_FAILURE_MSG_HDR("invalid table location/size");
  }
  // Blocks must follow immediately after the previous block.
  // (Except for padding with a maximum of three null bytes)
  uint64_t block_end = ots::Round4(
      static_cast<uint64_t>(tables[last_index].offset) +
      static_cast<uint64_t>(tables[last_index].length));
  if (block_end > std::numeric_limits<uint32_t>::max()) {
    return OTS_FAILURE_MSG_HDR("invalid table location/size");
  }
  if (meta_offset) {
    if (block_end != meta_offset) {
      return OTS_FAILURE_MSG_HDR("Invalid metadata block offset");
    }
    block_end = ots::Round4(static_cast<uint64_t>(meta_offset) +
                            static_cast<uint64_t>(meta_length));
    if (block_end > std::numeric_limits<uint32_t>::max()) {
      return OTS_FAILURE_MSG_HDR("Invalid metadata block length");
    }
  }
  if (priv_offset) {
    if (block_end != priv_offset) {
      return OTS_FAILURE_MSG_HDR("Invalid private block offset");
    }
    block_end = ots::Round4(static_cast<uint64_t>(priv_offset) +
                            static_cast<uint64_t>(priv_length));
    if (block_end > std::numeric_limits<uint32_t>::max()) {
      return OTS_FAILURE_MSG_HDR("Invalid private block length");
    }
  }
  if (block_end != ots::Round4(length)) {
    return OTS_FAILURE_MSG_HDR("File length mismatch (trailing junk?)");
  }

  return ProcessGeneric(header, font, woff_tag, output, data, length, tables, file);
}

bool ProcessWOFF2(ots::FontFile* header, ots::OTSStream* output,
                  const uint8_t* data, size_t length, uint32_t index) {
  return RLBoxProcessWOFF2(header, output, data, length, index, ProcessTTC, ProcessTTF);
}

ots::TableAction GetTableAction(const ots::FontFile *header, uint32_t tag) {
  ots::TableAction action = header->context->GetTableAction(tag);

  if (action == ots::TABLE_ACTION_DEFAULT) {
    action = ots::TABLE_ACTION_DROP;

    for (unsigned i = 0; ; ++i) {
      if (supported_tables[i].tag == 0) break;

      if (supported_tables[i].tag == tag) {
        action = ots::TABLE_ACTION_SANITIZE;
        break;
      }
    }
  }

  assert(action != ots::TABLE_ACTION_DEFAULT); // Should never return this.
  return action;
}

bool GetTableData(const uint8_t *data,
                  const ots::TableEntry& table,
                  ots::Arena &arena,
                  size_t *table_length,
                  const uint8_t **table_data) {
  if (table.uncompressed_length != table.length) {
    // Compressed table. Need to uncompress into memory first.
    *table_length = table.uncompressed_length;
    *table_data = arena.Allocate(*table_length);
    uLongf dest_len = *table_length;
    int r = uncompress((Bytef*) *table_data, &dest_len,
                       data + table.offset, table.length);
    if (r != Z_OK || dest_len != *table_length) {
      return false;
    }
  } else {
    // Uncompressed table. We can process directly from memory.
    *table_data = data + table.offset;
    *table_length = table.length;
  }

  return true;
}

bool ProcessGeneric(ots::FontFile *header,
                    ots::Font *font,
                    uint32_t signature,
                    ots::OTSStream *output,
                    const uint8_t *data, size_t length,
                    const std::vector<ots::TableEntry>& tables,
                    ots::Buffer& file) {
  const size_t data_offset = file.offset();

  uint32_t uncompressed_sum = 0;

  for (unsigned i = 0; i < font->num_tables; ++i) {
    // the tables must be sorted by tag (when taken as big-endian numbers).
    // This also remove the possibility of duplicate tables.
    if (i) {
      const uint32_t this_tag = tables[i].tag;
      const uint32_t prev_tag = tables[i - 1].tag;
      if (this_tag <= prev_tag) {
        OTS_WARNING_MSG_HDR("Table directory is not correctly ordered");
      }
    }

    // all tag names must be built from printable ASCII characters
    if (!ots::CheckTag(tables[i].tag)) {
      OTS_WARNING_MSG_HDR("Invalid table tag: 0x%X", tables[i].tag);
    }

    // tables must be 4-byte aligned
    if (tables[i].offset & 3) {
      return OTS_FAILURE_MSG_TAG("misaligned table", tables[i].tag);
    }

    // and must be within the file
    if (tables[i].offset < data_offset || tables[i].offset >= length) {
      return OTS_FAILURE_MSG_TAG("invalid table offset", tables[i].tag);
    }
    // disallow all tables with a zero length
    if (tables[i].length < 1) {
      // Note: malayalam.ttf has zero length CVT table...
      return OTS_FAILURE_MSG_TAG("zero-length table", tables[i].tag);
    }
    // disallow all tables with a length > 1GB
    if (tables[i].length > 1024 * 1024 * 1024) {
      return OTS_FAILURE_MSG_TAG("table length exceeds 1GB", tables[i].tag);
    }
    // disallow tables where the uncompressed size is < the compressed size.
    if (tables[i].uncompressed_length < tables[i].length) {
      return OTS_FAILURE_MSG_TAG("invalid compressed table", tables[i].tag);
    }
    if (tables[i].uncompressed_length > tables[i].length) {
      // We'll probably be decompressing this table.

      // disallow all tables which decompress to > OTS_MAX_DECOMPRESSED_TABLE_SIZE
      if (tables[i].uncompressed_length > OTS_MAX_DECOMPRESSED_TABLE_SIZE) {
        return OTS_FAILURE_MSG_HDR("%c%c%c%c: decompressed table length exceeds %gMB",
                                   OTS_UNTAG(tables[i].tag),
                                   OTS_MAX_DECOMPRESSED_TABLE_SIZE / (1024.0 * 1024.0));        
      }
      if (uncompressed_sum + tables[i].uncompressed_length < uncompressed_sum) {
        return OTS_FAILURE_MSG_TAG("overflow of decompressed sum", tables[i].tag);
      }

      uncompressed_sum += tables[i].uncompressed_length;
    }
    // since we required that the file be < 1GB in length, and that the table
    // length is < 1GB, the following addtion doesn't overflow
    uint32_t end_byte = tables[i].offset + tables[i].length;
    // Tables in the WOFF file must be aligned 4-byte boundary.
    if (signature == OTS_TAG('w','O','F','F')) {
        end_byte = ots::Round4(end_byte);
    }
    if (!end_byte || end_byte > length) {
      return OTS_FAILURE_MSG_TAG("table overruns end of file", tables[i].tag);
    }
  }

  // All decompressed tables decompressed must be <= OTS_MAX_DECOMPRESSED_FILE_SIZE.
  if (uncompressed_sum > OTS_MAX_DECOMPRESSED_FILE_SIZE) {
    return OTS_FAILURE_MSG_HDR("decompressed sum exceeds %gMB",
                               OTS_MAX_DECOMPRESSED_FILE_SIZE / (1024.0 * 1024.0));        
  }

  if (uncompressed_sum > output->size()) {
    return OTS_FAILURE_MSG_HDR("decompressed sum exceeds output size (%gMB)", output->size() / (1024.0 * 1024.0));
  }

  // check that the tables are not overlapping.
  std::vector<std::pair<uint32_t, uint8_t> > overlap_checker;
  for (unsigned i = 0; i < font->num_tables; ++i) {
    overlap_checker.push_back(
        std::make_pair(tables[i].offset, static_cast<uint8_t>(1) /* start */));
    overlap_checker.push_back(
        std::make_pair(tables[i].offset + tables[i].length,
                       static_cast<uint8_t>(0) /* end */));
  }
  std::sort(overlap_checker.begin(), overlap_checker.end());
  int overlap_count = 0;
  for (unsigned i = 0; i < overlap_checker.size(); ++i) {
    overlap_count += (overlap_checker[i].second ? 1 : -1);
    if (overlap_count > 1) {
      return OTS_FAILURE_MSG_HDR("overlapping tables");
    }
  }

  std::map<uint32_t, ots::TableEntry> table_map;
  for (unsigned i = 0; i < font->num_tables; ++i) {
    table_map[tables[i].tag] = tables[i];
  }

  ots::Arena arena;
  // Parse known tables first as we need to parse them in specific order.
  for (unsigned i = 0; ; ++i) {
    if (supported_tables[i].tag == 0) break;

    uint32_t tag = supported_tables[i].tag;
    const auto &it = table_map.find(tag);
    if (it == table_map.cend()) {
      if (supported_tables[i].required) {
        return OTS_FAILURE_MSG_TAG("missing required table", tag);
      }
    } else {
      if (!font->ParseTable(it->second, data, arena)) {
        return OTS_FAILURE_MSG_TAG("Failed to parse table", tag);
      }
    }
  }

  // Then parse any tables left.
  for (const auto &table_entry : tables) {
    if (!font->GetTable(table_entry.tag)) {
      if (!font->ParseTable(table_entry, data, arena)) {
        return OTS_FAILURE_MSG_TAG("Failed to parse table", table_entry.tag);
      }
    }
  }

  ots::Table *glyf = font->GetTable(OTS_TAG_GLYF);
  ots::Table *loca = font->GetTable(OTS_TAG_LOCA);
  ots::Table *cff  = font->GetTable(OTS_TAG_CFF);
  ots::Table *cff2 = font->GetTable(OTS_TAG_CFF2);

  if (glyf && loca) {
    if (font->version != 0x000010000) {
      OTS_WARNING_MSG_HDR("wrong sfntVersion for glyph data");
      font->version = 0x000010000;
    }
    if (cff)
       cff->Drop("font contains both CFF and glyf/loca tables");
    if (cff2)
       cff2->Drop("font contains both CFF and glyf/loca tables");
  } else if (cff || cff2) {
    if (font->version != OTS_TAG('O','T','T','O')) {
      OTS_WARNING_MSG_HDR("wrong sfntVersion for glyph data");
      font->version = OTS_TAG('O','T','T','O');
    }
    if (glyf)
       glyf->Drop("font contains both CFF and glyf tables");
    if (loca)
       loca->Drop("font contains both CFF and loca tables");
  } else if (font->GetTable(OTS_TAG('C','B','D','T')) &&
             font->GetTable(OTS_TAG('C','B','L','C'))) {
      // We don't sanitize bitmap tables, but don’t reject bitmap-only fonts if
      // we are asked to pass them thru.
  } else {
      return OTS_FAILURE_MSG_HDR("no supported glyph data table(s) present");
  }

  uint16_t num_output_tables = 0;
  for (const auto &it : table_map) {
    ots::Table *table = font->GetTable(it.first);
    if (table)
      num_output_tables++;
  }

  uint16_t max_pow2 = 0;
  while (1u << (max_pow2 + 1) <= num_output_tables) {
    max_pow2++;
  }
  const uint16_t output_search_range = (1u << max_pow2) << 4;

  // most of the errors here are highly unlikely - they'd only occur if the
  // output stream returns a failure, e.g. lack of space to write
  output->ResetChecksum();
  if (!output->WriteU32(font->version) ||
      !output->WriteU16(num_output_tables) ||
      !output->WriteU16(output_search_range) ||
      !output->WriteU16(max_pow2) ||
      !output->WriteU16((num_output_tables << 4) - output_search_range)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }
  const uint32_t offset_table_chksum = output->chksum();

  const size_t table_record_offset = output->Tell();
  if (!output->Pad(16 * num_output_tables)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }

  std::vector<ots::TableEntry> out_tables;

  size_t head_table_offset = 0;
  for (const auto &it : table_map) {
    uint32_t input_offset = it.second.offset;
    const auto &ot = header->table_entries.find(input_offset);
    if (ot != header->table_entries.end()) {
      ots::TableEntry out = ot->second;
      if (out.tag == OTS_TAG('h','e','a','d')) {
        head_table_offset = out.offset;
      }
      out_tables.push_back(out);
    } else {
      ots::TableEntry out;
      out.tag = it.first;
      out.offset = output->Tell();

      if (out.tag == OTS_TAG('h','e','a','d')) {
        head_table_offset = out.offset;
      }

      ots::Table *table = font->GetTable(out.tag);
      if (table) {
        output->ResetChecksum();
        if (!table->Serialize(output)) {
          return OTS_FAILURE_MSG_TAG("Failed to serialize table", out.tag);
        }

        const size_t end_offset = output->Tell();
        if (end_offset <= out.offset) {
          // paranoid check. |end_offset| is supposed to be greater than the offset,
          // as long as the Tell() interface is implemented correctly.
          return OTS_FAILURE_MSG_TAG("Table is empty or have -ve size", out.tag);
        }
        out.length = end_offset - out.offset;

        // align tables to four bytes
        if (!output->Pad((4 - (end_offset & 3)) % 4)) {
          return OTS_FAILURE_MSG_TAG("Failed to pad table to 4 bytes", out.tag);
        }
        out.chksum = output->chksum();
        out_tables.push_back(out);
        header->table_entries[input_offset] = out;
      }
    }
  }

  const size_t end_of_file = output->Tell();

  // Need to sort the output tables for inclusion in the file
  std::sort(out_tables.begin(), out_tables.end());
  if (!output->Seek(table_record_offset)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }

  output->ResetChecksum();
  uint32_t tables_chksum = 0;
  for (unsigned i = 0; i < out_tables.size(); ++i) {
    if (!output->WriteU32(out_tables[i].tag) ||
        !output->WriteU32(out_tables[i].chksum) ||
        !output->WriteU32(out_tables[i].offset) ||
        !output->WriteU32(out_tables[i].length)) {
      return OTS_FAILURE_MSG_HDR("error writing output");
    }
    tables_chksum += out_tables[i].chksum;
  }
  const uint32_t table_record_chksum = output->chksum();

  // http://www.microsoft.com/typography/otspec/otff.htm
  const uint32_t file_chksum
      = offset_table_chksum + tables_chksum + table_record_chksum;
  const uint32_t chksum_magic = static_cast<uint32_t>(0xb1b0afba) - file_chksum;

  // seek into the 'head' table and write in the checksum magic value
  if (!head_table_offset) {
    return OTS_FAILURE_MSG_HDR("internal error!");
  }
  if (!output->Seek(head_table_offset + 8)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }
  if (!output->WriteU32(chksum_magic)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }

  if (!output->Seek(end_of_file)) {
    return OTS_FAILURE_MSG_HDR("error writing output");
  }

  return true;
}

}  // namespace

namespace ots {

FontFile::~FontFile() {
  for (const auto& it : tables) {
    delete it.second;
  }
  tables.clear();
}

bool Font::ParseTable(const TableEntry& table_entry, const uint8_t* data,
                      Arena &arena) {
  uint32_t tag = table_entry.tag;
  TableAction action = GetTableAction(file, tag);
  if (action == TABLE_ACTION_DROP) {
    return true;
  }

  const auto &it = file->tables.find(table_entry);
  if (it != file->tables.end()) {
    m_tables[tag] = it->second;
    return true;
  }

  Table *table = NULL;
  bool ret = false;

  if (action == TABLE_ACTION_PASSTHRU) {
    table = new TablePassthru(this, tag);
  } else {
    switch (tag) {
      case OTS_TAG_AVAR: table = new OpenTypeAVAR(this, tag); break;
      case OTS_TAG_CFF:  table = new OpenTypeCFF(this,  tag); break;
      case OTS_TAG_CFF2: table = new OpenTypeCFF2(this, tag); break;
      case OTS_TAG_CMAP: table = new OpenTypeCMAP(this, tag); break;
      case OTS_TAG_CVAR: table = new OpenTypeCVAR(this, tag); break;
      case OTS_TAG_CVT:  table = new OpenTypeCVT(this,  tag); break;
      case OTS_TAG_FPGM: table = new OpenTypeFPGM(this, tag); break;
      case OTS_TAG_FVAR: table = new OpenTypeFVAR(this, tag); break;
      case OTS_TAG_GASP: table = new OpenTypeGASP(this, tag); break;
      case OTS_TAG_GDEF: table = new OpenTypeGDEF(this, tag); break;
      case OTS_TAG_GLYF: table = new OpenTypeGLYF(this, tag); break;
      case OTS_TAG_GPOS: table = new OpenTypeGPOS(this, tag); break;
      case OTS_TAG_GSUB: table = new OpenTypeGSUB(this, tag); break;
      case OTS_TAG_GVAR: table = new OpenTypeGVAR(this, tag); break;
      case OTS_TAG_HDMX: table = new OpenTypeHDMX(this, tag); break;
      case OTS_TAG_HEAD: table = new OpenTypeHEAD(this, tag); break;
      case OTS_TAG_HHEA: table = new OpenTypeHHEA(this, tag); break;
      case OTS_TAG_HMTX: table = new OpenTypeHMTX(this, tag); break;
      case OTS_TAG_HVAR: table = new OpenTypeHVAR(this, tag); break;
      case OTS_TAG_KERN: table = new OpenTypeKERN(this, tag); break;
      case OTS_TAG_LOCA: table = new OpenTypeLOCA(this, tag); break;
      case OTS_TAG_LTSH: table = new OpenTypeLTSH(this, tag); break;
      case OTS_TAG_MATH: table = new OpenTypeMATH(this, tag); break;
      case OTS_TAG_MAXP: table = new OpenTypeMAXP(this, tag); break;
      case OTS_TAG_MVAR: table = new OpenTypeMVAR(this, tag); break;
      case OTS_TAG_NAME: table = new OpenTypeNAME(this, tag); break;
      case OTS_TAG_OS2:  table = new OpenTypeOS2(this,  tag); break;
      case OTS_TAG_POST: table = new OpenTypePOST(this, tag); break;
      case OTS_TAG_PREP: table = new OpenTypePREP(this, tag); break;
      case OTS_TAG_STAT: table = new OpenTypeSTAT(this, tag); break;
      case OTS_TAG_VDMX: table = new OpenTypeVDMX(this, tag); break;
      case OTS_TAG_VHEA: table = new OpenTypeVHEA(this, tag); break;
      case OTS_TAG_VMTX: table = new OpenTypeVMTX(this, tag); break;
      case OTS_TAG_VORG: table = new OpenTypeVORG(this, tag); break;
      case OTS_TAG_VVAR: table = new OpenTypeVVAR(this, tag); break;
      // Graphite tables
#ifdef OTS_GRAPHITE
      case OTS_TAG_FEAT: table = new OpenTypeFEAT(this, tag); break;
      case OTS_TAG_GLAT: table = new OpenTypeGLAT(this, tag); break;
      case OTS_TAG_GLOC: table = new OpenTypeGLOC(this, tag); break;
      case OTS_TAG_SILE: table = new OpenTypeSILE(this, tag); break;
      case OTS_TAG_SILF: table = new OpenTypeSILF(this, tag); break;
      case OTS_TAG_SILL: table = new OpenTypeSILL(this, tag); break;
#endif
      default: break;
    }
  }

  if (table) {
    const uint8_t* table_data;
    size_t table_length;

    ret = GetTableData(data, table_entry, arena, &table_length, &table_data);
    if (ret) {
      // FIXME: Parsing some tables will fail if the table is not added to
      // m_tables first.
      m_tables[tag] = table;
      ret = table->Parse(table_data, table_length);
      if (ret)
        file->tables[table_entry] = table;
      else
        m_tables.erase(tag);
    }
  }

  if (!ret)
    delete table;

  return ret;
}

Table* Font::GetTable(uint32_t tag) const {
  const auto &it = m_tables.find(tag);
  if (it != m_tables.end() && it->second && it->second->ShouldSerialize())
    return it->second;
  return NULL;
}

Table* Font::GetTypedTable(uint32_t tag) const {
  Table* t = GetTable(tag);
  if (t && t->Type() == tag)
    return t;
  return NULL;
}

void Font::DropGraphite() {
  file->context->Message(0, "Dropping all Graphite tables");
  for (const std::pair<uint32_t, Table*> entry : m_tables) {
    if (entry.first == OTS_TAG_FEAT ||
        entry.first == OTS_TAG_GLAT ||
        entry.first == OTS_TAG_GLOC ||
        entry.first == OTS_TAG_SILE ||
        entry.first == OTS_TAG_SILF ||
        entry.first == OTS_TAG_SILL) {
      entry.second->Drop("Discarding Graphite table");
    }
  }
}

void Font::DropVariations() {
  file->context->Message(0, "Dropping all Variation tables");
  for (const std::pair<uint32_t, Table*> entry : m_tables) {
    if (entry.first == OTS_TAG_AVAR ||
        entry.first == OTS_TAG_CVAR ||
        entry.first == OTS_TAG_FVAR ||
        entry.first == OTS_TAG_GVAR ||
        entry.first == OTS_TAG_HVAR ||
        entry.first == OTS_TAG_MVAR ||
        entry.first == OTS_TAG_STAT ||
        entry.first == OTS_TAG_VVAR) {
      entry.second->Drop("Discarding Variations table");
    }
  }
}

bool Table::ShouldSerialize() {
  return m_shouldSerialize;
}

void Table::Message(int level, const char *format, va_list va) {
  char msg[206] = { OTS_UNTAG(m_tag), ':', ' ' };
  std::vsnprintf(msg + 6, 200, format, va);
  m_font->file->context->Message(level, msg);
}

bool Table::Error(const char *format, ...) {
  va_list va;
  va_start(va, format);
  Message(0, format, va);
  va_end(va);

  return false;
}

bool Table::Warning(const char *format, ...) {
  va_list va;
  va_start(va, format);
  Message(1, format, va);
  va_end(va);

  return true;
}

bool Table::Drop(const char *format, ...) {
  m_shouldSerialize = false;

  va_list va;
  va_start(va, format);
  Message(0, format, va);
  m_font->file->context->Message(0, "Table discarded");
  va_end(va);

  return true;
}

bool Table::DropGraphite(const char *format, ...) {
  va_list va;
  va_start(va, format);
  Message(0, format, va);
  va_end(va);

  m_font->DropGraphite();
  return true;
}

bool Table::DropVariations(const char *format, ...) {
  va_list va;
  va_start(va, format);
  Message(0, format, va);
  va_end(va);

  m_font->DropVariations();
  return true;
}

bool TablePassthru::Parse(const uint8_t *data, size_t length) {
  m_data = data;
  m_length = length;
  return true;
}

bool TablePassthru::Serialize(OTSStream *out) {
    if (!out->Write(m_data, m_length)) {
    return Error("Failed to write table");
  }

  return true;
}

bool OTSContext::Process(OTSStream *output,
                         const uint8_t *data,
                         size_t length,
                         uint32_t index) {
  FontFile header;
  Font font(&header);
  header.context = this;

  if (length < 4) {
    return OTS_FAILURE_MSG_(&header, "file less than 4 bytes");
  }

  bool result;
  if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == 'F') {
    result = ProcessWOFF(&header, &font, output, data, length);
  } else if (data[0] == 'w' && data[1] == 'O' && data[2] == 'F' && data[3] == '2') {
    result = ProcessWOFF2(&header, output, data, length, index);
  } else if (data[0] == 't' && data[1] == 't' && data[2] == 'c' && data[3] == 'f') {
    result = ProcessTTC(&header, output, data, length, index);
  } else {
    result = ProcessTTF(&header, &font, output, data, length);
  }

  return result;
}

}  // namespace ots
