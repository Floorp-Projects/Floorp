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

#include "nsDataHashtable.h"

#include <usp10.h>
#include <cairo-win32.h>

/* Bug 341128 - w32api defines min/max which causes problems with <bitset> */
#ifdef __MINGW32__
#undef min
#undef max
#endif

#include <bitset>

#define NO_RANGE_FOUND 126 // bit 126 in the font unicode ranges is required to be 0

/** @description Font Weights
 * Each available font weight is stored as as single bit inside a bitset.
 * e.g. The binary value 0000000000001000 indcates font weight 400 is available.
 * while the binary value 0000000000001001 indicates both font weight 100 and 400 are available
 *
 * The font weights which will be represented include {100, 200, 300, 400, 500, 600, 700, 800, 900}
 * The font weight specified in the mFont->weight may include values which are not an even multiple of 100.
 * If so, the font weight mod 100 indicates the number steps to lighten are make bolder.
 * This corresponds to the CSS lighter and bolder property values. If bolder is applied twice to the font which has
 * a font weight of 400 then the mFont->weight will contain the value 402.
 * If lighter is applied twice to a font of weight 400 then the mFont->weight will contain the value 398.
 * Only nine steps of bolder or lighter are allowed by the CSS XPCODE.
 */
// XXX change this from using a bitset to something cleaner eventually
class WeightTable
{
public:
    THEBES_INLINE_DECL_REFCOUNTING(WeightTable)

    WeightTable() : mWeights(0) {}
    ~WeightTable() {

    }
    PRBool TriedWeight(PRUint8 aWeight) {
        return mWeights[aWeight - 1 + 10];
    }
    PRBool HasWeight(PRUint8 aWeight) {
        return mWeights[aWeight - 1];
    }
    void SetWeight(PRUint8 aWeight, PRBool aValue) {
        mWeights[aWeight - 1] = (aValue == PR_TRUE);
        mWeights[aWeight - 1 + 10] = PR_TRUE;
    }

private:
    std::bitset<20> mWeights;
};


/* Unicode subrange table
 *   from: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/unicode_63ub.asp
*/
struct UnicodeRangeTableEntry
{
    PRUint8 bit;
    PRUint32 start;
    PRUint32 end;
    const char *info;
};

