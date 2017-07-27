// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "post.h"

#include "maxp.h"

// post - PostScript
// http://www.microsoft.com/typography/otspec/post.htm

namespace ots {

bool OpenTypePOST::Parse(const uint8_t *data, size_t length) {
  Buffer table(data, length);

  if (!table.ReadU32(&this->version)) {
    return Error("Failed to read table version");
  }

  if (this->version != 0x00010000 &&
      this->version != 0x00020000 &&
      this->version != 0x00030000) {
    // 0x00025000 is deprecated. We don't accept it.
    return Error("Unsupported table version 0x%x", this->version);
  }

  if (!table.ReadU32(&this->italic_angle) ||
      !table.ReadS16(&this->underline) ||
      !table.ReadS16(&this->underline_thickness) ||
      !table.ReadU32(&this->is_fixed_pitch) ||
      // We don't care about the memory usage fields. We'll set all these to
      // zero when serialising
      !table.Skip(16)) {
    return Error("Failed to read table header");
  }

  if (this->underline_thickness < 0) {
    this->underline_thickness = 1;
  }

  if (this->version == 0x00010000 || this->version == 0x00030000) {
    return true;
  }

  // We have a version 2 table with a list of Pascal strings at the end

  uint16_t num_glyphs = 0;
  if (!table.ReadU16(&num_glyphs)) {
    return Error("Failed to read numberOfGlyphs");
  }

  OpenTypeMAXP* maxp = static_cast<OpenTypeMAXP*>
    (GetFont()->GetTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Missing required maxp table");
  }

  if (num_glyphs == 0) {
    if (maxp->num_glyphs > 258) {
      return Error("Can't have no glyphs in the post table if there are more "
                   "than 258 glyphs in the font");
    }
    // workaround for fonts in http://www.fontsquirrel.com/fontface
    // (e.g., yataghan.ttf).
    this->version = 0x00010000;
    return Warning("Table version is 1, but no glyph names are found");
  }

  if (num_glyphs != maxp->num_glyphs) {
    // Note: Fixedsys500c.ttf seems to have inconsistent num_glyphs values.
    return Error("Bad number of glyphs: %d", num_glyphs);
  }

  this->glyph_name_index.resize(num_glyphs);
  for (unsigned i = 0; i < num_glyphs; ++i) {
    if (!table.ReadU16(&this->glyph_name_index[i])) {
      return Error("Failed to read glyph name %d", i);
    }
    // Note: A strict interpretation of the specification requires name indexes
    // are less than 32768. This, however, excludes fonts like unifont.ttf
    // which cover all of unicode.
  }

  // Now we have an array of Pascal strings. We have to check that they are all
  // valid and read them in.
  const size_t strings_offset = table.offset();
  const uint8_t *strings = data + strings_offset;
  const uint8_t *strings_end = data + length;

  for (;;) {
    if (strings == strings_end) break;
    const unsigned string_length = *strings;
    if (strings + 1 + string_length > strings_end) {
      return Error("Bad string length %d", string_length);
    }
    if (std::memchr(strings + 1, '\0', string_length)) {
      return Error("Bad string of length %d", string_length);
    }
    this->names.push_back(
        std::string(reinterpret_cast<const char*>(strings + 1), string_length));
    strings += 1 + string_length;
  }
  const unsigned num_strings = this->names.size();

  // check that all the references are within bounds
  for (unsigned i = 0; i < num_glyphs; ++i) {
    unsigned offset = this->glyph_name_index[i];
    if (offset < 258) {
      continue;
    }

    offset -= 258;
    if (offset >= num_strings) {
      return Error("Bad string index %d", offset);
    }
  }

  return true;
}

bool OpenTypePOST::Serialize(OTSStream *out) {
  // OpenType with CFF glyphs must have v3 post table.
  if (GetFont()->GetTable(OTS_TAG_CFF) && this->version != 0x00030000) {
    return Error("Only version supported for fonts with CFF table is 0x00030000"
                 " not 0x%x", this->version);
  }

  if (!out->WriteU32(this->version) ||
      !out->WriteU32(this->italic_angle) ||
      !out->WriteS16(this->underline) ||
      !out->WriteS16(this->underline_thickness) ||
      !out->WriteU32(this->is_fixed_pitch) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0)) {
    return Error("Failed to write post header");
  }

  if (this->version != 0x00020000) {
    return true;  // v1.0 and v3.0 does not have glyph names.
  }

  const uint16_t num_indexes =
      static_cast<uint16_t>(this->glyph_name_index.size());
  if (num_indexes != this->glyph_name_index.size() ||
      !out->WriteU16(num_indexes)) {
    return Error("Failed to write number of indices");
  }

  for (uint16_t i = 0; i < num_indexes; ++i) {
    if (!out->WriteU16(this->glyph_name_index[i])) {
      return Error("Failed to write name index %d", i);
    }
  }

  // Now we just have to write out the strings in the correct order
  for (unsigned i = 0; i < this->names.size(); ++i) {
    const std::string& s = this->names[i];
    const uint8_t string_length = static_cast<uint8_t>(s.size());
    if (string_length != s.size() ||
        !out->Write(&string_length, 1)) {
      return Error("Failed to write string %d", i);
    }
    // Some ttf fonts (e.g., frank.ttf on Windows Vista) have zero-length name.
    // We allow them.
    if (string_length > 0 && !out->Write(s.data(), string_length)) {
      return Error("Failed to write string length for string %d", i);
    }
  }

  return true;
}

}  // namespace ots
