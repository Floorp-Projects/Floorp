// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "post.h"

#include "maxp.h"

// post - PostScript
// http://www.microsoft.com/opentype/otspec/post.htm

namespace ots {

bool ots_post_parse(OpenTypeFile *file, const uint8_t *data, size_t length) {
  Buffer table(data, length);

  OpenTypePOST *post = new OpenTypePOST;
  file->post = post;

  if (!table.ReadU32(&post->version) ||
      !table.ReadU32(&post->italic_angle) ||
      !table.ReadS16(&post->underline) ||
      !table.ReadS16(&post->underline_thickness) ||
      !table.ReadU32(&post->is_fixed_pitch)) {
    return OTS_FAILURE();
  }

  if (post->underline_thickness < 0) {
    post->underline_thickness = 1;
  }

  if (post->version == 0x00010000) {
    return true;
  } else if (post->version == 0x00030000) {
    return true;
  } else if (post->version != 0x00020000) {
    // 0x00025000 is deprecated. We don't accept it.
    return OTS_FAILURE();
  }

  // We have a version 2 table with a list of Pascal strings at the end

  // We don't care about the memory usage fields. We'll set all these to zero
  // when serialising
  if (!table.Skip(16)) {
    return OTS_FAILURE();
  }

  uint16_t num_glyphs = 0;
  if (!table.ReadU16(&num_glyphs)) {
    return OTS_FAILURE();
  }

  if (!file->maxp) {
    return OTS_FAILURE();
  }

  if (num_glyphs == 0) {
    if (file->maxp->num_glyphs > 258) {
      return OTS_FAILURE();
    }
    OTS_WARNING("table version is 1, but no glyf names are found");
    // workaround for fonts in http://www.fontsquirrel.com/fontface
    // (e.g., yataghan.ttf).
    post->version = 0x00010000;
    return true;
  }

  if (num_glyphs != file->maxp->num_glyphs) {
    // Note: Fixedsys500c.ttf seems to have inconsistent num_glyphs values.
    return OTS_FAILURE();
  }

  post->glyph_name_index.resize(num_glyphs);
  for (unsigned i = 0; i < num_glyphs; ++i) {
    if (!table.ReadU16(&post->glyph_name_index[i])) {
      return OTS_FAILURE();
    }
    if (post->glyph_name_index[i] >= 32768) {
      // Note: droid_arialuni.ttf fails this test.
      return OTS_FAILURE();  // reserved area.
    }
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
      return OTS_FAILURE();
    }
    if (std::memchr(strings + 1, '\0', string_length)) {
      return OTS_FAILURE();
    }
    post->names.push_back(
        std::string(reinterpret_cast<const char*>(strings + 1), string_length));
    strings += 1 + string_length;
  }
  const unsigned num_strings = post->names.size();

  // check that all the references are within bounds
  for (unsigned i = 0; i < num_glyphs; ++i) {
    unsigned offset = post->glyph_name_index[i];
    if (offset < 258) {
      continue;
    }

    offset -= 258;
    if (offset >= num_strings) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool ots_post_should_serialise(OpenTypeFile *file) {
  return file->post != NULL;
}

bool ots_post_serialise(OTSStream *out, OpenTypeFile *file) {
  const OpenTypePOST *post = file->post;

  // OpenType with CFF glyphs must have v3 post table.
  if (file->post && file->cff && file->post->version != 0x00030000) {
    return OTS_FAILURE();
  }

  if (!out->WriteU32(post->version) ||
      !out->WriteU32(post->italic_angle) ||
      !out->WriteS16(post->underline) ||
      !out->WriteS16(post->underline_thickness) ||
      !out->WriteU32(post->is_fixed_pitch) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0) ||
      !out->WriteU32(0)) {
    return OTS_FAILURE();
  }

  if (post->version != 0x00020000) {
    return true;  // v1.0 and v3.0 does not have glyph names.
  }

  if (!out->WriteU16(post->glyph_name_index.size())) {
    return OTS_FAILURE();
  }

  for (unsigned i = 0; i < post->glyph_name_index.size(); ++i) {
    if (!out->WriteU16(post->glyph_name_index[i])) {
      return OTS_FAILURE();
    }
  }

  // Now we just have to write out the strings in the correct order
  for (unsigned i = 0; i < post->names.size(); ++i) {
    const std::string& s = post->names[i];
    const uint8_t string_length = s.size();
    if (!out->Write(&string_length, 1)) {
      return OTS_FAILURE();
    }
    // Some ttf fonts (e.g., frank.ttf on Windows Vista) have zero-length name.
    // We allow them.
    if (string_length > 0 && !out->Write(s.data(), string_length)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

void ots_post_free(OpenTypeFile *file) {
  delete file->post;
}

}  // namespace ots
