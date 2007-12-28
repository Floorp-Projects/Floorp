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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef GFX_WINDOWSFONTS_H
#define GFX_WINDOWSFONTS_H

#include "prtypes.h"
#include "gfxTypes.h"
#include "gfxColor.h"
#include "gfxFont.h"
#include "gfxMatrix.h"
#include "gfxFontUtils.h"

#include "nsDataHashtable.h"

#include <usp10.h>
#include <cairo-win32.h>

/**
 * FontEntry is a class that describes one of the fonts on the users system
 * It contains information such as the name, font type, charset table and unicode ranges.
 * It may be extended to also keep basic metrics of the fonts so that we can better
 * compare one FontEntry to another.
 */
class FontEntry
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(FontEntry)

    FontEntry(const nsAString& aName, PRUint16 aFontType) : 
        mName(aName), mFontType(aFontType), mDefaultWeight(0),
        mUnicodeFont(PR_FALSE), mSymbolFont(PR_FALSE),
        mCharset(0), mUnicodeRanges(0)
    {
    }

    PRBool IsCrappyFont() const {
        /* return if it is a bitmap, old school font or not a unicode font */
        return (!mUnicodeFont || mSymbolFont || mFontType != TRUETYPE_FONTTYPE);
    }

    PRBool MatchesGenericFamily(const nsACString& aGeneric) const {
        if (aGeneric.IsEmpty())
            return PR_TRUE;

        // Japanese 'Mincho' fonts do not belong to FF_MODERN even if
        // they are fixed pitch because they have variable stroke width.
        if (mFamily == FF_ROMAN && mPitch & FIXED_PITCH) {
            return aGeneric.EqualsLiteral("monospace");
        }

        // Japanese 'Gothic' fonts do not belong to FF_SWISS even if
        // they are variable pitch because they have constant stroke width.
        if (mFamily == FF_MODERN && mPitch & VARIABLE_PITCH) {
            return aGeneric.EqualsLiteral("sans-serif");
        }

        // All other fonts will be grouped correctly using family...
        switch (mFamily) {
        case FF_DONTCARE:
            return PR_TRUE;
        case FF_ROMAN:
            return aGeneric.EqualsLiteral("serif");
        case FF_SWISS:
            return aGeneric.EqualsLiteral("sans-serif");
        case FF_MODERN:
            return aGeneric.EqualsLiteral("monospace");
        case FF_SCRIPT:
            return aGeneric.EqualsLiteral("cursive");
        case FF_DECORATIVE:
            return aGeneric.EqualsLiteral("fantasy");
        }

        return PR_FALSE;
    }

    PRBool SupportsLangGroup(const nsACString& aLangGroup) const {
        if (aLangGroup.IsEmpty())
            return PR_TRUE;

        PRInt16 bit = -1;

        /* map our langgroup names in to Windows charset bits */
        if (aLangGroup.EqualsLiteral("x-western")) {
            bit = ANSI_CHARSET;
        } else if (aLangGroup.EqualsLiteral("ja")) {
            bit = SHIFTJIS_CHARSET;
        } else if (aLangGroup.EqualsLiteral("ko")) {
            bit = HANGEUL_CHARSET;
        } else if (aLangGroup.EqualsLiteral("ko-XXX")) {
            bit = JOHAB_CHARSET;
        } else if (aLangGroup.EqualsLiteral("zh-CN")) {
            bit = GB2312_CHARSET;
        } else if (aLangGroup.EqualsLiteral("zh-TW")) {
            bit = CHINESEBIG5_CHARSET;
        } else if (aLangGroup.EqualsLiteral("el")) {
            bit = GREEK_CHARSET;
        } else if (aLangGroup.EqualsLiteral("tr")) {
            bit = TURKISH_CHARSET;
        } else if (aLangGroup.EqualsLiteral("he")) {
            bit = HEBREW_CHARSET;
        } else if (aLangGroup.EqualsLiteral("ar")) {
            bit = ARABIC_CHARSET;
        } else if (aLangGroup.EqualsLiteral("x-baltic")) {
            bit = BALTIC_CHARSET;
        } else if (aLangGroup.EqualsLiteral("x-cyrillic")) {
            bit = RUSSIAN_CHARSET;
        } else if (aLangGroup.EqualsLiteral("th")) {
            bit = THAI_CHARSET;
        } else if (aLangGroup.EqualsLiteral("x-central-euro")) {
            bit = EASTEUROPE_CHARSET;
        } else if (aLangGroup.EqualsLiteral("x-symbol")) {
            bit = SYMBOL_CHARSET;
        }

        if (bit != -1)
            return mCharset[bit];

        return PR_FALSE;
    }

    PRBool SupportsRange(PRUint8 range) {
        return mUnicodeRanges[range];
    }

    class WeightTable
    {
    public:
        THEBES_INLINE_DECL_REFCOUNTING(WeightTable)
            
        WeightTable() : mWeights(0) {}
        ~WeightTable() {}
        PRBool TriedWeight(PRUint8 aWeight) {
            return mWeights[aWeight - 1 + 10];
        }
        PRBool HasWeight(PRUint8 aWeight) {
            return mWeights[aWeight - 1];
        }
        void SetWeight(PRUint8 aWeight, PRBool aValue) {
            mWeights[aWeight - 1] = aValue;
            mWeights[aWeight - 1 + 10] = PR_TRUE;
        }
    private:
        std::bitset<20> mWeights;
    };

    // The family name of the font
    nsString mName;

    PRUint16 mFontType;
    PRUint16 mDefaultWeight;

    PRUint8 mFamily;
    PRUint8 mPitch;
    PRPackedBool mUnicodeFont;
    PRPackedBool mSymbolFont;

    std::bitset<256> mCharset;
    std::bitset<128> mUnicodeRanges;

    WeightTable mWeightTable;

    gfxSparseBitSet mCharacterMap;
};


