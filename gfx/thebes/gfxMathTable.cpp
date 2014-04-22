/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMathTable.h"

#include "MathTableStructures.h"
#include "harfbuzz/hb.h"
#include <algorithm>

using namespace mozilla;

gfxMathTable::gfxMathTable(hb_blob_t* aMathTable)
  : mMathTable(aMathTable)
  , mGlyphConstruction(nullptr)
  , mGlyphID(-1)
  , mVertical(false)
{
}

gfxMathTable::~gfxMathTable()
{
  hb_blob_destroy(mMathTable);
}

bool
gfxMathTable::HasValidHeaders()
{
  const char* mathData = hb_blob_get_data(mMathTable, nullptr);
  // Verify the MATH table header.
  if (!ValidStructure(mathData, sizeof(MATHTableHeader))) {
    return false;
  }
  const MATHTableHeader* header = GetMATHTableHeader();
  if (uint32_t(header->mVersion) != 0x00010000 ||
      !ValidOffset(mathData, uint16_t(header->mMathConstants)) ||
      !ValidOffset(mathData, uint16_t(header->mMathGlyphInfo)) ||
      !ValidOffset(mathData, uint16_t(header->mMathVariants))) {
    return false;
  }

  // Verify the MathConstants header.
  const MathConstants* mathconstants = GetMathConstants();
  const char* start = reinterpret_cast<const char*>(mathconstants);
  if (!ValidStructure(start, sizeof(MathConstants))) {
    return false;
  }

  // Verify the MathGlyphInfo header.
  const MathGlyphInfo* mathglyphinfo = GetMathGlyphInfo();
  start = reinterpret_cast<const char*>(mathglyphinfo);
  if (!ValidStructure(start, sizeof(MathGlyphInfo))) {
    return false;
  }

  // Verify the MathVariants header.
  const MathVariants* mathvariants = GetMathVariants();
  start = reinterpret_cast<const char*>(mathvariants);
  if (!ValidStructure(start, sizeof(MathVariants)) ||
      !ValidStructure(start,
                      sizeof(MathVariants) + sizeof(Offset) *
                      (uint16_t(mathvariants->mVertGlyphCount) +
                       uint16_t(mathvariants->mHorizGlyphCount))) ||
      !ValidOffset(start, uint16_t(mathvariants->mVertGlyphCoverage)) ||
      !ValidOffset(start, uint16_t(mathvariants->mHorizGlyphCoverage))) {
    return false;
  }

  return true;
}

int32_t
gfxMathTable::GetMathConstant(gfxFontEntry::MathConstant aConstant)
{
  const MathConstants* mathconstants = GetMathConstants();

  if (aConstant <= gfxFontEntry::ScriptScriptPercentScaleDown) {
    return int16_t(mathconstants->mInt16[aConstant]);
  }

  if (aConstant <= gfxFontEntry::DisplayOperatorMinHeight) {
    return
      uint16_t(mathconstants->
               mUint16[aConstant - gfxFontEntry::DelimitedSubFormulaMinHeight]);
  }

  if (aConstant <= gfxFontEntry::RadicalKernAfterDegree) {
    return int16_t(mathconstants->
                   mMathValues[aConstant - gfxFontEntry::MathLeading].mValue);
  }

  return uint16_t(mathconstants->mRadicalDegreeBottomRaisePercent);
}

