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

#include "prtypes.h"
#include "gfxTypes.h"

#include "gfxContext.h"
#include "gfxUniscribeShaper.h"
#include "gfxWindowsPlatform.h"
#include "gfxAtoms.h"

#include "gfxFontTest.h"

#include "cairo.h"
#include "cairo-win32.h"

#include <windows.h>

#include "nsTArray.h"

#include "prlog.h"
#include "prinit.h"
static PRLogModuleInfo *gFontLog = PR_NewLogModule("winfonts");

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
                  const PRUnichar *aString, PRUint32 aLength,
                  SCRIPT_ITEM *aItem, PRUint32 aIVS) :
        mContext(aContext), mDC(aDC),
        mShaper(aShaper),
        mItemString(aString), mItemLength(aLength), 
        mAlternativeString(nsnull), mScriptItem(aItem),
        mScript(aItem->a.eScript),
        mNumGlyphs(0), mMaxGlyphs(ESTIMATE_MAX_GLYPHS(aLength)),
        mFontSelected(PR_FALSE), mIVS(aIVS)
    {
        NS_ASSERTION(mMaxGlyphs < 65535, "UniscribeItem is too big, ScriptShape() will fail!");
    }

    ~UniscribeItem() {
        free(mAlternativeString);
    }

    PRBool AllocateBuffers() {
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
        HDC shapeDC = nsnull;

        const PRUnichar *str = mAlternativeString ? mAlternativeString : mItemString;

        mScriptItem->a.fLogicalOrder = PR_TRUE; 
        SCRIPT_ANALYSIS sa = mScriptItem->a;

        while (PR_TRUE) {

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
                PRUint32 lastChar = str[mItemLength - 1];
                if (NS_IS_LOW_SURROGATE(lastChar)
                    && NS_IS_HIGH_SURROGATE(str[mItemLength - 2])) {
                    lastChar = SURROGATE_TO_UCS4(str[mItemLength - 2], lastChar);
                }
                PRUint16 glyphId = mShaper->GetFont()->GetUVSGlyph(lastChar, mIVS);
                if (glyphId) {
                    mGlyphs[mNumGlyphs - 1] = glyphId;
                }
            }

            return rv;
        }
    }

    PRBool ShapingEnabled() {
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
            mAlternativeString = nsnull;
        }
    }

    PRBool IsGlyphMissing(SCRIPT_FONTPROPERTIES *aSFP, PRUint32 aGlyphIndex) {
        return (mGlyphs[aGlyphIndex] == aSFP->wgDefault);
    }


    HRESULT Place() {
        HRESULT rv;
        HDC placeDC = nsnull;

        if (!mOffsets.SetLength(mNumGlyphs) ||
            !mAdvances.SetLength(mNumGlyphs)) {
            return E_OUTOFMEMORY;
        }

        SCRIPT_ANALYSIS sa = mScriptItem->a;

        while (PR_TRUE) {
            rv = ScriptPlace(placeDC, mShaper->ScriptCache(),
                             mGlyphs.Elements(), mNumGlyphs,
                             mAttr.Elements(), &sa,
                             mAdvances.Elements(), mOffsets.Elements(), NULL);

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
        rv = ScriptGetFontProperties(NULL, mShaper->ScriptCache(),
                                     sfp);
        if (rv == E_PENDING) {
            SelectFont();
            rv = ScriptGetFontProperties(mDC, mShaper->ScriptCache(),
                                         sfp);
        }
    }

    void SaveGlyphs(gfxTextRun *aRun, PRUint32 aRunStart) {
        PRUint32 offsetInRun = aRunStart + mScriptItem->iCharPos;

        // XXX We should store this in the item and only fetch it once
        SCRIPT_FONTPROPERTIES sfp;
        ScriptFontProperties(&sfp);

        PRUint32 offset = 0;
        nsAutoTArray<gfxTextRun::DetailedGlyph,1> detailedGlyphs;
        gfxTextRun::CompressedGlyph g;
        const PRUint32 appUnitsPerDevUnit = aRun->GetAppUnitsPerDevUnit();
        while (offset < mItemLength) {
            PRUint32 runOffset = offsetInRun + offset;
            if (offset > 0 && mClusters[offset] == mClusters[offset - 1]) {
                g.SetComplex(aRun->IsClusterStart(runOffset), PR_FALSE, 0);
                aRun->SetGlyphs(runOffset, g, nsnull);
            } else {
                // Count glyphs for this character
                PRUint32 k = mClusters[offset];
                PRUint32 glyphCount = mNumGlyphs - k;
                PRUint32 nextClusterOffset;
                PRBool missing = IsGlyphMissing(&sfp, k);
                for (nextClusterOffset = offset + 1; nextClusterOffset < mItemLength; ++nextClusterOffset) {
                    if (mClusters[nextClusterOffset] > k) {
                        glyphCount = mClusters[nextClusterOffset] - k;
                        break;
                    }
                }
                PRUint32 j;
                for (j = 1; j < glyphCount; ++j) {
                    if (IsGlyphMissing(&sfp, k + j)) {
                        missing = PR_TRUE;
                    }
                }
                PRInt32 advance = mAdvances[k]*appUnitsPerDevUnit;
                WORD glyph = mGlyphs[k];
                NS_ASSERTION(!gfxFontGroup::IsInvalidChar(mItemString[offset]),
                             "invalid character detected");
                if (missing) {
                    if (NS_IS_HIGH_SURROGATE(mItemString[offset]) &&
                        offset + 1 < mItemLength &&
                        NS_IS_LOW_SURROGATE(mItemString[offset + 1])) {
                        aRun->SetMissingGlyph(runOffset,
                                              SURROGATE_TO_UCS4(mItemString[offset],
                                                                mItemString[offset + 1]));
                    } else {
                        aRun->SetMissingGlyph(runOffset, mItemString[offset]);
                    }
                } else if (glyphCount == 1 && advance >= 0 &&
                    mOffsets[k].dv == 0 && mOffsets[k].du == 0 &&
                    gfxTextRun::CompressedGlyph::IsSimpleAdvance(advance) &&
                    gfxTextRun::CompressedGlyph::IsSimpleGlyphID(glyph)) {
                    aRun->SetSimpleGlyph(runOffset, g.SetSimpleGlyph(advance, glyph));
                } else {
                    if (detailedGlyphs.Length() < glyphCount) {
                        if (!detailedGlyphs.AppendElements(glyphCount - detailedGlyphs.Length()))
                            return;
                    }
                    PRUint32 i;
                    for (i = 0; i < glyphCount; ++i) {
                        gfxTextRun::DetailedGlyph *details = &detailedGlyphs[i];
                        details->mGlyphID = mGlyphs[k + i];
                        details->mAdvance = mAdvances[k + i]*appUnitsPerDevUnit;
                        details->mXOffset = float(mOffsets[k + i].du)*appUnitsPerDevUnit*aRun->GetDirection();
                        details->mYOffset = - float(mOffsets[k + i].dv)*appUnitsPerDevUnit;
                    }
                    aRun->SetGlyphs(runOffset,
                        g.SetComplex(PR_TRUE, PR_TRUE, glyphCount), detailedGlyphs.Elements());
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

        mFontSelected = PR_TRUE;
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
        for (PRUint32 i = 0; i < mItemLength; i++) {
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
    const PRUint32 mItemLength;

private:
    PRUnichar *mAlternativeString;

#define AVERAGE_ITEM_LENGTH 40

    nsAutoTArray<WORD, PRUint32(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mGlyphs;
    nsAutoTArray<WORD, AVERAGE_ITEM_LENGTH + 1> mClusters;
    nsAutoTArray<SCRIPT_VISATTR, PRUint32(ESTIMATE_MAX_GLYPHS(AVERAGE_ITEM_LENGTH))> mAttr;
 
    nsAutoTArray<GOFFSET, 2 * AVERAGE_ITEM_LENGTH> mOffsets;
    nsAutoTArray<int, 2 * AVERAGE_ITEM_LENGTH> mAdvances;

#undef AVERAGE_ITEM_LENGTH

    int mMaxGlyphs;
    int mNumGlyphs;
    PRUint32 mIVS;

    PRPackedBool mFontSelected;
};

#define MAX_ITEM_LENGTH 32768

static PRUint32 FindNextItemStart(int aOffset, int aLimit,
                                  nsTArray<SCRIPT_LOGATTR> &aLogAttr,
                                  const PRUnichar *aString)
{
    if (aOffset + MAX_ITEM_LENGTH >= aLimit) {
        // The item starting at aOffset can't be longer than the max length,
        // so starting the next item at aLimit won't cause ScriptShape() to fail.
        return aLimit;
    }

    // Try to start the next item before or after a space, since spaces
    // don't kern or ligate.
    PRUint32 off;
    int boundary = -1;
    for (off = MAX_ITEM_LENGTH; off > 1; --off) {
      if (aLogAttr[off].fCharStop) {
          if (off > boundary) {
              boundary = off;
          }
          if (aString[aOffset+off] == ' ' || aString[aOffset+off - 1] == ' ')
            return aOffset+off;
      }
    }

    // Try to start the next item at the last cluster boundary in the range.
    if (boundary > 0) {
      return aOffset+boundary;
    }

    // No nice cluster boundaries inside MAX_ITEM_LENGTH characters, break
    // on the size limit. It won't be visually plesaing, but at least it
    // won't cause ScriptShape() to fail.
    return aOffset + MAX_ITEM_LENGTH;
}

class Uniscribe
{
public:
    Uniscribe(const PRUnichar *aString, PRUint32 aLength, PRBool aIsRTL) :
        mString(aString), mLength(aLength), mIsRTL(aIsRTL)
    {
    }
    ~Uniscribe() {
    }

    void Init() {
        memset(&mControl, 0, sizeof(SCRIPT_CONTROL));
        memset(&mState, 0, sizeof(SCRIPT_STATE));
        // Lock the direction. Don't allow the itemizer to change directions
        // based on character type.
        mState.uBidiLevel = mIsRTL;
        mState.fOverrideDirection = PR_TRUE;
    }

private:

    // Append mItems[aIndex] to aDest, adding extra items to aDest to ensure
    // that no item is too long for ScriptShape() to handle. See bug 366643.
    nsresult CopyItemSplitOversize(int aIndex, nsTArray<SCRIPT_ITEM> &aDest) {
        aDest.AppendElement(mItems[aIndex]);
        const int itemLength = mItems[aIndex+1].iCharPos - mItems[aIndex].iCharPos;
        if (ESTIMATE_MAX_GLYPHS(itemLength) > 65535) {
            // This items length would cause ScriptShape() to fail. We need to
            // add extra items here so that no item's length could cause the fail.

            // Get cluster boundaries, so we can break cleanly if possible.
            nsTArray<SCRIPT_LOGATTR> logAttr;
            if (!logAttr.SetLength(itemLength))
                return NS_ERROR_FAILURE;
            HRESULT rv = ScriptBreak(mString+mItems[aIndex].iCharPos, itemLength,
                                     &mItems[aIndex].a, logAttr.Elements());
            if (FAILED(rv))
                return NS_ERROR_FAILURE;

            const int nextItemStart = mItems[aIndex+1].iCharPos;
            int start = FindNextItemStart(mItems[aIndex].iCharPos,
                                          nextItemStart, logAttr, mString);

            while (start < nextItemStart) {
                SCRIPT_ITEM item = mItems[aIndex];
                item.iCharPos = start;
                aDest.AppendElement(item);
                start = FindNextItemStart(start, nextItemStart, logAttr, mString);
            }
        } 
        return NS_OK;
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
        while ((rv = ScriptItemize(mString, mLength, maxItems, &mControl, &mState,
                                   mItems.Elements(), &mNumItems)) == E_OUTOFMEMORY) {
            maxItems *= 2;
            if (!mItems.SetLength(maxItems + 1)) {
                return 0;
            }
            Init();
        }

        if (ESTIMATE_MAX_GLYPHS(mLength) > 65535) {
            // Any item of length > 43680 will cause ScriptShape() to fail, as its
            // mMaxGlyphs value will be greater than 65535 (43680*1.5+16>65535). So we
            // need to break up items which are longer than that upon cluster boundaries.
            // See bug 394751 for details.
            nsTArray<SCRIPT_ITEM> items;
            for (int i=0; i<mNumItems; i++) {
                nsresult nrs = CopyItemSplitOversize(i, items);
                NS_ASSERTION(NS_SUCCEEDED(nrs), "CopyItemSplitOversize() failed");
            }
            items.AppendElement(mItems[mNumItems]); // copy terminator.

            mItems = items;
            mNumItems = items.Length() - 1; // Don't count the terminator.
        }
        return mNumItems;
    }

    PRUint32 ItemsLength() {
        return mNumItems;
    }

    SCRIPT_ITEM *ScriptItem(PRUint32 i) {
        NS_ASSERTION(i <= (PRUint32)mNumItems, "Trying to get out of bounds item");
        return &mItems[i];
    }

private:
    const PRUnichar *mString;
    const PRUint32 mLength;
    const PRBool mIsRTL;

    SCRIPT_CONTROL mControl;
    SCRIPT_STATE   mState;
    nsTArray<SCRIPT_ITEM> mItems;
    int mNumItems;
};


PRBool
gfxUniscribeShaper::InitTextRun(gfxContext *aContext,
                                gfxTextRun *aTextRun,
                                const PRUnichar *aString,
                                PRUint32 aRunStart,
                                PRUint32 aRunLength,
                                PRInt32 aRunScript)
{
    DCFromContext aDC(aContext);
 
    const PRBool isRTL = aTextRun->IsRightToLeft();

    PRBool result = PR_TRUE;
    HRESULT rv;

    gfxGDIFont *font = static_cast<gfxGDIFont*>(mFont);
    AutoSelectFont fs(aDC, font->GetHFONT());

    Uniscribe us(aString + aRunStart, aRunLength, isRTL);

    /* itemize the string */
    int numItems = us.Itemize();

    SaveDC(aDC);
    PRUint32 ivs = 0;
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

        if (i+1 < numItems && aRunStart + iCharPosNext <= aRunLength - 2
            && aString[aRunStart + iCharPosNext] == H_SURROGATE(kUnicodeVS17)
            && PRUint32(aString[aRunStart + iCharPosNext + 1]) - L_SURROGATE(kUnicodeVS17)
            <= L_SURROGATE(kUnicodeVS256) - L_SURROGATE(kUnicodeVS17)) {

            ivs = SURROGATE_TO_UCS4(aString[aRunStart + iCharPosNext],
                                    aString[aRunStart + iCharPosNext + 1]);
        } else {
            ivs = 0;
        }

        UniscribeItem item(aContext, aDC, this,
                           aString + aRunStart + iCharPos,
                           iCharPosNext - iCharPos,
                           us.ScriptItem(i), ivs);
        if (!item.AllocateBuffers()) {
            result = PR_FALSE;
            break;
        }

        if (!item.ShapingEnabled()) {
            item.EnableShaping();
        }

        rv = item.Shape();
        if (FAILED(rv)) {
            PR_LOG(gFontLog, PR_LOG_DEBUG, ("shaping failed"));
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
            aTextRun->ResetGlyphRuns();
            // Uniscribe doesn't like this font for some reason.
            // Returning FALSE will make the gfxGDIFont discard this
            // shaper and replace it with a "dumb" GDI one.
            result = PR_FALSE;
            break;
        }

        item.SaveGlyphs(aTextRun, aRunStart);
    }

    RestoreDC(aDC, -1);

    return result;
}
