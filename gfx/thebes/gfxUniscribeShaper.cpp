/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxUniscribeShaper.h"
#include "gfxWindowsPlatform.h"

#include "gfxFontTest.h"

#include "cairo.h"
#include "cairo-win32.h"

#include <windows.h>

#include "nsTArray.h"

#include "prinit.h"

/**********************************************************************
 *
 * class gfxUniscribeShaper
 *
 **********************************************************************/

#define ESTIMATE_MAX_GLYPHS(L) (((3 * (L)) >> 1) + 16)

class UniscribeItem
{
public:
    UniscribeItem(gfxContext *aContext, HDC aDC,
                  gfxUniscribeShaper *aShaper,
                  const PRUnichar *aString, uint32_t aLength,
                  SCRIPT_ITEM *aItem, uint32_t aIVS) :
        mContext(aContext), mDC(aDC),
        mShaper(aShaper),
        mItemString(aString), mItemLength(aLength), 
        mAlternativeString(nullptr), mScriptItem(aItem),
        mScript(aItem->a.eScript),
        mNumGlyphs(0), mMaxGlyphs(ESTIMATE_MAX_GLYPHS(aLength)),
        mFontSelected(false), mIVS(aIVS)
    {
        // See bug 394751 for details.
        NS_ASSERTION(mMaxGlyphs < 65535,
                     "UniscribeItem is too big, ScriptShape() will fail!");
    }

    ~UniscribeItem() {
        free(mAlternativeString);
    }

    bool AllocateBuffers() {
        return (mGlyphs.SetLength(mMaxGlyphs) &&
                mClusters.SetLength(mItemLength + 1) &&
                mAttr.SetLength(mMaxGlyphs));
    }

    /* possible return values:
     * S_OK - things succeeded
     * GDI_ERROR - things failed to shape.  Might want to try again after calling DisableShaping()
     */

    HRESULT Shape() {
        HRESULT rv;
        HDC shapeDC = nullptr;

        const PRUnichar *str = mAlternativeString ? mAlternativeString : mItemString;

        mScriptItem->a.fLogicalOrder = true; 
        SCRIPT_ANALYSIS sa = mScriptItem->a;

        while (true) {

            rv = ScriptShape(shapeDC, mShaper->ScriptCache(),
                             str, mItemLength,
                             mMaxGlyphs, &sa,
                             mGlyphs.Elements(), mClusters.Elements(),
                             mAttr.Elements(), &mNumGlyphs);

            if (rv == E_OUTOFMEMORY) {
                mMaxGlyphs *= 2;
                if (!mGlyphs.SetLength(mMaxGlyphs) ||
                    !mAttr.SetLength(mMaxGlyphs)) {
                    return E_OUTOFMEMORY;
                }
                continue;
            }

            // Uniscribe can't do shaping with some fonts, so it sets the 
            // fNoGlyphIndex flag in the SCRIPT_ANALYSIS structure to indicate
            // this.  This occurs with CFF fonts loaded with 
            // AddFontMemResourceEx but it's not clear what the other cases
            // are. We return an error so our caller can try fallback shaping.
            // see http://msdn.microsoft.com/en-us/library/ms776520(VS.85).aspx

            if (sa.fNoGlyphIndex) {
                return GDI_ERROR;
            }

            if (rv == E_PENDING) {
                if (shapeDC == mDC) {
                    // we already tried this once, something failed, give up
                    return E_PENDING;
                }

                SelectFont();

                shapeDC = mDC;
                continue;
            }

            // http://msdn.microsoft.com/en-us/library/dd368564(VS.85).aspx:
            // Uniscribe will return this if "the font corresponding to the
            // DC does not support the script required by the run...".
            // In this case, we'll set the script code to SCRIPT_UNDEFINED
            // and try again, so that we'll at least get glyphs even though
            // they won't necessarily have proper shaping.
            // (We probably shouldn't have selected this font at all,
            // but it's too late to fix that here.)
            if (rv == USP_E_SCRIPT_NOT_IN_FONT) {
                sa.eScript = SCRIPT_UNDEFINED;
                NS_WARNING("Uniscribe says font does not support script needed");
                continue;
            }

            // Prior to Windows 7, Uniscribe didn't support Ideographic Variation
            // Selectors. Replace the UVS glyph manually.
            if (mIVS) {
                uint32_t lastChar = str[mItemLength - 1];
                if (NS_IS_LOW_SURROGATE(lastChar)
                    && NS_IS_HIGH_SURROGATE(str[mItemLength - 2])) {
                    lastChar = SURROGATE_TO_UCS4(str[mItemLength - 2], lastChar);
                }
                uint16_t glyphId = mShaper->GetFont()->GetUVSGlyph(lastChar, mIVS);
                if (glyphId) {
                    mGlyphs[mNumGlyphs - 1] = glyphId;
                }
            }

            return rv;
        }
    }