bool
gfxMathTable::GetMathItalicsCorrection(uint32_t aGlyphID,
                                       int16_t* aItalicCorrection)
{
  const MathGlyphInfo* mathglyphinfo = GetMathGlyphInfo();

  // Get the offset of the italic correction and verify whether it is valid.
  const char* start = reinterpret_cast<const char*>(mathglyphinfo);
  uint16_t offset = mathglyphinfo->mMathItalicsCorrectionInfo;
  if (offset == 0 || !ValidOffset(start, offset)) {
    return false;
  }
  start += offset;

  // Verify the validity of the MathItalicsCorrectionInfo and retrieve it.
  if (!ValidStructure(start, sizeof(MathItalicsCorrectionInfo))) {
    return false;
  }
  const MathItalicsCorrectionInfo* italicsCorrectionInfo =
    reinterpret_cast<const MathItalicsCorrectionInfo*>(start);

  // Get the coverage index for the glyph.
  offset = italicsCorrectionInfo->mCoverage;
  const Coverage* coverage =
    reinterpret_cast<const Coverage*>(start + offset);
  int32_t i = GetCoverageIndex(coverage, aGlyphID);

  // Get the ItalicsCorrection.
  uint16_t count = italicsCorrectionInfo->mItalicsCorrectionCount;
  if (i < 0 || i >= count) {
    return false;
  }
  start = reinterpret_cast<const char*>(italicsCorrectionInfo + 1);
  if (!ValidStructure(start, count * sizeof(MathValueRecord))) {
    return false;
  }
  const MathValueRecord* mathValueRecordArray =
    reinterpret_cast<const MathValueRecord*>(start);

  *aItalicCorrection = int16_t(mathValueRecordArray[i].mValue);
  return true;
}

uint32_t
gfxMathTable::GetMathVariantsSize(uint32_t aGlyphID, bool aVertical,
                                  uint16_t aSize)
{
  // Select the glyph construction.
  SelectGlyphConstruction(aGlyphID, aVertical);
  if (!mGlyphConstruction) {
    return 0;
  }

  // Verify the validity of the array of the MathGlyphVariantRecord's and
  // whether there is a variant of the requested size.
  uint16_t count = mGlyphConstruction->mVariantCount;
  const char* start = reinterpret_cast<const char*>(mGlyphConstruction + 1);
  if (aSize >= count ||
      !ValidStructure(start, count * sizeof(MathGlyphVariantRecord))) {
    return 0;
  }

  // Return the glyph index of the requested size variant.
  const MathGlyphVariantRecord* recordArray =
    reinterpret_cast<const MathGlyphVariantRecord*>(start);
  return uint32_t(recordArray[aSize].mVariantGlyph);
}

bool
gfxMathTable::GetMathVariantsParts(uint32_t aGlyphID, bool aVertical,
                                   uint32_t aGlyphs[4])
{
  // Get the glyph assembly corresponding to that (aGlyphID, aVertical) pair.
  const GlyphAssembly* glyphAssembly = GetGlyphAssembly(aGlyphID, aVertical);
  if (!glyphAssembly) {
    return false;
  }

  // Verify the validity of the array of GlyphPartRecord's and retrieve it.
  uint16_t count = glyphAssembly->mPartCount;
  const char* start = reinterpret_cast<const char*>(glyphAssembly + 1);
  if (!ValidStructure(start, count * sizeof(GlyphPartRecord))) {
    return false;
  }
  const GlyphPartRecord* recordArray =
    reinterpret_cast<const GlyphPartRecord*>(start);

  // XXXfredw The structure of the Open Type Math table is a bit more general
  // than the one currently used by the nsMathMLChar code, so we try to fallback
  // in reasonable way. We use the approach of the copyComponents function in
  // github.com/mathjax/MathJax-dev/blob/master/fonts/OpenTypeMath/fontUtil.py
  //
  // The nsMathMLChar code can use at most 3 non extender pieces (aGlyphs[0],
  // aGlyphs[1] and aGlyphs[2]) and the extenders between these pieces should
  // all be the same (aGlyphs[4]). Also, the parts of vertical assembly are
  // stored from bottom to top in the Open Type MATH table while they are
  // stored from top to bottom in nsMathMLChar.

  // Count the number of non extender pieces
  uint16_t nonExtenderCount = 0;
  for (uint16_t i = 0; i < count; i++) {
    if (!(uint16_t(recordArray[i].mPartFlags) & PART_FLAG_EXTENDER)) {
      nonExtenderCount++;
    }
  }
  if (nonExtenderCount > 3) {
    // Not supported: too many pieces
    return false;
  }

  // Now browse the list of pieces

  // 0 = look for a left/bottom glyph
  // 1 = look for an extender between left/bottom and mid
  // 2 = look for a middle glyph
  // 3 = look for an extender between middle and right/top
  // 4 = look for a right/top glyph
  // 5 = no more piece expected
  uint8_t state = 0;

  // First extender char found.
  uint32_t extenderChar = 0;

  // Clear the aGlyphs table.
  memset(aGlyphs, 0, sizeof(uint32_t) * 4);

  for (uint16_t i = 0; i < count; i++) {

    bool isExtender = uint16_t(recordArray[i].mPartFlags) & PART_FLAG_EXTENDER;
    uint32_t glyph = recordArray[i].mGlyph;

    if ((state == 1 || state == 2) && nonExtenderCount < 3) {
      // do not try to find a middle glyph
      state += 2;
    }

    if (isExtender) {
      if (!extenderChar) {
        extenderChar = glyph;
        aGlyphs[3] = extenderChar;
      } else if (extenderChar != glyph)  {
        // Not supported: different extenders
        return false;
      }

      if (state == 0) { // or state == 1
        // ignore left/bottom piece and multiple successive extenders
        state = 1;
      } else if (state == 2) { // or state == 3
        // ignore middle piece and multiple successive extenders
        state = 3;
      } else if (state >= 4) {
        // Not supported: unexpected extender
        return false;
      }

      continue;
    }

    if (state == 0) {
      // copy left/bottom part
      aGlyphs[mVertical ? 2 : 0] = glyph;
      state = 1;
      continue;
    }

    if (state == 1 || state == 2) {
      // copy middle part
      aGlyphs[1] = glyph;
      state = 3;
      continue;
    }

    if (state == 3 || state == 4) {
      // copy right/top part
      aGlyphs[mVertical ? 0 : 2] = glyph;
      state = 5;
    }

  }

  return true;
}