static const struct UnicodeRangeTableEntry gUnicodeRanges[] = {
    { 0, 0x40, 0x5a, "Basic Latin" },
    { 0, 0x60, 0x7a, "Basic Latin" },
    { 1, 0xa0, 0xff, "Latin-1 Supplement" },
    { 2, 0x100, 0x17f, "Latin Extended-A" },
    { 3, 0x180, 0x24f, "Latin Extended-B" },
    { 4, 0x250, 0x2af, "IPA Extensions" },
    { 5, 0x2b0, 0x2ff, "Spacing Modifier Letters" },
    { 6, 0x300, 0x36f, "Combining Diacritical Marks" },
    { 7, 0x370, 0x3ff, "Greek and Coptic" },
    /* 8 - reserved */
    { 9, 0x400, 0x4ff, "Cyrillic" },
    { 9, 0x500, 0x52f, "Cyrillic Supplementary" },
    { 10, 0x530, 0x58f, "Armenian" },
    { 11, 0x590, 0x5ff, "Basic Hebrew" },
    /* 12 - reserved */
    { 13, 0x600, 0x6ff, "Basic Arabic" },
    /* 14 - reserved */
    { 15, 0x900, 0x97f, "Devanagari" },
    { 16, 0x980, 0x9ff, "Bengali" },
    { 17, 0xa00, 0xa7f, "Gurmukhi" },
    { 18, 0xa80, 0xaff, "Gujarati" },
    { 19, 0xb00, 0xb7f, "Oriya" },
    { 20, 0xb80, 0xbff, "Tamil" },
    { 21, 0xc00, 0xc7f, "Telugu" },
    { 22, 0xc80, 0xcff, "Kannada" },
    { 23, 0xd00, 0xd7f, "Malayalam" },
    { 24, 0xe00, 0xe7f, "Thai" },
    { 25, 0xe80, 0xeff, "Lao" },
    { 26, 0x10a0, 0x10ff, "Georgian" },
    /* 27 - reserved */
    { 28, 0x1100, 0x11ff, "Hangul Jamo" },
    { 29, 0x1e00, 0x1eff, "Latin Extended Additional" },
    { 30, 0x1f00, 0x1fff, "Greek Extended" },
    { 31, 0x2000, 0x206f, "General Punctuation" },
    { 32, 0x2070, 0x209f, "Subscripts and Superscripts" },
    { 33, 0x20a0, 0x20cf, "Currency Symbols" },
    { 34, 0x20d0, 0x20ff, "Combining Diacritical Marks for Symbols" },
    { 35, 0x2100, 0x214f, "Letter-like Symbols" },
    { 36, 0x2150, 0x218f, "Number Forms" },
    { 37, 0x2190, 0x21ff, "Arrows" },
    { 37, 0x27f0, 0x27ff, "Supplemental Arrows-A" },
    { 37, 0x2900, 0x297f, "Supplemental Arrows-B" },
    { 38, 0x2200, 0x22ff, "Mathematical Operators" },
    { 38, 0x2a00, 0x2aff, "Supplemental Mathematical Operators" },
    { 38, 0x27c0, 0x27ef, "Miscellaneous Mathematical Symbols-A" },
    { 38, 0x2980, 0x29ff, "Miscellaneous Mathematical Symbols-B" },
    { 39, 0x2300, 0x23ff, "Miscellaneous Technical" },
    { 40, 0x2400, 0x243f, "Control Pictures" },
    { 41, 0x2440, 0x245f, "Optical Character Recognition" },
    { 42, 0x2460, 0x24ff, "Enclosed Alphanumerics" },
    { 43, 0x2500, 0x257f, "Box Drawing" },
    { 44, 0x2580, 0x259f, "Block Elements" },
    { 45, 0x25a0, 0x25ff, "Geometric Shapes" },
    { 46, 0x2600, 0x26ff, "Miscellaneous Symbols" },
    { 47, 0x2700, 0x27bf, "Dingbats" },
    { 48, 0x3000, 0x303f, "Chinese, Japanese, and Korean (CJK) Symbols and Punctuation" },
    { 49, 0x3040, 0x309f, "Hiragana" },
    { 50, 0x30a0, 0x30ff, "Katakana" },
    { 50, 0x31f0, 0x31ff, "Katakana Phonetic Extensions" },
    { 51, 0x3100, 0x312f, "Bopomofo" },
    { 51, 0x31a0, 0x31bf, "Extended Bopomofo" },
    { 52, 0x3130, 0x318f, "Hangul Compatibility Jamo" },
    /* 53 - reserved */
    { 54, 0x3200, 0x32ff, "Enclosed CJK Letters and Months" },
    { 55, 0x3300, 0x33ff, "CJK Compatibility" },
    { 56, 0xac00, 0xd7a3, "Hangul" },
    { 57, 0xd800, 0xdfff, "Surrogates. Note that setting this bit implies that there is at least one supplementary code point (beyond the Basic Multilingual Plane, or BMP) that is supported by this font. See Surrogates and Supplementary Characters." },
    /* 58 - reserved */
    { 59, 0x4e00, 0x9fff, "CJK Unified Ideographs" },
    { 59, 0x2e80, 0x2eff, "CJK Radicals Supplement" },
    { 59, 0x2f00, 0x2fdf, "Kangxi Radicals" },
    { 59, 0x2ff0, 0x2fff, "Ideographic Description" },
    { 59, 0x3190, 0x319f, "Kanbun" },
    { 59, 0x3400, 0x4dbf, "CJK Unified Ideographs Extension A" },
    { 59, 0x20000, 0x2a6df, "CJK Unified Ideographs Extension B" },
    { 60, 0xe000, 0xf8ff, "Private Use Area" },
    { 61, 0xf900, 0xfaff, "CJK Compatibility Ideographs" },
    { 61, 0x2f800, 0x2fa1f, "CJK Compatibility Ideographs Supplement" },
    { 62, 0xfb00, 0xfb4f, "Alphabetical Presentation Forms" },
    { 63, 0xfb50, 0xfdff, "Arabic Presentation Forms-A" },
    { 64, 0xfe20, 0xfe2f, "Combining Half Marks" },
    { 65, 0xfe30, 0xfe4f, "CJK Compatibility Forms" },
    { 66, 0xfe50, 0xfe6f, "Small Form Variants" },
    { 67, 0xfe70, 0xfefe, "Arabic Presentation Forms-B" },
    { 68, 0xff00, 0xffef, "Halfwidth and Fullwidth Forms" },
    { 69, 0xfff0, 0xffff, "Specials" },
    { 70, 0xf00, 0xfff, "Tibetan" },
    { 71, 0x700, 0x74f, "Syriac" },
    { 72, 0x780, 0x7bf, "Thaana" },
    { 73, 0xd80, 0xdff, "Sinhala" },
    { 74, 0x1000, 0x109f, "Myanmar" },
    { 75, 0x1200, 0x12bf, "Ethiopic" },
    { 76, 0x13a0, 0x13ff, "Cherokee" },
    { 77, 0x1400, 0x167f, "Canadian Aboriginal Syllabics" },
    { 78, 0x1680, 0x169f, "Ogham" },
    { 79, 0x16a0, 0x16ff, "Runic" },
    { 80, 0x1780, 0x17ff, "Khmer" },
    { 80, 0x19e0, 0x19ff, "Khmer Symbols" },
    { 81, 0x1800, 0x18af, "Mongolian" },
    { 82, 0x2800, 0x28ff, "Braille" },
    { 83, 0xa000, 0xa48f, "Yi" },
    { 83, 0xa480, 0xa4cf, "Yi Radicals" },
    { 84, 0x1700, 0x171f, "Tagalog" },
    { 84, 0x1720, 0x173f, "Hanunoo" },
    { 84, 0x1740, 0x175f, "Buhid" },
    { 84, 0x1760, 0x177f, "Tagbanwa" },
    { 85, 0x10300, 0x1032f, "Old Italic" },
    { 86, 0x10330, 0x1034f, "Gothic" },
    { 87, 0x10440, 0x1044f, "Deseret" },
    { 88, 0x1d000, 0x1d0ff, "Byzantine Musical Symbols" },
    { 88, 0x1d100, 0x1d1ff, "Musical Symbols" },
    { 89, 0x1d400, 0x1d7ff, "Mathematical Alphanumeric Symbols" },
    { 90, 0xfff80, 0xfffff, "Private Use (Plane 15)" },
    { 90, 0x10ff80, 0x10ffff, "Private Use (Plane 16)" },
    { 91, 0xe0100, 0xe01ef, "Variation Selectors" },
    { 92, 0xe0000, 0xe007f , "Tags" }
};