    bool ShapingEnabled() {
        return (mScriptItem->a.eScript != SCRIPT_UNDEFINED);
    }
    void DisableShaping() {
        mScriptItem->a.eScript = SCRIPT_UNDEFINED;
        // Note: If we disable the shaping by using SCRIPT_UNDEFINED and
        // the string has the surrogate pair, ScriptShape API is
        // *sometimes* crashed. Therefore, we should replace the surrogate
        // pair to U+FFFD. See bug 341500.
        GenerateAlternativeString();
    }
    void EnableShaping() {
        mScriptItem->a.eScript = mScript;
        if (mAlternativeString) {
            free(mAlternativeString);
            mAlternativeString = nullptr;
        }
    }

    bool IsGlyphMissing(SCRIPT_FONTPROPERTIES *aSFP, uint32_t aGlyphIndex) {
        return (mGlyphs[aGlyphIndex] == aSFP->wgDefault);
    }


    HRESULT Place() {
        HRESULT rv;
        HDC placeDC = nullptr;

        if (!mOffsets.SetLength(mNumGlyphs) ||
            !mAdvances.SetLength(mNumGlyphs)) {
            return E_OUTOFMEMORY;
        }

        SCRIPT_ANALYSIS sa = mScriptItem->a;

        while (true) {
            rv = ScriptPlace(placeDC, mShaper->ScriptCache(),
                             mGlyphs.Elements(), mNumGlyphs,
                             mAttr.Elements(), &sa,
                             mAdvances.Elements(), mOffsets.Elements(), nullptr);

            if (rv == E_PENDING) {
                SelectFont();
                placeDC = mDC;
                continue;
            }

            if (rv == USP_E_SCRIPT_NOT_IN_FONT) {
                sa.eScript = SCRIPT_UNDEFINED;
                continue;
            }

            break;
        }

        return rv;
    }

    void ScriptFontProperties(SCRIPT_FONTPROPERTIES *sfp) {
        HRESULT rv;

        memset(sfp, 0, sizeof(SCRIPT_FONTPROPERTIES));
        sfp->cBytes = sizeof(SCRIPT_FONTPROPERTIES);
        rv = ScriptGetFontProperties(nullptr, mShaper->ScriptCache(),
                                     sfp);
        if (rv == E_PENDING) {
            SelectFont();
            rv = ScriptGetFontProperties(mDC, mShaper->ScriptCache(),
                                         sfp);
        }
    }