/**********************************************************************
 *
 * class gfxWindowsFont
 *
 **********************************************************************/

class gfxWindowsFont : public gfxFont {
public:
    gfxWindowsFont(const nsAString& aName, const gfxFontStyle *aFontStyle);
    virtual ~gfxWindowsFont();

    virtual const gfxFont::Metrics& GetMetrics();

    HFONT GetHFONT() { return mFont; }
    cairo_font_face_t *CairoFontFace();
    cairo_scaled_font_t *CairoScaledFont();
    SCRIPT_CACHE *ScriptCache() { return &mScriptCache; }
    gfxFloat GetAdjustedSize() { MakeHFONT(); return mAdjustedSize; }

    virtual nsString GetUniqueName();

    virtual void Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                      gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aBaselineOrigin,
                      Spacing *aSpacing);

    virtual PRUint32 GetSpaceGlyph() {
        GetMetrics(); // ensure that the metrics are computed but don't recompute them
        return mSpaceGlyph;
    };

    FontEntry *GetFontEntry() { return mFontEntry; }

protected:
    HFONT MakeHFONT();
    void FillLogFont(gfxFloat aSize, PRInt16 aWeight);

    HFONT    mFont;
    gfxFloat mAdjustedSize;
    PRUint32 mSpaceGlyph;

private:
    void ComputeMetrics();

    SCRIPT_CACHE mScriptCache;

    cairo_font_face_t *mFontFace;
    cairo_scaled_font_t *mScaledFont;

    gfxFont::Metrics *mMetrics;

    LOGFONTW mLogFont;

    nsRefPtr<FontEntry> mFontEntry;
    
    virtual PRBool SetupCairoFont(gfxContext *aContext);
};

/**********************************************************************
 *
 * class gfxWindowsFontGroup
 *
 **********************************************************************/

class THEBES_API gfxWindowsFontGroup : public gfxFontGroup {

public:
    gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle* aStyle);
    virtual ~gfxWindowsFontGroup();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    virtual gfxTextRun *MakeTextRun(const PRUnichar* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                    const Parameters* aParams, PRUint32 aFlags);

    const nsACString& GetGenericFamily() const {
        return mGenericFamily;
    }

    const nsTArray<nsRefPtr<FontEntry> >& GetFontList() const {
        return mFontEntries;
    }
    PRUint32 FontListLength() const {
        return mFontEntries.Length();
    }

    FontEntry *GetFontEntryAt(PRInt32 i) {
        return mFontEntries[i];
    }

    virtual gfxWindowsFont *GetFontAt(PRInt32 i);

protected:
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const char *aString, PRUint32 aLength);
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

    void InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

private:
    nsCString mGenericFamily;
    nsTArray<nsRefPtr<FontEntry> > mFontEntries;
};

#endif /* GFX_WINDOWSFONTS_H */