static PRUint8 CharRangeBit(PRUint32 ch) {
    const PRUint32 n = sizeof(gUnicodeRanges) / sizeof(struct UnicodeRangeTableEntry);

    for (PRUint32 i = 0; i < n; ++i)
        if (ch >= gUnicodeRanges[i].start && ch <= gUnicodeRanges[i].end)
            return gUnicodeRanges[i].bit;

    return NO_RANGE_FOUND;
}

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
        mName(aName), mFontType(aFontType), mUnicodeFont(PR_FALSE),
        mCharset(0), mUnicodeRanges(0)
    {
    }

    PRBool IsCrappyFont() const {
        /* return if it is a bitmap, old school font or not a unicode font */
        return (!mUnicodeFont || mFontType == 0 || mFontType == 1);
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

    // The family name of the font
    nsString mName;

    PRUint16 mFontType;

    PRUint8 mFamily;
    PRUint8 mPitch;
    PRPackedBool mUnicodeFont;

    std::bitset<256> mCharset;
    std::bitset<128> mUnicodeRanges;
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

    HFONT GetHFONT();
    cairo_font_face_t *CairoFontFace();
    cairo_scaled_font_t *CairoScaledFont();
    SCRIPT_CACHE *ScriptCache() { return &mScriptCache; }
    const gfxMatrix& CurrentMatrix() const { return mCTM; }
    void UpdateCTM(const gfxMatrix& aMatrix);
    gfxFloat GetAdjustedSize() { MakeHFONT(); return mAdjustedSize; }

    virtual nsString GetUniqueName();

    virtual void Draw(gfxTextRun *aTextRun, PRUint32 aStart, PRUint32 aEnd,
                      gfxContext *aContext, PRBool aDrawToPath, gfxPoint *aBaselineOrigin,
                      Spacing *aSpacing);

protected:
    HFONT MakeHFONT();
    cairo_font_face_t *MakeCairoFontFace();
    cairo_scaled_font_t *MakeCairoScaledFont();
    void FillLogFont(gfxFloat aSize, PRInt16 aWeight);

    HFONT mFont;
    gfxFloat mAdjustedSize;
    gfxMatrix mCTM;

private:
    void Destroy();
    void ComputeMetrics();

    SCRIPT_CACHE mScriptCache;

    cairo_font_face_t *mFontFace;
    cairo_scaled_font_t *mScaledFont;

    gfxFont::Metrics *mMetrics;

    LOGFONTW mLogFont;

    nsRefPtr<WeightTable> mWeightTable;
    
    virtual void SetupCairoFont(cairo_t *aCR);
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
                                    Parameters* aParams);
    virtual gfxTextRun *MakeTextRun(const PRUint8* aString, PRUint32 aLength,
                                    Parameters* aParams);

    const nsACString& GetGenericFamily() const {
        return mGenericFamily;
    }

    PRUint32 FontListLength() const {
        return mFonts.Length();
    }

    gfxWindowsFont *GetFontAt(PRInt32 i) {
        return NS_STATIC_CAST(gfxWindowsFont*, NS_STATIC_CAST(gfxFont*, mFonts[i]));
    }

    void AppendFont(gfxWindowsFont *aFont) {
        mFonts.AppendElement(aFont);
    }

    PRBool HasFontNamed(const nsAString& aName) const {
        PRUint32 len = mFonts.Length();
        for (PRUint32 i = 0; i < len; ++i)
            if (aName.Equals(mFonts[i]->GetName()))
                return PR_TRUE;
        return PR_FALSE;
    }

    gfxWindowsFont *GetCachedFont(const nsAString& aName) const {
        nsRefPtr<gfxWindowsFont> font;
        if (mFontCache.Get(aName, &font))
            return font;
        return nsnull;
    }

    void PutCachedFont(const nsAString& aName, gfxWindowsFont *aFont) {
        mFontCache.Put(aName, aFont);
    }

protected:
    static PRBool MakeFont(const nsAString& fontName,
                           const nsACString& genericName,
                           void *closure);

    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const char *aString, PRUint32 aLength);
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

    void InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

private:
    friend class gfxWindowsTextRun;

    nsCString mGenericFamily;

    nsDataHashtable<nsStringHashKey, nsRefPtr<gfxWindowsFont> > mFontCache;
};

#endif /* GFX_WINDOWSFONTS_H */
