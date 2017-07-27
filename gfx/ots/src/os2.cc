// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "os2.h"
#include "head.h"

// OS/2 - OS/2 and Windows Metrics
// http://www.microsoft.com/typography/otspec/os2.htm

namespace ots {

bool OpenTypeOS2::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU16(&this->table.version) ||
      !table.ReadS16(&this->table.avg_char_width) ||
      !table.ReadU16(&this->table.weight_class) ||
      !table.ReadU16(&this->table.width_class) ||
      !table.ReadU16(&this->table.type) ||
      !table.ReadS16(&this->table.subscript_x_size) ||
      !table.ReadS16(&this->table.subscript_y_size) ||
      !table.ReadS16(&this->table.subscript_x_offset) ||
      !table.ReadS16(&this->table.subscript_y_offset) ||
      !table.ReadS16(&this->table.superscript_x_size) ||
      !table.ReadS16(&this->table.superscript_y_size) ||
      !table.ReadS16(&this->table.superscript_x_offset) ||
      !table.ReadS16(&this->table.superscript_y_offset) ||
      !table.ReadS16(&this->table.strikeout_size) ||
      !table.ReadS16(&this->table.strikeout_position) ||
      !table.ReadS16(&this->table.family_class)) {
    return Error("Error reading basic table elements");
  }

  if (this->table.version > 5) {
    return Error("Unsupported table version: %u", this->table.version);
  }

  // Follow WPF Font Selection Model's advice.
  if (1 <= this->table.weight_class && this->table.weight_class <= 9) {
    Warning("Bad usWeightClass: %u, changing it to %u",
            this->table.weight_class, this->table.weight_class * 100);
    this->table.weight_class *= 100;
  }
  // Ditto.
  if (this->table.weight_class > 999) {
    Warning("Bad usWeightClass: %u, changing it to %d",
             this->table.weight_class, 999);
    this->table.weight_class = 999;
  }

  if (this->table.width_class < 1) {
    Warning("Bad usWidthClass: %u, changing it to %d",
            this->table.width_class, 1);
    this->table.width_class = 1;
  } else if (this->table.width_class > 9) {
    Warning("Bad usWidthClass: %u, changing it to %d",
            this->table.width_class, 9);
    this->table.width_class = 9;
  }

  // lowest 3 bits of fsType are exclusive.
  if (this->table.type & 0x2) {
    // mask bits 2 & 3.
    this->table.type &= 0xfff3u;
  } else if (this->table.type & 0x4) {
    // mask bits 1 & 3.
    this->table.type &= 0xfff4u;
  } else if (this->table.type & 0x8) {
    // mask bits 1 & 2.
    this->table.type &= 0xfff9u;
  }

  // mask reserved bits. use only 0..3, 8, 9 bits.
  this->table.type &= 0x30f;

#define SET_TO_ZERO(a, b)                                                      \
  if (this->table.b < 0) {                                                     \
    Warning("Bad " a ": %d, setting it to zero", this->table.b);               \
    this->table.b = 0;                                                         \
  }

  SET_TO_ZERO("ySubscriptXSize", subscript_x_size);
  SET_TO_ZERO("ySubscriptYSize", subscript_y_size);
  SET_TO_ZERO("ySuperscriptXSize", superscript_x_size);
  SET_TO_ZERO("ySuperscriptYSize", superscript_y_size);
  SET_TO_ZERO("yStrikeoutSize", strikeout_size);
