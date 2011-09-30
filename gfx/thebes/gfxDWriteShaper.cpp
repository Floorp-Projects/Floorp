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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
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

#include "gfxDWriteShaper.h"
#include "gfxWindowsPlatform.h"

#include <dwrite.h>

#include "gfxDWriteTextAnalysis.h"

#include "nsCRT.h"

#define MAX_RANGE_LENGTH 25000

bool
gfxDWriteShaper::InitTextRun(gfxContext *aContext,
                             gfxTextRun *aTextRun,
                             const PRUnichar *aString,
                             PRUint32 aRunStart,
                             PRUint32 aRunLength,
                             PRInt32 aRunScript)
{
    HRESULT hr;
    // TODO: Handle TEST_DISABLE_OPTIONAL_LIGATURES

    DWRITE_READING_DIRECTION readingDirection = 
        aTextRun->IsRightToLeft()
            ? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
            : DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;

    gfxDWriteFont *font = static_cast<gfxDWriteFont*>(mFont);

    gfxTextRun::CompressedGlyph g;

    nsRefPtr<IDWriteTextAnalyzer> analyzer;

    hr = gfxWindowsPlatform::GetPlatform()->GetDWriteFactory()->
        CreateTextAnalyzer(getter_AddRefs(analyzer));
    if (FAILED(hr)) {
        return PR_FALSE;
    }

    /**
     * There's an internal 16-bit limit on some things inside the analyzer.
     * to be on the safe side here we split up any ranges over MAX_RANGE_LENGTH 
     * characters.
     * TODO: Figure out what exactly is going on, and what is a safe number, and 
     * why.
     */
    bool result = true;
    UINT32 rangeOffset = 0;
    while (rangeOffset < aRunLength) {
        PRUint32 rangeLen = NS_MIN<PRUint32>(aRunLength - rangeOffset,
                                             MAX_RANGE_LENGTH);
        if (rangeOffset + rangeLen < aRunLength) {
            // Iterate backwards to find a decent place to break shaping:
            // look for a whitespace char, and if that's not found then
            // at least a char where IsClusterStart is true.
            // However, we never iterate back further than half-way;
            // if a decent break is not found by then, we just chop anyway
            // at the original range length.
            PRUint32 adjRangeLen = 0;
            const PRUnichar *rangeStr = aString + aRunStart + rangeOffset;
            for (PRUint32 i = rangeLen; i > MAX_RANGE_LENGTH / 2; i--) {
                if (nsCRT::IsAsciiSpace(rangeStr[i])) {
                    adjRangeLen = i;
                    break;
                }
                if (adjRangeLen == 0 &&
                    aTextRun->IsClusterStart(aRunStart + rangeOffset + i)) {
                    adjRangeLen = i;
                }
            }
            if (adjRangeLen != 0) {
                rangeLen = adjRangeLen;
            }
        }

        PRUint32 rangeStart = aRunStart + rangeOffset;
        rangeOffset += rangeLen;
        TextAnalysis analysis(aString + rangeStart, rangeLen,
            NULL, 
            readingDirection);
        TextAnalysis::Run *runHead;
        DWRITE_LINE_BREAKPOINT *linebreaks;
        hr = analysis.GenerateResults(analyzer, &runHead, &linebreaks);

        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to generate results.");
            result = PR_FALSE;
            break;
        }

        PRUint32 appUnitsPerDevPixel = aTextRun->GetAppUnitsPerDevUnit();

        UINT32 maxGlyphs = 0;
trymoreglyphs:
        if ((PR_UINT32_MAX - 3 * rangeLen / 2 + 16) < maxGlyphs) {
            // This isn't going to work, we're going to cross the UINT32 upper
            // limit. Next range it is.
            continue;
        }
        maxGlyphs += 3 * rangeLen / 2 + 16;

        nsAutoTArray<UINT16, 400> clusters;
        nsAutoTArray<UINT16, 400> indices;
        nsAutoTArray<DWRITE_SHAPING_TEXT_PROPERTIES, 400> textProperties;
        nsAutoTArray<DWRITE_SHAPING_GLYPH_PROPERTIES, 400> glyphProperties;
        if (!clusters.SetLength(rangeLen) ||
            !indices.SetLength(maxGlyphs) || 
            !textProperties.SetLength(maxGlyphs) ||
            !glyphProperties.SetLength(maxGlyphs)) {
                continue;
        }

        UINT32 actualGlyphs;

        hr = analyzer->GetGlyphs(aString + rangeStart, rangeLen,
            font->GetFontFace(), FALSE, 
            readingDirection == DWRITE_READING_DIRECTION_RIGHT_TO_LEFT,
            &runHead->mScript, NULL, NULL, NULL, NULL, 0,
            maxGlyphs, clusters.Elements(), textProperties.Elements(),
            indices.Elements(), glyphProperties.Elements(), &actualGlyphs);

        if (hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
            // We increase the amount of glyphs and try again.
            goto trymoreglyphs;
        }
        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to get glyphs.");
            result = PR_FALSE;
            break;
        }

        WORD gID = indices[0];
        nsAutoTArray<FLOAT, 400> advances;
        nsAutoTArray<DWRITE_GLYPH_OFFSET, 400> glyphOffsets;
        if (!advances.SetLength(actualGlyphs) || 
            !glyphOffsets.SetLength(actualGlyphs)) {
            continue;
        }

        if (!static_cast<gfxDWriteFont*>(mFont)->mUseSubpixelPositions) {
            hr = analyzer->GetGdiCompatibleGlyphPlacements(
                                              aString + rangeStart,
                                              clusters.Elements(),
                                              textProperties.Elements(),
                                              rangeLen,
                                              indices.Elements(),
                                              glyphProperties.Elements(),
                                              actualGlyphs,
                                              font->GetFontFace(),
                                              font->GetAdjustedSize(),
                                              1.0,
                                              nsnull,
                                              FALSE,
                                              FALSE,
                                              FALSE,
                                              &runHead->mScript,
                                              NULL,
                                              NULL,
                                              NULL,
                                              0,
                                              advances.Elements(),
                                              glyphOffsets.Elements());
        } else {
            hr = analyzer->GetGlyphPlacements(aString + rangeStart,
                                              clusters.Elements(),
                                              textProperties.Elements(),
                                              rangeLen,
                                              indices.Elements(),
                                              glyphProperties.Elements(),
                                              actualGlyphs,
                                              font->GetFontFace(),
                                              font->GetAdjustedSize(),
                                              FALSE,
                                              FALSE,
                                              &runHead->mScript,
                                              NULL,
                                              NULL,
                                              NULL,
                                              0,
                                              advances.Elements(),
                                              glyphOffsets.Elements());
        }
        if (FAILED(hr)) {
            NS_WARNING("Analyzer failed to get glyph placements.");
            result = PR_FALSE;
            break;
        }

        nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;

        for (unsigned int c = 0; c < rangeLen; c++) {
            PRUint32 k = clusters[c];
            PRUint32 absC = rangeStart + c;

            if (c > 0 && k == clusters[c - 1]) {
                g.SetComplex(aTextRun->IsClusterStart(absC), PR_FALSE, 0);
                aTextRun->SetGlyphs(absC, g, nsnull);
                // This is a cluster continuation. No glyph here.
                continue;
            }

            // Count glyphs for this character
            PRUint32 glyphCount = actualGlyphs - k;
            PRUint32 nextClusterOffset;
            for (nextClusterOffset = c + 1; 
                nextClusterOffset < rangeLen; ++nextClusterOffset) {
                if (clusters[nextClusterOffset] > k) {
                    glyphCount = clusters[nextClusterOffset] - k;
                    break;
                }
            }
            PRInt32 advance = (PRInt32)(advances[k] * appUnitsPerDevPixel);
            if (glyphCount == 1 && advance >= 0 &&
                glyphOffsets[k].advanceOffset == 0 &&
                glyphOffsets[k].ascenderOffset == 0 &&
                aTextRun->IsClusterStart(absC) &&
                gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                gfxTextRun::CompressedGlyph::IsSimpleGlyphID(indices[k])) {
                  aTextRun->SetSimpleGlyph(absC, 
                                          g.SetSimpleGlyph(advance, 
                                                           indices[k]));
            } else {
                if (detailedGlyphs.Length() < glyphCount) {
                    if (!detailedGlyphs.AppendElements(
                        glyphCount - detailedGlyphs.Length())) {
                        continue;
                    }
                }
                float totalAdvance = 0;
                for (unsigned int z = 0; z < glyphCount; z++) {
                    detailedGlyphs[z].mGlyphID = indices[k + z];
                    detailedGlyphs[z].mAdvance = 
                        (PRInt32)(advances[k + z]
                           * appUnitsPerDevPixel);
                    if (readingDirection == 
                        DWRITE_READING_DIRECTION_RIGHT_TO_LEFT) {
                        detailedGlyphs[z].mXOffset = 
                            (totalAdvance + 
                              glyphOffsets[k + z].advanceOffset)
                            * appUnitsPerDevPixel;
                    } else {
                        detailedGlyphs[z].mXOffset = 
                            glyphOffsets[k + z].advanceOffset *
                            appUnitsPerDevPixel;
                    }
                    detailedGlyphs[z].mYOffset = 
                        -glyphOffsets[k + z].ascenderOffset *
                        appUnitsPerDevPixel;
                    totalAdvance += advances[k + z];
                }
                aTextRun->SetGlyphs(
                    absC,
                    g.SetComplex(aTextRun->IsClusterStart(absC),
                                 PR_TRUE,
                                 glyphCount),
                    detailedGlyphs.Elements());
            }
        }
    }

    return result;
}