    void SaveGlyphs(gfxShapedText *aShapedText, uint32_t aOffset) {
        uint32_t offsetInRun = mScriptItem->iCharPos;

        // XXX We should store this in the item and only fetch it once
        SCRIPT_FONTPROPERTIES sfp;
        ScriptFontProperties(&sfp);

        uint32_t offset = 0;
        nsAutoTArray<gfxShapedText::DetailedGlyph,1> detailedGlyphs;
        gfxShapedText::CompressedGlyph g;
        gfxShapedText::CompressedGlyph *charGlyphs =
            aShapedText->GetCharacterGlyphs();
        const uint32_t appUnitsPerDevUnit = aShapedText->GetAppUnitsPerDevUnit();
        while (offset < mItemLength) {
            uint32_t runOffset = aOffset + offsetInRun + offset;
            bool atClusterStart = charGlyphs[runOffset].IsClusterStart();
            if (offset > 0 && mClusters[offset] == mClusters[offset - 1]) {
                gfxShapedText::CompressedGlyph &g = charGlyphs[runOffset];
                NS_ASSERTION(!g.IsSimpleGlyph(), "overwriting a simple glyph");
                g.SetComplex(atClusterStart, false, 0);
            } else {
                // Count glyphs for this character
                uint32_t k = mClusters[offset];
                uint32_t glyphCount = mNumGlyphs - k;
                uint32_t nextClusterOffset;
                bool missing = IsGlyphMissing(&sfp, k);
                for (nextClusterOffset = offset + 1; nextClusterOffset < mItemLength; ++nextClusterOffset) {
                    if (mClusters[nextClusterOffset] > k) {
                        glyphCount = mClusters[nextClusterOffset] - k;
                        break;
                    }
                }
                uint32_t j;
                for (j = 1; j < glyphCount; ++j) {
                    if (IsGlyphMissing(&sfp, k + j)) {
                        missing = true;
                    }
                }
                int32_t advance = mAdvances[k]*appUnitsPerDevUnit;
                WORD glyph = mGlyphs[k];
                NS_ASSERTION(!gfxFontGroup::IsInvalidChar(mItemString[offset]),
                             "invalid character detected");
                if (missing) {
                    if (NS_IS_HIGH_SURROGATE(mItemString[offset]) &&
                        offset + 1 < mItemLength &&
                        NS_IS_LOW_SURROGATE(mItemString[offset + 1])) {
                        aShapedText->SetMissingGlyph(runOffset,
                                                     SURROGATE_TO_UCS4(mItemString[offset],
                                                                       mItemString[offset + 1]),
                                                     mShaper->GetFont());
                    } else {
                        aShapedText->SetMissingGlyph(runOffset, mItemString[offset],
                                                     mShaper->GetFont());
                    }
                } else if (glyphCount == 1 && advance >= 0 &&
                    mOffsets[k].dv == 0 && mOffsets[k].du == 0 &&
                    gfxShapedText::CompressedGlyph::IsSimpleAdvance(advance) &&
                    gfxShapedText::CompressedGlyph::IsSimpleGlyphID(glyph) &&
                    atClusterStart)
                {
                    charGlyphs[runOffset].SetSimpleGlyph(advance, glyph);
                } else {
                    if (detailedGlyphs.Length() < glyphCount) {
                        if (!detailedGlyphs.AppendElements(glyphCount - detailedGlyphs.Length()))
                            return;
                    }
                    uint32_t i;
                    for (i = 0; i < glyphCount; ++i) {
                        gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                        details->mGlyphID = mGlyphs[k + i];
                        details->mAdvance = mAdvances[k + i] * appUnitsPerDevUnit;
                        details->mXOffset = float(mOffsets[k + i].du) * appUnitsPerDevUnit *
                            aShapedText->GetDirection();
                        details->mYOffset = - float(mOffsets[k + i].dv) * appUnitsPerDevUnit;
                    }
                    aShapedText->SetGlyphs(runOffset,
                                           g.SetComplex(atClusterStart, true,
                                                        glyphCount),
                                           detailedGlyphs.Elements());
                }
            }
            ++offset;
        }
    }

    void SelectFont() {
        if (mFontSelected)
            return;

        cairo_t *cr = mContext->GetCairo();

        cairo_set_font_face(cr, mShaper->GetFont()->CairoFontFace());
        cairo_set_font_size(cr, mShaper->GetFont()->GetAdjustedSize());
        cairo_scaled_font_t *scaledFont = mShaper->GetFont()->CairoScaledFont();
        cairo_win32_scaled_font_select_font(scaledFont, mDC);

        mFontSelected = true;
    }

private:

    void GenerateAlternativeString() {
        if (mAlternativeString)
            free(mAlternativeString);
        mAlternativeString = (PRUnichar *)malloc(mItemLength * sizeof(PRUnichar));
        if (!mAlternativeString)
            return;
        memcpy((void *)mAlternativeString, (const void *)mItemString,
               mItemLength * sizeof(PRUnichar));
        for (uint32_t i = 0; i < mItemLength; i++) {
            if (NS_IS_HIGH_SURROGATE(mItemString[i]) || NS_IS_LOW_SURROGATE(mItemString[i]))
                mAlternativeString[i] = PRUnichar(0xFFFD);
        }
    }

private:
    nsRefPtr<gfxContext> mContext;
    HDC mDC;
    gfxUniscribeShaper *mShaper;

    SCRIPT_ITEM *mScriptItem;
    WORD mScript;

public:
    // these point to the full string/length of the item
    const PRUnichar *mItemString;
    const uint32_t mItemLength;

private:
    PRUnichar *mAlternativeString;

#define AVERAGE_ITEM_LENGTH 40

