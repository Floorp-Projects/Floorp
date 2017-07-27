// Copyright (c) 2014-2017 The OTS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We use an underscore to avoid confusion with the standard math.h library.
#include "math_.h"

#include <limits>
#include <vector>

#include "layout.h"
#include "maxp.h"

// MATH - The MATH Table
// http://www.microsoft.com/typography/otspec/math.htm

namespace {

// The size of MATH header.
// Version
// MathConstants
// MathGlyphInfo
// MathVariants
const unsigned kMathHeaderSize = 4 + 3 * 2;

// The size of the MathGlyphInfo header.
// MathItalicsCorrectionInfo
// MathTopAccentAttachment
// ExtendedShapeCoverage
// MathKernInfo
const unsigned kMathGlyphInfoHeaderSize = 4 * 2;

// The size of the MathValueRecord.
// Value
// DeviceTable
const unsigned kMathValueRecordSize = 2 * 2;

// The size of the GlyphPartRecord.
// glyph
// StartConnectorLength
// EndConnectorLength
// FullAdvance
// PartFlags
const unsigned kGlyphPartRecordSize = 5 * 2;

}  // namespace

namespace ots {

// Shared Table: MathValueRecord

bool OpenTypeMATH::ParseMathValueRecord(ots::Buffer* subtable,
                                        const uint8_t *data,
                                        const size_t length) {
  // Check the Value field.
  if (!subtable->Skip(2)) {
    return OTS_FAILURE();
  }

  // Check the offset to device table.
  uint16_t offset = 0;
  if (!subtable->ReadU16(&offset)) {
    return OTS_FAILURE();
  }
  if (offset) {
    if (offset >= length) {
      return OTS_FAILURE();
    }
    if (!ots::ParseDeviceTable(GetFont(), data + offset, length - offset)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathConstantsTable(const uint8_t *data,
                                           size_t length) {
  ots::Buffer subtable(data, length);

  // Part 1: int16 or uint16 constants.
  //  ScriptPercentScaleDown
  //  ScriptScriptPercentScaleDown
  //  DelimitedSubFormulaMinHeight
  //  DisplayOperatorMinHeight
  if (!subtable.Skip(4 * 2)) {
    return OTS_FAILURE();
  }

  // Part 2: MathValueRecord constants.
  // MathLeading
  // AxisHeight
  // AccentBaseHeight
  // FlattenedAccentBaseHeight
  // SubscriptShiftDown
  // SubscriptTopMax
  // SubscriptBaselineDropMin
  // SuperscriptShiftUp
  // SuperscriptShiftUpCramped
  // SuperscriptBottomMin
  //
  // SuperscriptBaselineDropMax
  // SubSuperscriptGapMin
  // SuperscriptBottomMaxWithSubscript
  // SpaceAfterScript
  // UpperLimitGapMin
  // UpperLimitBaselineRiseMin
  // LowerLimitGapMin
  // LowerLimitBaselineDropMin
  // StackTopShiftUp
  // StackTopDisplayStyleShiftUp
  //
  // StackBottomShiftDown
  // StackBottomDisplayStyleShiftDown
  // StackGapMin
  // StackDisplayStyleGapMin
  // StretchStackTopShiftUp
  // StretchStackBottomShiftDown
  // StretchStackGapAboveMin
  // StretchStackGapBelowMin
  // FractionNumeratorShiftUp
  // FractionNumeratorDisplayStyleShiftUp
  //
  // FractionDenominatorShiftDown
  // FractionDenominatorDisplayStyleShiftDown
  // FractionNumeratorGapMin
  // FractionNumDisplayStyleGapMin
  // FractionRuleThickness
  // FractionDenominatorGapMin
  // FractionDenomDisplayStyleGapMin
  // SkewedFractionHorizontalGap
  // SkewedFractionVerticalGap
  // OverbarVerticalGap
  //
  // OverbarRuleThickness
  // OverbarExtraAscender
  // UnderbarVerticalGap
  // UnderbarRuleThickness
  // UnderbarExtraDescender
  // RadicalVerticalGap
  // RadicalDisplayStyleVerticalGap
  // RadicalRuleThickness
  // RadicalExtraAscender
  // RadicalKernBeforeDegree
  //
  // RadicalKernAfterDegree
  for (unsigned i = 0; i < static_cast<unsigned>(51); ++i) {
    if (!ParseMathValueRecord(&subtable, data, length)) {
      return OTS_FAILURE();
    }
  }

  // Part 3: uint16 constant
  // RadicalDegreeBottomRaisePercent
  if (!subtable.Skip(2)) {
    return OTS_FAILURE();
  }

  return true;
}

bool OpenTypeMATH::ParseMathValueRecordSequenceForGlyphs(ots::Buffer* subtable,
                                                         const uint8_t *data,
                                                         const size_t length,
                                                         const uint16_t num_glyphs) {
  // Check the header.
  uint16_t offset_coverage = 0;
  uint16_t sequence_count = 0;
  if (!subtable->ReadU16(&offset_coverage) ||
      !subtable->ReadU16(&sequence_count)) {
    return OTS_FAILURE();
  }

  const unsigned sequence_end = static_cast<unsigned>(2 * 2) +
      sequence_count * kMathValueRecordSize;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  // Check coverage table.
  if (offset_coverage < sequence_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage,
                               length - offset_coverage,
                               num_glyphs, sequence_count)) {
    return OTS_FAILURE();
  }

  // Check sequence.
  for (unsigned i = 0; i < sequence_count; ++i) {
    if (!ParseMathValueRecord(subtable, data, length)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathItalicsCorrectionInfoTable(const uint8_t *data,
                                                       size_t length,
                                                       const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);
  return ParseMathValueRecordSequenceForGlyphs(&subtable, data, length,
                                               num_glyphs);
}

bool OpenTypeMATH::ParseMathTopAccentAttachmentTable(const uint8_t *data,
                                                     size_t length,
                                                     const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);
  return ParseMathValueRecordSequenceForGlyphs(&subtable, data, length,
                                               num_glyphs);
}

bool OpenTypeMATH::ParseMathKernTable(const uint8_t *data, size_t length) {
  ots::Buffer subtable(data, length);

  // Check the Height count.
  uint16_t height_count = 0;
  if (!subtable.ReadU16(&height_count)) {
    return OTS_FAILURE();
  }

  // Check the Correction Heights.
  for (unsigned i = 0; i < height_count; ++i) {
    if (!ParseMathValueRecord(&subtable, data, length)) {
      return OTS_FAILURE();
    }
  }

  // Check the Kern Values.
  for (unsigned i = 0; i <= height_count; ++i) {
    if (!ParseMathValueRecord(&subtable, data, length)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathKernInfoTable(const uint8_t *data,
                                          size_t length,
                                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Check the header.
  uint16_t offset_coverage = 0;
  uint16_t sequence_count = 0;
  if (!subtable.ReadU16(&offset_coverage) ||
      !subtable.ReadU16(&sequence_count)) {
    return OTS_FAILURE();
  }

  const unsigned sequence_end = static_cast<unsigned>(2 * 2) +
    sequence_count * 4 * 2;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  // Check coverage table.
  if (offset_coverage < sequence_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage, length - offset_coverage,
                               num_glyphs, sequence_count)) {
    return OTS_FAILURE();
  }

  // Check sequence of MathKernInfoRecord
  for (unsigned i = 0; i < sequence_count; ++i) {
    // Check TopRight, TopLeft, BottomRight and BottomLeft Math Kern.
    for (unsigned j = 0; j < 4; ++j) {
      uint16_t offset_math_kern = 0;
      if (!subtable.ReadU16(&offset_math_kern)) {
        return OTS_FAILURE();
      }
      if (offset_math_kern) {
        if (offset_math_kern < sequence_end || offset_math_kern >= length ||
            !ParseMathKernTable(data + offset_math_kern,
                                length - offset_math_kern)) {
          return OTS_FAILURE();
        }
      }
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathGlyphInfoTable(const uint8_t *data,
                                           size_t length,
                                           const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Check Header.
  uint16_t offset_math_italics_correction_info = 0;
  uint16_t offset_math_top_accent_attachment = 0;
  uint16_t offset_extended_shaped_coverage = 0;
  uint16_t offset_math_kern_info = 0;
  if (!subtable.ReadU16(&offset_math_italics_correction_info) ||
      !subtable.ReadU16(&offset_math_top_accent_attachment) ||
      !subtable.ReadU16(&offset_extended_shaped_coverage) ||
      !subtable.ReadU16(&offset_math_kern_info)) {
    return OTS_FAILURE();
  }

  // Check subtables.
  // The specification does not say whether the offsets for
  // MathItalicsCorrectionInfo, MathTopAccentAttachment and MathKernInfo may
  // be NULL, but that's the case in some fonts (e.g STIX) so we accept that.
  if (offset_math_italics_correction_info) {
    if (offset_math_italics_correction_info >= length ||
        offset_math_italics_correction_info < kMathGlyphInfoHeaderSize ||
        !ParseMathItalicsCorrectionInfoTable(
            data + offset_math_italics_correction_info,
            length - offset_math_italics_correction_info,
            num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  if (offset_math_top_accent_attachment) {
    if (offset_math_top_accent_attachment >= length ||
        offset_math_top_accent_attachment < kMathGlyphInfoHeaderSize ||
        !ParseMathTopAccentAttachmentTable(data +
                                           offset_math_top_accent_attachment,
                                           length -
                                           offset_math_top_accent_attachment,
                                           num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  if (offset_extended_shaped_coverage) {
    if (offset_extended_shaped_coverage >= length ||
        offset_extended_shaped_coverage < kMathGlyphInfoHeaderSize ||
        !ots::ParseCoverageTable(GetFont(), data + offset_extended_shaped_coverage,
                                 length - offset_extended_shaped_coverage,
                                 num_glyphs)) {
      return OTS_FAILURE();
    }
  }
  if (offset_math_kern_info) {
    if (offset_math_kern_info >= length ||
        offset_math_kern_info < kMathGlyphInfoHeaderSize ||
        !ParseMathKernInfoTable(data + offset_math_kern_info,
                                length - offset_math_kern_info, num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  return true;
}

bool OpenTypeMATH::ParseGlyphAssemblyTable(const uint8_t *data,
                                           size_t length,
                                           const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Check the header.
  uint16_t part_count = 0;
  if (!ParseMathValueRecord(&subtable, data, length) ||
      !subtable.ReadU16(&part_count)) {
    return OTS_FAILURE();
  }

  const unsigned sequence_end = kMathValueRecordSize +
    static_cast<unsigned>(2) + part_count * kGlyphPartRecordSize;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  // Check the sequence of GlyphPartRecord.
  for (unsigned i = 0; i < part_count; ++i) {
    uint16_t glyph = 0;
    uint16_t part_flags = 0;
    if (!subtable.ReadU16(&glyph) ||
        !subtable.Skip(2 * 3) ||
        !subtable.ReadU16(&part_flags)) {
      return OTS_FAILURE();
    }
    if (glyph >= num_glyphs) {
      return Error("bad glyph ID: %u", glyph);
    }
    if (part_flags & ~0x00000001) {
      return Error("unknown part flag: %u", part_flags);
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathGlyphConstructionTable(const uint8_t *data,
                                                   size_t length,
                                                   const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Check the header.
  uint16_t offset_glyph_assembly = 0;
  uint16_t variant_count = 0;
  if (!subtable.ReadU16(&offset_glyph_assembly) ||
      !subtable.ReadU16(&variant_count)) {
    return OTS_FAILURE();
  }

  const unsigned sequence_end = static_cast<unsigned>(2 * 2) +
    variant_count * 2 * 2;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  // Check the GlyphAssembly offset.
  if (offset_glyph_assembly) {
    if (offset_glyph_assembly >= length ||
        offset_glyph_assembly < sequence_end) {
      return OTS_FAILURE();
    }
    if (!ParseGlyphAssemblyTable(data + offset_glyph_assembly,
                                 length - offset_glyph_assembly, num_glyphs)) {
      return OTS_FAILURE();
    }
  }

  // Check the sequence of MathGlyphVariantRecord.
  for (unsigned i = 0; i < variant_count; ++i) {
    uint16_t glyph = 0;
    if (!subtable.ReadU16(&glyph) ||
        !subtable.Skip(2)) {
      return OTS_FAILURE();
    }
    if (glyph >= num_glyphs) {
      return Error("bad glyph ID: %u", glyph);
    }
  }

  return true;
}

bool OpenTypeMATH::ParseMathGlyphConstructionSequence(ots::Buffer* subtable,
                                                      const uint8_t *data,
                                                      size_t length,
                                                      const uint16_t num_glyphs,
                                                      uint16_t offset_coverage,
                                                      uint16_t glyph_count,
                                                      const unsigned sequence_end) {
  // Zero glyph count, nothing to parse.
  if (!glyph_count) {
    return true;
  }

  // Check coverage table.
  if (offset_coverage < sequence_end || offset_coverage >= length) {
    return OTS_FAILURE();
  }
  if (!ots::ParseCoverageTable(GetFont(), data + offset_coverage,
                               length - offset_coverage,
                               num_glyphs, glyph_count)) {
    return OTS_FAILURE();
  }

  // Check sequence of MathGlyphConstruction.
  for (unsigned i = 0; i < glyph_count; ++i) {
      uint16_t offset_glyph_construction = 0;
      if (!subtable->ReadU16(&offset_glyph_construction)) {
        return OTS_FAILURE();
      }
      if (offset_glyph_construction < sequence_end ||
          offset_glyph_construction >= length ||
          !ParseMathGlyphConstructionTable(data + offset_glyph_construction,
                                           length - offset_glyph_construction,
                                           num_glyphs)) {
        return OTS_FAILURE();
      }
  }

  return true;
}

bool OpenTypeMATH::ParseMathVariantsTable(const uint8_t *data,
                                          size_t length,
                                          const uint16_t num_glyphs) {
  ots::Buffer subtable(data, length);

  // Check the header.
  uint16_t offset_vert_glyph_coverage = 0;
  uint16_t offset_horiz_glyph_coverage = 0;
  uint16_t vert_glyph_count = 0;
  uint16_t horiz_glyph_count = 0;
  if (!subtable.Skip(2) ||  // MinConnectorOverlap
      !subtable.ReadU16(&offset_vert_glyph_coverage) ||
      !subtable.ReadU16(&offset_horiz_glyph_coverage) ||
      !subtable.ReadU16(&vert_glyph_count) ||
      !subtable.ReadU16(&horiz_glyph_count)) {
    return OTS_FAILURE();
  }

  const unsigned sequence_end = 5 * 2 + vert_glyph_count * 2 +
    horiz_glyph_count * 2;
  if (sequence_end > std::numeric_limits<uint16_t>::max()) {
    return OTS_FAILURE();
  }

  if (!ParseMathGlyphConstructionSequence(&subtable, data, length, num_glyphs,
                                          offset_vert_glyph_coverage,
                                          vert_glyph_count,
                                          sequence_end) ||
      !ParseMathGlyphConstructionSequence(&subtable, data, length, num_glyphs,
                                          offset_horiz_glyph_coverage,
                                          horiz_glyph_count,
                                          sequence_end)) {
    return OTS_FAILURE();
  }

  return true;
}

bool OpenTypeMATH::Parse(const uint8_t *data, size_t length) {
  // Grab the number of glyphs in the font from the maxp table to check
  // GlyphIDs in MATH table.
  OpenTypeMAXP *maxp = static_cast<OpenTypeMAXP*>(
      GetFont()->GetTypedTable(OTS_TAG_MAXP));
  if (!maxp) {
    return Error("Required maxp table missing");
  }
  const uint16_t num_glyphs = maxp->num_glyphs;

  Buffer table(data, length);

  uint32_t version = 0;
  if (!table.ReadU32(&version)) {
    return OTS_FAILURE();
  }
  if (version != 0x00010000) {
    return Drop("bad MATH version");
  }

  uint16_t offset_math_constants = 0;
  uint16_t offset_math_glyph_info = 0;
  uint16_t offset_math_variants = 0;
  if (!table.ReadU16(&offset_math_constants) ||
      !table.ReadU16(&offset_math_glyph_info) ||
      !table.ReadU16(&offset_math_variants)) {
    return OTS_FAILURE();
  }

  if (offset_math_constants >= length ||
      offset_math_constants < kMathHeaderSize ||
      offset_math_glyph_info >= length ||
      offset_math_glyph_info < kMathHeaderSize ||
      offset_math_variants >= length ||
      offset_math_variants < kMathHeaderSize) {
    return Drop("bad offset in MATH header");
  }

  if (!ParseMathConstantsTable(data + offset_math_constants,
                               length - offset_math_constants)) {
    return Drop("failed to parse MathConstants table");
  }
  if (!ParseMathGlyphInfoTable(data + offset_math_glyph_info,
                               length - offset_math_glyph_info, num_glyphs)) {
    return Drop("failed to parse MathGlyphInfo table");
  }
  if (!ParseMathVariantsTable(data + offset_math_variants,
                              length - offset_math_variants, num_glyphs)) {
    return Drop("failed to parse MathVariants table");
  }

  this->m_data = data;
  this->m_length = length;
  return true;
}

bool OpenTypeMATH::Serialize(OTSStream *out) {
  if (!out->Write(this->m_data, this->m_length)) {
    return OTS_FAILURE();
  }

  return true;
}

bool OpenTypeMATH::ShouldSerialize() {
  return Table::ShouldSerialize() && this->m_data != NULL;
}

}  // namespace ots