bool
gfxMathTable::ValidStructure(const char* aStart, uint16_t aSize)
{
  unsigned int mathDataLength;
  const char* mathData = hb_blob_get_data(mMathTable, &mathDataLength);
  return (mathData <= aStart &&
          aStart + aSize <= mathData + mathDataLength);
}

bool
gfxMathTable::ValidOffset(const char* aStart, uint16_t aOffset)
{
  unsigned int mathDataLength;
  const char* mathData = hb_blob_get_data(mMathTable, &mathDataLength);
  return (mathData <= aStart + aOffset &&
          aStart + aOffset < mathData + mathDataLength);
}

const MATHTableHeader*
gfxMathTable::GetMATHTableHeader()
{
  const char* mathData = hb_blob_get_data(mMathTable, nullptr);
  return reinterpret_cast<const MATHTableHeader*>(mathData);
}

const MathConstants*
gfxMathTable::GetMathConstants()
{
  const char* mathData = hb_blob_get_data(mMathTable, nullptr);
  return
    reinterpret_cast<const MathConstants*>(mathData +
                                           uint16_t(GetMATHTableHeader()->
                                                    mMathConstants));
}

const MathGlyphInfo*
gfxMathTable::GetMathGlyphInfo()
{
  const char* mathData = hb_blob_get_data(mMathTable, nullptr);
  return
    reinterpret_cast<const MathGlyphInfo*>(mathData +
                                           uint16_t(GetMATHTableHeader()->
                                                    mMathGlyphInfo));
}

const MathVariants*
gfxMathTable::GetMathVariants()
{
  const char* mathData = hb_blob_get_data(mMathTable, nullptr);
  return
    reinterpret_cast<const MathVariants*>(mathData +
                                          uint16_t(GetMATHTableHeader()->
                                                   mMathVariants));
}

const GlyphAssembly*
gfxMathTable::GetGlyphAssembly(uint32_t aGlyphID, bool aVertical)
{
  // Select the glyph construction.
  SelectGlyphConstruction(aGlyphID, aVertical);
  if (!mGlyphConstruction) {
    return nullptr;
  }

  // Get the offset of the glyph assembly and verify whether it is valid.
  const char* start = reinterpret_cast<const char*>(mGlyphConstruction);
  uint16_t offset = mGlyphConstruction->mGlyphAssembly;
  if (offset == 0 || !ValidOffset(start, offset)) {
    return nullptr;
  }
  start += offset;

  // Verify the validity of the GlyphAssembly and return it.
  if (!ValidStructure(start, sizeof(GlyphAssembly))) {
    return nullptr;
  }
  return reinterpret_cast<const GlyphAssembly*>(start);
}