#undef SET_TO_ZERO

  static std::string panose_strings[10] = {
    "bFamilyType",
    "bSerifStyle",
    "bWeight",
    "bProportion",
    "bContrast",
    "bStrokeVariation",
    "bArmStyle",
    "bLetterform",
    "bMidline",
    "bXHeight",
  };
  for (unsigned i = 0; i < 10; ++i) {
    if (!table.ReadU8(&this->table.panose[i])) {
      return Error("Failed to read PANOSE %s", panose_strings[i].c_str());
    }
  }

  if (!table.ReadU32(&this->table.unicode_range_1) ||
      !table.ReadU32(&this->table.unicode_range_2) ||
      !table.ReadU32(&this->table.unicode_range_3) ||
      !table.ReadU32(&this->table.unicode_range_4) ||
      !table.ReadU32(&this->table.vendor_id) ||
      !table.ReadU16(&this->table.selection) ||
      !table.ReadU16(&this->table.first_char_index) ||
      !table.ReadU16(&this->table.last_char_index) ||
      !table.ReadS16(&this->table.typo_ascender) ||
      !table.ReadS16(&this->table.typo_descender) ||
      !table.ReadS16(&this->table.typo_linegap) ||
      !table.ReadU16(&this->table.win_ascent) ||
      !table.ReadU16(&this->table.win_descent)) {
    return Error("Error reading more basic table fields");
  }

  // If bit 6 is set, then bits 0 and 5 must be clear.
  if (this->table.selection & 0x40) {
    this->table.selection &= 0xffdeu;
  }

  // the settings of bits 0 and 1 must be reflected in the macStyle bits
  // in the 'head' table.
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));

  if ((this->table.selection & 0x1) &&
      head && !(head->mac_style & 0x2)) {
    Warning("Adjusting head.macStyle (italic) to match fsSelection");
    head->mac_style |= 0x2;
  }
  if ((this->table.selection & 0x2) &&
      head && !(head->mac_style & 0x4)) {
    Warning("Adjusting head.macStyle (underscore) to match fsSelection");
    head->mac_style |= 0x4;
  }

  // While bit 6 on implies that bits 0 and 1 of macStyle are clear,
  // the reverse is not true.
  if ((this->table.selection & 0x40) &&
      head && (head->mac_style & 0x3)) {
    Warning("Adjusting head.macStyle (regular) to match fsSelection");
    head->mac_style &= 0xfffcu;
  }

  if ((this->table.version < 4) &&
      (this->table.selection & 0x300)) {
    // bit 8 and 9 must be unset in OS/2 table versions less than 4.
    return Error("fSelection bits 8 and 9 must be unset for table version %d",
                 this->table.version);
  }

  // mask reserved bits. use only 0..9 bits.
  this->table.selection &= 0x3ff;

  if (this->table.first_char_index > this->table.last_char_index) {
    return Error("usFirstCharIndex %d > usLastCharIndex %d",
                 this->table.first_char_index, this->table.last_char_index);
  }
  if (this->table.typo_linegap < 0) {
    Warning("Bad sTypoLineGap, setting it to 0: %d", this->table.typo_linegap);
    this->table.typo_linegap = 0;
  }

  if (this->table.version < 1) {
    // http://www.microsoft.com/typography/otspec/os2ver0.htm
    return true;
  }

  if (length < offsetof(OS2Data, code_page_range_2)) {
    Warning("Bad version number, setting it to 0: %u", this->table.version);
    // Some fonts (e.g., kredit1.ttf and quinquef.ttf) have weird version
    // numbers. Fix them.
    this->table.version = 0;
    return true;
  }

  if (!table.ReadU32(&this->table.code_page_range_1) ||
      !table.ReadU32(&this->table.code_page_range_2)) {
    return Error("Failed to read ulCodePageRange1 or ulCodePageRange2");
  }

  if (this->table.version < 2) {
    // http://www.microsoft.com/typography/otspec/os2ver1.htm
    return true;
  }

  if (length < offsetof(OS2Data, max_context)) {
    Warning("Bad version number, setting it to 1: %u", this->table.version);
    // some Japanese fonts (e.g., mona.ttf) have weird version number.
    // fix them.
    this->table.version = 1;
    return true;
  }

  if (!table.ReadS16(&this->table.x_height) ||
      !table.ReadS16(&this->table.cap_height) ||
      !table.ReadU16(&this->table.default_char) ||
      !table.ReadU16(&this->table.break_char) ||
      !table.ReadU16(&this->table.max_context)) {
    return Error("Failed to read version 2-specific fields");
  }

  if (this->table.x_height < 0) {
    Warning("Bad sxHeight settig it to 0: %d", this->table.x_height);
    this->table.x_height = 0;
  }
  if (this->table.cap_height < 0) {
    Warning("Bad sCapHeight setting it to 0: %d", this->table.cap_height);
    this->table.cap_height = 0;
  }

  if (this->table.version < 5) {
    // http://www.microsoft.com/typography/otspec/os2ver4.htm
    return true;
  }

  if (!table.ReadU16(&this->table.lower_optical_pointsize) ||
      !table.ReadU16(&this->table.upper_optical_pointsize)) {
    return Error("Failed to read version 5-specific fields");
  }

  if (this->table.lower_optical_pointsize > 0xFFFE) {
    Warning("usLowerOpticalPointSize is bigger than 0xFFFE: %d",
            this->table.lower_optical_pointsize);
    this->table.lower_optical_pointsize = 0xFFFE;
  }

  if (this->table.upper_optical_pointsize < 2) {
    Warning("usUpperOpticalPointSize is lower than 2: %d",
            this->table.upper_optical_pointsize);
    this->table.upper_optical_pointsize = 2;
  }

  return true;
}