    nsAutoTArray<WORD, uint32_t(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mGlyphs;
    nsAutoTArray<WORD, AVERAGE_ITEM_LENGTH + 1> mClusters;
    nsAutoTArray<SCRIPT_VISATTR, uint32_t(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mAttr;
 
    nsAutoTArray<GOFFSET, 2 * AVERAGE_ITEM_LENGTH> mOffsets;
    nsAutoTArray<int, 2 * AVERAGE_ITEM_LENGTH> mAdvances;

#undef AVERAGE_ITEM_LENGTH

    int mMaxGlyphs;
    int mNumGlyphs;
    uint32_t mIVS;

    bool mFontSelected;
};

class Uniscribe
{
public:
    Uniscribe(const PRUnichar *aString,
              gfxShapedText *aShapedText,
              uint32_t aOffset, uint32_t aLength):
        mString(aString), mShapedText(aShapedText),
        mOffset(aOffset), mLength(aLength)
    {
    }

    void Init() {
        memset(&mControl, 0, sizeof(SCRIPT_CONTROL));
        memset(&mState, 0, sizeof(SCRIPT_STATE));
        // Lock the direction. Don't allow the itemizer to change directions
        // based on character type.
        mState.uBidiLevel = mShapedText->IsRightToLeft() ? 1 : 0;
        mState.fOverrideDirection = true;
    }

public:
    int Itemize() {
        HRESULT rv;

        int maxItems = 5;

        Init();

        // Allocate space for one more item than expected, to handle a rare
        // overflow in ScriptItemize (pre XP SP2). See bug 366643.
        if (!mItems.SetLength(maxItems + 1)) {
            return 0;
        }
        while ((rv = ScriptItemize(mString, mLength,
                                   maxItems, &mControl, &mState,
                                   mItems.Elements(), &mNumItems)) == E_OUTOFMEMORY) {
            maxItems *= 2;
            if (!mItems.SetLength(maxItems + 1)) {
                return 0;
            }
            Init();
        }

        return mNumItems;
    }

    SCRIPT_ITEM *ScriptItem(uint32_t i) {
        NS_ASSERTION(i <= (uint32_t)mNumItems, "Trying to get out of bounds item");
        return &mItems[i];
    }

private:
    const PRUnichar *mString;
    gfxShapedText   *mShapedText;
    uint32_t         mOffset;
    uint32_t         mLength;

    SCRIPT_CONTROL mControl;
    SCRIPT_STATE   mState;
    nsTArray<SCRIPT_ITEM> mItems;
    int mNumItems;
};


bool
gfxUniscribeShaper::ShapeText(gfxContext      *aContext,
                              const PRUnichar *aText,
                              uint32_t         aOffset,
                              uint32_t         aLength,
                              int32_t          aScript,
                              gfxShapedText   *aShapedText)
{
    DCFromContext aDC(aContext);
 
    bool result = true;
    HRESULT rv;

    Uniscribe us(aText, aShapedText, aOffset, aLength);

    /* itemize the string */
    int numItems = us.Itemize();

    uint32_t length = aLength;
    SaveDC(aDC);
    uint32_t ivs = 0;
    for (int i = 0; i < numItems; ++i) {
        int iCharPos = us.ScriptItem(i)->iCharPos;
        int iCharPosNext = us.ScriptItem(i+1)->iCharPos;

        if (ivs) {
            iCharPos += 2;
            if (iCharPos >= iCharPosNext) {
                ivs = 0;
                continue;
            }
        }

        if (i+1 < numItems && iCharPosNext <= length - 2
            && aText[iCharPosNext] == H_SURROGATE(kUnicodeVS17)
            && uint32_t(aText[iCharPosNext + 1]) - L_SURROGATE(kUnicodeVS17)
            <= L_SURROGATE(kUnicodeVS256) - L_SURROGATE(kUnicodeVS17)) {

            ivs = SURROGATE_TO_UCS4(aText[iCharPosNext],
                                    aText[iCharPosNext + 1]);
        } else {
            ivs = 0;
        }

        UniscribeItem item(aContext, aDC, this,
                           aText + iCharPos,
                           iCharPosNext - iCharPos,
                           us.ScriptItem(i), ivs);
        if (!item.AllocateBuffers()) {
            result = false;
            break;
        }

        if (!item.ShapingEnabled()) {
            item.EnableShaping();
        }

        rv = item.Shape();
        if (FAILED(rv)) {
            // we know we have the glyphs to display this font already
            // so Uniscribe just doesn't know how to shape the script.
            // Render the glyphs without shaping.
            item.DisableShaping();
            rv = item.Shape();
        }
#ifdef DEBUG
        if (FAILED(rv)) {
            NS_WARNING("Uniscribe failed to shape with font");
        }
#endif

        if (SUCCEEDED(rv)) {
            rv = item.Place();
#ifdef DEBUG
            if (FAILED(rv)) {
                // crap fonts may fail when placing (e.g. funky free fonts)
                NS_WARNING("Uniscribe failed to place with font");
            }
#endif
        }

        if (FAILED(rv)) {
            // Uniscribe doesn't like this font for some reason.
            // Returning FALSE will make the gfxGDIFont retry with the
            // "dumb" GDI shaper, unless useUniscribeOnly was set.
            result = false;
            break;
        }

        item.SaveGlyphs(aShapedText, aOffset);
    }

    RestoreDC(aDC, -1);

    return result;
}
