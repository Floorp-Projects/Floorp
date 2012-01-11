/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Foundation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2005-2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
 *   Mats Palmgren <mats.palmgren@bredband.net>
 *   John Daggett <jdaggett@mozilla.com>
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
