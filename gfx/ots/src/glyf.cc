// Copyright (c) 2009-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "glyf.h"

#include <algorithm>
#include <limits>

#include "head.h"
#include "loca.h"
#include "maxp.h"

// glyf - Glyph Data
// http://www.microsoft.com/typography/otspec/glyf.htm

namespace ots {

bool OpenTypeGLYF::ParseFlagsForSimpleGlyph(Buffer &glyph,
                                            uint32_t num_flags,
                                            uint32_t *flag_index,
                                            uint32_t *coordinates_length) {
  uint8_t flag = 0;
  if (!glyph.ReadU8(&flag)) {
    return Error("Can't read flag");
  }

  uint32_t delta = 0;
  if (flag & (1u << 1)) {  // x-Short
    ++delta;
  } else if (!(flag & (1u << 4))) {
    delta += 2;
  }

  if (flag & (1u << 2)) {  // y-Short
    ++delta;
  } else if (!(flag & (1u << 5))) {
    delta += 2;
  }

  /* MS and Apple specs say this bit is reserved and must be set to zero, but
   * Apple spec then contradicts itself and says it should be set on the first
   * contour flag for simple glyphs with overlapping contours:
   * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6AATIntro.html
   * (“Overlapping contours” section) */
  if (flag & (1u << 6) && *flag_index != 0) {
    return Error("Bad glyph flag (%d), "
                 "bit 6 must be set to zero for flag %d", flag, *flag_index);
  }

  if (flag & (1u << 3)) {  // repeat
    if (*flag_index + 1 >= num_flags) {
      return Error("Count too high (%d + 1 >= %d)", *flag_index, num_flags);
    }
    uint8_t repeat = 0;
    if (!glyph.ReadU8(&repeat)) {
      return Error("Can't read repeat value");
    }
    if (repeat == 0) {
      return Error("Zero repeat");
    }
    delta += (delta * repeat);

    *flag_index += repeat;
    if (*flag_index >= num_flags) {
      return Error("Count too high (%d >= %d)", *flag_index, num_flags);
    }
  }

  if (flag & (1u << 7)) {  // reserved flag
    return Error("Bad glyph flag (%d), reserved bit 7 must be set to zero", flag);
  }

  *coordinates_length += delta;
  if (glyph.length() < *coordinates_length) {
    return Error("Glyph coordinates length bigger than glyph length (%d > %d)",
                 *coordinates_length, glyph.length());
  }

  return true;
}

bool OpenTypeGLYF::ParseSimpleGlyph(Buffer &glyph,
                                    int16_t num_contours) {
  // read the end-points array
  uint16_t num_flags = 0;
  for (int i = 0; i < num_contours; ++i) {
    uint16_t tmp_index = 0;
    if (!glyph.ReadU16(&tmp_index)) {
      return Error("Can't read contour index %d", i);
    }
    if (tmp_index == 0xffffu) {
      return Error("Bad contour index %d", i);
    }
    // check if the indices are monotonically increasing
    if (i && (tmp_index + 1 <= num_flags)) {
      return Error("Decreasing contour index %d + 1 <= %d", tmp_index, num_flags);
    }
    num_flags = tmp_index + 1;
  }

  uint16_t bytecode_length = 0;
  if (!glyph.ReadU16(&bytecode_length)) {
    return Error("Can't read bytecode length");
  }

  if (this->maxp->version_1 &&
      this->maxp->max_size_glyf_instructions < bytecode_length) {
    return Error("Bytecode length is bigger than maxp.maxSizeOfInstructions "
        "%d: %d", this->maxp->max_size_glyf_instructions, bytecode_length);
  }

  if (!glyph.Skip(bytecode_length)) {
    return Error("Can't read bytecode of length %d", bytecode_length);
  }

  uint32_t coordinates_length = 0;
  for (uint32_t i = 0; i < num_flags; ++i) {
    if (!ParseFlagsForSimpleGlyph(glyph, num_flags, &i, &coordinates_length)) {
      return Error("Failed to parse glyph flags %d", i);
    }
  }

  if (!glyph.Skip(coordinates_length)) {
    return Error("Glyph too short %d", glyph.length());
  }

  if (glyph.remaining() > 3) {
    // We allow 0-3 bytes difference since gly_length is 4-bytes aligned,
    // zero-padded length.
    Warning("Extra bytes at end of the glyph: %d", glyph.remaining());
  }

  this->iov.push_back(std::make_pair(glyph.buffer(), glyph.offset()));

  return true;
}

#define ARG_1_AND_2_ARE_WORDS    (1u << 0)
#define WE_HAVE_A_SCALE          (1u << 3)
#define MORE_COMPONENTS          (1u << 5)
#define WE_HAVE_AN_X_AND_Y_SCALE (1u << 6)
#define WE_HAVE_A_TWO_BY_TWO     (1u << 7)
#define WE_HAVE_INSTRUCTIONS     (1u << 8)

bool OpenTypeGLYF::ParseCompositeGlyph(Buffer &glyph) {
  uint16_t flags = 0;
  uint16_t gid = 0;
  do {
    if (!glyph.ReadU16(&flags) || !glyph.ReadU16(&gid)) {
      return Error("Can't read composite glyph flags or glyphIndex");
    }

    if (gid >= this->maxp->num_glyphs) {
      return Error("Invalid glyph id used in composite glyph: %d", gid);
    }

    if (flags & ARG_1_AND_2_ARE_WORDS) {
      int16_t argument1;
      int16_t argument2;
      if (!glyph.ReadS16(&argument1) || !glyph.ReadS16(&argument2)) {
        return Error("Can't read argument1 or argument2");
      }
    } else {
      uint8_t argument1;
      uint8_t argument2;
      if (!glyph.ReadU8(&argument1) || !glyph.ReadU8(&argument2)) {
        return Error("Can't read argument1 or argument2");
      }
    }

    if (flags & WE_HAVE_A_SCALE) {
      int16_t scale;
      if (!glyph.ReadS16(&scale)) {
        return Error("Can't read scale");
      }
    } else if (flags & WE_HAVE_AN_X_AND_Y_SCALE) {
      int16_t xscale;
      int16_t yscale;
      if (!glyph.ReadS16(&xscale) || !glyph.ReadS16(&yscale)) {
        return Error("Can't read xscale or yscale");
      }
    } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
      int16_t xscale;
      int16_t scale01;
      int16_t scale10;
      int16_t yscale;
      if (!glyph.ReadS16(&xscale) ||
          !glyph.ReadS16(&scale01) ||
          !glyph.ReadS16(&scale10) ||
          !glyph.ReadS16(&yscale)) {
        return Error("Can't read transform");
      }
    }
  } while (flags & MORE_COMPONENTS);

