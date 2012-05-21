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
gfxGDIShaper::ShapeWord(gfxContext *aContext,
                        gfxShapedWord *aShapedWord,
                        const PRUnichar *aString)
{
    DCFromContext dc(aContext);
    AutoSelectFont selectFont(dc, static_cast<gfxGDIFont*>(mFont)->GetHFONT());

    PRUint32 length = aShapedWord->Length();
    nsAutoTArray<WORD,500> glyphArray;
    if (!glyphArray.SetLength(length)) {
        return false;
    }
    WORD *glyphs = glyphArray.Elements();

    DWORD ret = ::GetGlyphIndicesW(dc, aString, length,
                                   glyphs, GGI_MARK_NONEXISTING_GLYPHS);
    if (ret == GDI_ERROR) {
        return false;
    }

    for (int k = 0; k < length; k++) {
        if (glyphs[k] == 0xFFFF)
            return false;
    }
 
    SIZE size;
    nsAutoTArray<int,500> partialWidthArray;
    if (!partialWidthArray.SetLength(length)) {
        return false;
    }

    BOOL success = ::GetTextExtentExPointI(dc,
                                           glyphs,
                                           length,
                                           INT_MAX,
                                           NULL,
                                           partialWidthArray.Elements(),
                                           &size);
    if (!success) {
        return false;
    }

    gfxTextRun::CompressedGlyph g;
    PRUint32 i;
    PRInt32 lastWidth = 0;
    PRUint32 appUnitsPerDevPixel = aShapedWord->AppUnitsPerDevUnit();
    for (i = 0; i < length; ++i) {
        PRUint32 offset = i;
        PRInt32 advancePixels = partialWidthArray[i] - lastWidth;
        lastWidth = partialWidthArray[i];
        PRInt32 advanceAppUnits = advancePixels * appUnitsPerDevPixel;
        WCHAR glyph = glyphs[i];
        NS_ASSERTION(!gfxFontGroup::IsInvalidChar(aShapedWord->GetCharAt(offset)),
                     "Invalid character detected!");
        bool atClusterStart = aShapedWord->IsClusterStart(offset);
        if (advanceAppUnits >= 0 &&
            gfxShapedWord::CompressedGlyph::IsSimpleAdvance(advanceAppUnits) &&
            gfxShapedWord::CompressedGlyph::IsSimpleGlyphID(glyph) &&
            atClusterStart)
        {
            aShapedWord->SetSimpleGlyph(offset,
                                        g.SetSimpleGlyph(advanceAppUnits, glyph));
        } else {
            gfxShapedWord::DetailedGlyph details;
            details.mGlyphID = glyph;
            details.mAdvance = advanceAppUnits;
            details.mXOffset = 0;
            details.mYOffset = 0;
            aShapedWord->SetGlyphs(offset,
                                   g.SetComplex(atClusterStart, true, 1),
                                   &details);
        }
    }

    return true;
}
