/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MATH_TABLE_H
#define GFX_MATH_TABLE_H

#include "gfxFont.h"

struct Coverage;
struct GlyphAssembly;
struct MATHTableHeader;
struct MathConstants;
struct MathGlyphConstruction;
struct MathGlyphInfo;
struct MathVariants;

/**
 * Used by |gfxFontEntry| to represent the MATH table of an OpenType font.
 * Each |gfxFontEntry| owns at most one |gfxMathTable| instance.
 */
class gfxMathTable
{
public:
    /**
     * @param aMathTable The MATH table from the OpenType font
     *
     * The gfxMathTable object takes over ownership of the blob references
     * that are passed in, and will hb_blob_destroy() them when finished;
     * the caller should -not- destroy this reference.
     */
    gfxMathTable(hb_blob_t* aMathTable);

    /**
     * Releases our reference to the MATH table and cleans up everything else.
     */
    ~gfxMathTable();

    /**
     * Returns the value of the specified constant from the MATH table.
     */
    int32_t GetMathConstant(gfxFontEntry::MathConstant aConstant);

    /**
     *  If the MATH table contains an italic correction for that glyph, this
     *  function gets the value and returns true. Otherwise it returns false.
     */
    bool
    GetMathItalicsCorrection(uint32_t aGlyphID, int16_t* aItalicCorrection);

    /**
     * @param aGlyphID  glyph index of the character we want to stretch
     * @param aVertical direction of the stretching (vertical/horizontal)
     * @param aSize     the desired size variant
     *
     * Returns the glyph index of the desired size variant or 0 if there is not
     * any such size variant.
     */
    uint32_t GetMathVariantsSize(uint32_t aGlyphID, bool aVertical,
                                 uint16_t aSize);

    /**
     * @param aGlyphID  glyph index of the character we want to stretch
     * @param aVertical direction of the stretching (vertical/horizontal)
     * @param aGlyphs   pre-allocated buffer of 4 elements where the glyph
     * indexes (or 0 for absent parts) will be stored. The parts are stored in
     * the order expected by the nsMathMLChar: Top (or Left), Middle, Bottom
     * (or Right), Glue.
     *
     * Tries to fill-in aGlyphs with the relevant glyph indexes and returns
     * whether the operation was successful. The function returns false if
     * there is not any assembly for the character we want to stretch or if
     * the format is not supported by the nsMathMLChar code.
     *
     */
    bool GetMathVariantsParts(uint32_t aGlyphID, bool aVertical,
                              uint32_t aGlyphs[4]);

protected:
    friend class gfxFontEntry;
    // This allows gfxFontEntry to verify the validity of the main headers
    // before starting to use the MATH table.
    bool HasValidHeaders();

private:
    // HarfBuzz blob where the MATH table is stored.
    hb_blob_t*    mMathTable;

    // Cached values for the latest (mGlyphID, mVertical) pair that has been
    // accessed and the corresponding glyph construction. These are verified
    // by SelectGlyphConstruction and updated if necessary.
    // mGlyphConstruction will be set to nullptr if no construction is defined
    // for the glyph. If non-null, its mGlyphAssembly and mVariantCount fields
    // may be safely read, but no further validation will have been done.
    const MathGlyphConstruction* mGlyphConstruction;
    uint32_t mGlyphID;
    bool     mVertical;
    void     SelectGlyphConstruction(uint32_t aGlyphID, bool aVertical);

    // Access to some structures of the MATH table.
    // These accessors just return a pointer, but do NOT themselves check the
    // validity of anything. Until we've checked that HasValidHeaders (which
    // does validate them) returns true, they might return pointers that cannot
    // even safely be dereferenced. GetGlyphAssembly may return nullptr if the
    // given glyph has no assembly defined.
    const MATHTableHeader* GetMATHTableHeader();
    const MathConstants*   GetMathConstants();
    const MathGlyphInfo*   GetMathGlyphInfo();
    const MathVariants*    GetMathVariants();
    const GlyphAssembly*   GetGlyphAssembly(uint32_t aGlyphID, bool aVertical);

    // Verify whether a structure or an offset belongs to the math data and can
    // be read safely.
    bool ValidStructure(const char* aStructStart, uint16_t aStructSize);
    bool ValidOffset(const char* aOffsetStart, uint16_t aOffset);

    // Get the coverage index of a glyph index from an Open Type coverage table
    // or -1 if the glyph index is not found.
    int32_t GetCoverageIndex(const Coverage* aCoverage, uint32_t aGlyph);
};

#endif