int32_t
gfxMathTable::GetCoverageIndex(const Coverage* aCoverage, uint32_t aGlyph)
{
  if (uint16_t(aCoverage->mFormat) == 1) {
    // Coverage Format 1: list of individual glyph indices in the glyph set.
    const CoverageFormat1* table =
      reinterpret_cast<const CoverageFormat1*>(aCoverage);
    uint16_t count = table->mGlyphCount;
    const char* start = reinterpret_cast<const char*>(table + 1);
    if (ValidStructure(start, count * sizeof(GlyphID))) {
      const GlyphID* glyphArray =
        reinterpret_cast<const GlyphID*>(start);
      uint32_t imin = 0, imax = count;
      while (imin < imax) {
        uint32_t imid = (imin + imax) >> 1;
        uint16_t glyphMid = glyphArray[imid];
        if (glyphMid == aGlyph) {
          return imid;
        }
        if (glyphMid < aGlyph) {
          imin = imid + 1;
        } else {
          imax = imid;
        }
      }
    }
  } else if (uint16_t(aCoverage->mFormat) == 2) {
    // Coverage Format 2: ranges of consecutive indices.
    const CoverageFormat2* table =
      reinterpret_cast<const CoverageFormat2*>(aCoverage);
    uint16_t count = table->mRangeCount;
    const char* start = reinterpret_cast<const char*>(table + 1);
    if (ValidStructure(start, count * sizeof(RangeRecord))) {
      const RangeRecord* rangeArray =
        reinterpret_cast<const RangeRecord*>(start);
      uint32_t imin = 0, imax = count;
      while (imin < imax) {
        uint32_t imid = (imin + imax) >> 1;
        uint16_t rStart = rangeArray[imid].mStart;
        uint16_t rEnd = rangeArray[imid].mEnd;
        if (rEnd < aGlyph) {
          imin = imid + 1;
        } else if (aGlyph < rStart) {
          imax = imid;
        } else {
          return (uint16_t(rangeArray[imid].mStartCoverageIndex) +
                  aGlyph - rStart);
        }
      }
    }
  }
  return -1;
}

void
gfxMathTable::SelectGlyphConstruction(uint32_t aGlyphID, bool aVertical)
{
  if (mGlyphID == aGlyphID && mVertical == aVertical) {
    // The (glyph, direction) pair is already selected: nothing to do.
    return;
  }

  // Update our cached values.
  mVertical = aVertical;
  mGlyphID = aGlyphID;
  mGlyphConstruction = nullptr;

  // Get the coverage index for the new values.
  const MathVariants* mathvariants = GetMathVariants();
  const char* start = reinterpret_cast<const char*>(mathvariants);
  uint16_t offset = (aVertical ?
                     mathvariants->mVertGlyphCoverage :
                     mathvariants->mHorizGlyphCoverage);
  const Coverage* coverage =
    reinterpret_cast<const Coverage*>(start + offset);
  int32_t i = GetCoverageIndex(coverage, aGlyphID);

  // Get the offset to the glyph construction.
  uint16_t count = (aVertical ?
                    mathvariants->mVertGlyphCount :
                    mathvariants->mHorizGlyphCount);
  start = reinterpret_cast<const char*>(mathvariants + 1);
  if (i < 0 || i >= count) {
    return;
  }
  if (!aVertical) {
    start += uint16_t(mathvariants->mVertGlyphCount) * sizeof(Offset);
  }
  if (!ValidStructure(start, count * sizeof(Offset))) {
    return;
  }
  const Offset* offsetArray = reinterpret_cast<const Offset*>(start);
  offset = uint16_t(offsetArray[i]);

  // Make mGlyphConstruction point to the desired glyph construction.
  start = reinterpret_cast<const char*>(mathvariants);
  if (!ValidStructure(start + offset, sizeof(MathGlyphConstruction))) {
    return;
  }
  mGlyphConstruction =
    reinterpret_cast<const MathGlyphConstruction*>(start + offset);
}