  if (flags & WE_HAVE_INSTRUCTIONS) {
    uint16_t bytecode_length;
    if (!glyph.ReadU16(&bytecode_length)) {
      return Error("Can't read instructions size");
    }

    if (this->maxp->version_1 &&
        this->maxp->max_size_glyf_instructions < bytecode_length) {
      return Error("Bytecode length is bigger than maxp.maxSizeOfInstructions "
                   "%d: %d",
                   this->maxp->max_size_glyf_instructions, bytecode_length);
    }

    if (!glyph.Skip(bytecode_length)) {
      return Error("Can't read bytecode of length %d", bytecode_length);
    }
  }

  this->iov.push_back(std::make_pair(glyph.buffer(), glyph.offset()));

  return true;
}

bool OpenTypeGLYF::Parse(const uint8_t *data, size_t length) {
  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  OpenTypeLOCA *loca = static_cast<OpenTypeLOCA*>(
      GetFont()->GetTypedTable(OTS_TAG_LOCA));
  OpenTypeHEAD *head = static_cast<OpenTypeHEAD*>(
      GetFont()->GetTypedTable(OTS_TAG_HEAD));
  if (!maxp || !loca || !head) {
    return Error("Missing maxp or loca or head table needed by glyf table");
  }

  this->maxp = maxp;

  const unsigned num_glyphs = maxp->num_glyphs;
  std::vector<uint32_t> &offsets = loca->offsets;

  if (offsets.size() != num_glyphs + 1) {
    return Error("Invalide glyph offsets size %ld != %d", offsets.size(), num_glyphs + 1);
  }

  std::vector<uint32_t> resulting_offsets(num_glyphs + 1);
  uint32_t current_offset = 0;

  for (unsigned i = 0; i < num_glyphs; ++i) {
    const unsigned gly_offset = offsets[i];
    // The LOCA parser checks that these values are monotonic
    const unsigned gly_length = offsets[i + 1] - offsets[i];
    if (!gly_length) {
      // this glyph has no outline (e.g. the space charactor)
      resulting_offsets[i] = current_offset;
      continue;
    }

    if (gly_offset >= length) {
      return Error("Glyph %d offset %d too high %ld", i, gly_offset, length);
    }
    // Since these are unsigned types, the compiler is not allowed to assume
    // that they never overflow.
    if (gly_offset + gly_length < gly_offset) {
      return Error("Glyph %d length (%d < 0)!", i, gly_length);
    }
    if (gly_offset + gly_length > length) {
      return Error("Glyph %d length %d too high", i, gly_length);
    }

    Buffer glyph(data + gly_offset, gly_length);

    int16_t num_contours, xmin, ymin, xmax, ymax;
    if (!glyph.ReadS16(&num_contours) ||
        !glyph.ReadS16(&xmin) ||
        !glyph.ReadS16(&ymin) ||
        !glyph.ReadS16(&xmax) ||
        !glyph.ReadS16(&ymax)) {
      return Error("Can't read glyph %d header", i);
    }

    if (num_contours <= -2) {
      // -2, -3, -4, ... are reserved for future use.
      return Error("Bad number of contours %d in glyph %d", num_contours, i);
    }

    // workaround for fonts in http://www.princexml.com/fonts/
    if ((xmin == 32767) &&
        (xmax == -32767) &&
        (ymin == 32767) &&
        (ymax == -32767)) {
      Warning("bad xmin/xmax/ymin/ymax values");
      xmin = xmax = ymin = ymax = 0;
    }

    if (xmin > xmax || ymin > ymax) {
      return Error("Bad bounding box values bl=(%d, %d), tr=(%d, %d) in glyph %d", xmin, ymin, xmax, ymax, i);
    }

    if (num_contours == 0) {
      // This is an empty glyph and shouldn’t have any glyph data, but if it
      // does we will simply ignore it.
      glyph.set_offset(0);
    } else if (num_contours > 0) {
      if (!ParseSimpleGlyph(glyph, num_contours)) {
        return Error("Failed to parse glyph %d", i);
      }
    } else {
      if (!ParseCompositeGlyph(glyph)) {
        return Error("Failed to parse glyph %d", i);
      }
    }

    size_t new_size = glyph.offset();
    resulting_offsets[i] = current_offset;
    // glyphs must be four byte aligned
    // TODO(yusukes): investigate whether this padding is really necessary.
    //                Which part of the spec requires this?
    const unsigned padding = (4 - (new_size & 3)) % 4;
    if (padding) {
      this->iov.push_back(std::make_pair(
          reinterpret_cast<const uint8_t*>("\x00\x00\x00\x00"),
          static_cast<size_t>(padding)));
      new_size += padding;
    }
    current_offset += new_size;
  }
  resulting_offsets[num_glyphs] = current_offset;

  const uint16_t max16 = std::numeric_limits<uint16_t>::max();
  if ((*std::max_element(resulting_offsets.begin(),
                         resulting_offsets.end()) >= (max16 * 2u)) &&
      (head->index_to_loc_format != 1)) {
    head->index_to_loc_format = 1;
  }

  loca->offsets = resulting_offsets;

  if (this->iov.empty()) {
    // As a special case when all glyph in the font are empty, add a zero byte
    // to the table, so that we don’t reject it down the way, and to make the
    // table work on Windows as well.
    // See https://github.com/khaledhosny/ots/issues/52
    static const uint8_t kZero = 0;
    this->iov.push_back(std::make_pair(&kZero, 1));
  }

  return true;
}

bool OpenTypeGLYF::Serialize(OTSStream *out) {
  for (unsigned i = 0; i < this->iov.size(); ++i) {
    if (!out->Write(this->iov[i].first, this->iov[i].second)) {
      return Error("Falied to write glyph %d", i);
    }
  }

  return true;
}

}  // namespace ots