bool OpenTypeOS2::Serialize(OTSStream *out) {
  if (!out->WriteU16(this->table.version) ||
      !out->WriteS16(this->table.avg_char_width) ||
      !out->WriteU16(this->table.weight_class) ||
      !out->WriteU16(this->table.width_class) ||
      !out->WriteU16(this->table.type) ||
      !out->WriteS16(this->table.subscript_x_size) ||
      !out->WriteS16(this->table.subscript_y_size) ||
      !out->WriteS16(this->table.subscript_x_offset) ||
      !out->WriteS16(this->table.subscript_y_offset) ||
      !out->WriteS16(this->table.superscript_x_size) ||
      !out->WriteS16(this->table.superscript_y_size) ||
      !out->WriteS16(this->table.superscript_x_offset) ||
      !out->WriteS16(this->table.superscript_y_offset) ||
      !out->WriteS16(this->table.strikeout_size) ||
      !out->WriteS16(this->table.strikeout_position) ||
      !out->WriteS16(this->table.family_class)) {
    return Error("Failed to write basic table data");
  }

  for (unsigned i = 0; i < 10; ++i) {
    if (!out->Write(&this->table.panose[i], 1)) {
      return Error("Failed to write PANOSE data");
    }
  }

  if (!out->WriteU32(this->table.unicode_range_1) ||
      !out->WriteU32(this->table.unicode_range_2) ||
      !out->WriteU32(this->table.unicode_range_3) ||
      !out->WriteU32(this->table.unicode_range_4) ||
      !out->WriteU32(this->table.vendor_id) ||
      !out->WriteU16(this->table.selection) ||
      !out->WriteU16(this->table.first_char_index) ||
      !out->WriteU16(this->table.last_char_index) ||
      !out->WriteS16(this->table.typo_ascender) ||
      !out->WriteS16(this->table.typo_descender) ||
      !out->WriteS16(this->table.typo_linegap) ||
      !out->WriteU16(this->table.win_ascent) ||
      !out->WriteU16(this->table.win_descent)) {
    return Error("Failed to write version 1-specific fields");
  }

  if (this->table.version < 1) {
    return true;
  }

  if (!out->WriteU32(this->table.code_page_range_1) ||
      !out->WriteU32(this->table.code_page_range_2)) {
    return Error("Failed to write codepage ranges");
  }

  if (this->table.version < 2) {
    return true;
  }

  if (!out->WriteS16(this->table.x_height) ||
      !out->WriteS16(this->table.cap_height) ||
      !out->WriteU16(this->table.default_char) ||
      !out->WriteU16(this->table.break_char) ||
      !out->WriteU16(this->table.max_context)) {
    return Error("Failed to write version 2-specific fields");
  }

  if (this->table.version < 5) {
    return true;
  }

  if (!out->WriteU16(this->table.lower_optical_pointsize) ||
      !out->WriteU16(this->table.upper_optical_pointsize)) {
    return Error("Failed to write version 5-specific fields");
  }

  return true;
}

}  // namespace ots
