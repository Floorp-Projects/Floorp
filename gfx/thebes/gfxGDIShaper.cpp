/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define FORCE_PR_LOG

#include "gfxGDIShaper.h"

/**********************************************************************
 *
 * class gfxGDIShaper
 *
 **********************************************************************/

bool
gfxGDIShaper::ShapeText(gfxContext      *aContext,
                        const char16_t *aText,
                        uint32_t         aOffset,
                        uint32_t         aLength,
                        int32_t          aScript,
                        gfxShapedText   *aShapedText)
{
    DCFromContext dc(aContext);
    AutoSelectFont selectFont(dc, static_cast<gfxGDIFont*>(mFont)->GetHFONT());

    uint32_t length = aLength;
    AutoFallibleTArray<WORD,500> glyphArray;
    if (!glyphArray.SetLength(length)) {
        return false;
    }
    WORD *glyphs = glyphArray.Elements();

    DWORD ret = ::GetGlyphIndicesW(dc, char16ptr_t(aText), length,
                                   glyphs, GGI_MARK_NONEXISTING_GLYPHS);
    if (ret == GDI_ERROR) {
        return false;
    }

    for (int k = 0; k < length; k++) {
        if (glyphs[k] == 0xFFFF)
            return false;
    }
 
    SIZE size;
    AutoFallibleTArray<int,500> partialWidthArray;
    if (!partialWidthArray.SetLength(length)) {
        return false;
    }

    BOOL success = ::GetTextExtentExPointI(dc,
                                           glyphs,
                                           length,
                                           INT_MAX,
                                           nullptr,
                                           partialWidthArray.Elements(),
                                           &size);
    if (!success) {
        return false;
    }

    gfxTextRun::CompressedGlyph g;
    gfxTextRun::CompressedGlyph *charGlyphs =
        aShapedText->GetCharacterGlyphs();
    uint32_t i;
    int32_t lastWidth = 0;
    int32_t appUnitsPerDevPixel = aShapedText->GetAppUnitsPerDevUnit();
    for (i = 0; i < length; ++i) {
        uint32_t offset = aOffset + i;
        int32_t advancePixels = partialWidthArray[i] - lastWidth;
        lastWidth = partialWidthArray[i];
        int32_t advanceAppUnits = advancePixels * appUnitsPerDevPixel;
        WCHAR glyph = glyphs[i];
        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(aText[i]),
                     "Invalid character detected!");
        bool atClusterStart = charGlyphs[offset].IsClusterStart();
        if (advanceAppUnits >= 0 &&
            gfxShapedWord::CompressedGlyph::IsSimpleAdvance(advanceAppUnits) &&
            gfxShapedWord::CompressedGlyph::IsSimpleGlyphID(glyph) &&
            atClusterStart)
        {
            charGlyphs[offset].SetSimpleGlyph(advanceAppUnits, glyph);
        } else {
            gfxShapedText::DetailedGlyph details;
            details.mGlyphID = glyph;
            details.mAdvance = advanceAppUnits;
            details.mXOffset = 0;
            details.mYOffset = 0;
            aShapedText->SetGlyphs(offset,
                                   g.SetComplex(atClusterStart, true, 1),
                                   &details);
        }
    }

    return true;
}
