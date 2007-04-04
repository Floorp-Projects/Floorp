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
 *
 * Use something like:
 * perl -pi -e 's/^(\d+)\s+([\dA-Fa-f]+)\s+-\s+([\dA-Fa-f]+)\s+\b(.*)/    { \1, 0x\2, 0x\3,\"\4\" },/' < unicoderanges.txt
 * to generate the below list.
 */
struct UnicodeRangeTableEntry
{
    PRUint8 bit;
    PRUint32 start;
    PRUint32 end;
    const char *info;
};

static const struct UnicodeRangeTableEntry gUnicodeRanges[] = {
    { 0, 0x0000, 0x007F, "Basic Latin" },
    { 1, 0x0080, 0x00FF, "Latin-1 Supplement" },
    { 2, 0x0100, 0x017F, "Latin Extended-A" },
    { 3, 0x0180, 0x024F, "Latin Extended-B" },
    { 4, 0x0250, 0x02AF, "IPA Extensions" },
    { 4, 0x1D00, 0x1D7F, "Phonetic Extensions" },
    { 4, 0x1D80, 0x1DBF, "Phonetic Extensions Supplement" },
    { 5, 0x02B0, 0x02FF, "Spacing Modifier Letters" },
    { 5, 0xA700, 0xA71F, "Modifier Tone Letters" },
    { 6, 0x0300, 0x036F, "Spacing Modifier Letters" },
    { 6, 0x1DC0, 0x1DFF, "Combining Diacritical Marks Supplement" },
    { 7, 0x0370, 0x03FF, "Greek and Coptic" },
    { 8, 0x2C80, 0x2CFF, "Coptic" },
    { 9, 0x0400, 0x04FF, "Cyrillic" },
    { 9, 0x0500, 0x052F, "Cyrillic Supplementary" },
    { 10, 0x0530, 0x058F, "Armenian" },
    { 11, 0x0590, 0x05FF, "Basic Hebrew" },
    /* 12 - reserved */
    { 13, 0x0600, 0x06FF, "Basic Arabic" },
    { 13, 0x0750, 0x077F, "Arabic Supplement" },
    { 14, 0x07C0, 0x07FF, "N'Ko" },
    { 15, 0x0900, 0x097F, "Devanagari" },
    { 16, 0x0980, 0x09FF, "Bengali" },
    { 17, 0x0A00, 0x0A7F, "Gurmukhi" },
    { 18, 0x0A80, 0x0AFF, "Gujarati" },
    { 19, 0x0B00, 0x0B7F, "Oriya" },
    { 20, 0x0B80, 0x0BFF, "Tamil" },
    { 21, 0x0C00, 0x0C7F, "Telugu" },
    { 22, 0x0C80, 0x0CFF, "Kannada" },
    { 23, 0x0D00, 0x0D7F, "Malayalam" },
    { 24, 0x0E00, 0x0E7F, "Thai" },
    { 25, 0x0E80, 0x0EFF, "Lao" },
    { 26, 0x10A0, 0x10FF, "Georgian" },
    { 26, 0x2D00, 0x2D2F, "Georgian Supplement" },
    { 27, 0x1B00, 0x1B7F, "Balinese" },
    { 28, 0x1100, 0x11FF, "Hangul Jamo" },
    { 29, 0x1E00, 0x1EFF, "Latin Extended Additional" },
    { 29, 0x2C60, 0x2C7F, "Latin Extended-C" },
    { 30, 0x1F00, 0x1FFF, "Greek Extended" },
    { 31, 0x2000, 0x206F, "General Punctuation" },
    { 31, 0x2E00, 0x2E7F, "Supplemental Punctuation" },
    { 32, 0x2070, 0x209F, "Subscripts and Superscripts" },
    { 33, 0x20A0, 0x20CF, "Currency Symbols" },
    { 34, 0x20D0, 0x20FF, "Combining Diacritical Marks for Symbols" },
    { 35, 0x2100, 0x214F, "Letter-like Symbols" },
    { 36, 0x2150, 0x218F, "Number Forms" },
    { 37, 0x2190, 0x21FF, "Arrows" },
    { 37, 0x27F0, 0x27FF, "Supplemental Arrows-A" },
    { 37, 0x2900, 0x297F, "Supplemental Arrows-B" },
    { 37, 0x2B00, 0x2BFF, "Miscellaneous Symbols and Arrows" },
    { 38, 0x2200, 0x22FF, "Mathematical Operators" },
    { 38, 0x27C0, 0x27EF, "Miscellaneous Mathematical Symbols-A" },
    { 38, 0x2980, 0x29FF, "Miscellaneous Mathematical Symbols-B" },
    { 38, 0x2A00, 0x2AFF, "Supplemental Mathematical Operators" },
    { 39, 0x2300, 0x23FF, "Miscellaneous Technical" },
    { 40, 0x2400, 0x243F, "Control Pictures" },
    { 41, 0x2440, 0x245F, "Optical Character Recognition" },
    { 42, 0x2460, 0x24FF, "Enclosed Alphanumerics" },
    { 43, 0x2500, 0x257F, "Box Drawing" },
    { 44, 0x2580, 0x259F, "Block Elements" },
    { 45, 0x25A0, 0x25FF, "Geometric Shapes" },
    { 46, 0x2600, 0x26FF, "Miscellaneous Symbols" },
    { 47, 0x2700, 0x27BF, "Dingbats" },
    { 48, 0x3000, 0x303F, "Chinese, Japanese, and Korean (CJK) Symbols and Punctuation" },
    { 49, 0x3040, 0x309F, "Hiragana" },
    { 50, 0x30A0, 0x30FF, "Katakana" },
    { 50, 0x31F0, 0x31FF, "Katakana Phonetic Extensions" },
    { 51, 0x3100, 0x312F, "Bopomofo" },
    { 51, 0x31A0, 0x31BF, "Extended Bopomofo" },
    { 52, 0x3130, 0x318F, "Hangul Compatibility Jamo" },
    { 53, 0xA840, 0xA87F, "Phags-pa" },
    { 54, 0x3200, 0x32FF, "Enclosed CJK Letters and Months" },
    { 55, 0x3300, 0x33FF, "CJK Compatibility" },
    { 56, 0xAC00, 0xD7A3, "Hangul" },
    { 57, 0xD800, 0xDFFF, "Surrogates. Note that setting this bit implies that there is at least one supplementary code point beyond the Basic Multilingual Plane (BMP) that is supported by this font. See Surrogates and Supplementary Characters." },
    { 58, 0x10900, 0x1091F, "Phoenician" },
    { 59, 0x2E80, 0x2EFF, "CJK Radicals Supplement" },
    { 59, 0x2F00, 0x2FDF, "Kangxi Radicals" },
    { 59, 0x2FF0, 0x2FFF, "Ideographic Description Characters" },
    { 59, 0x3190, 0x319F, "Kanbun" },
    { 59, 0x3400, 0x4DBF, "CJK Unified Ideographs Extension A" },
    { 59, 0x4E00, 0x9FFF, "CJK Unified Ideographs" },
    { 59, 0x20000, 0x2A6DF, "CJK Unified Ideographs Extension B" },
    { 60, 0xE000, 0xF8FF, "Private Use (Plane 0)" },
    { 61, 0x31C0, 0x31EF, "CJK Base Strokes" },
    { 61, 0xF900, 0xFAFF, "CJK Compatibility Ideographs" },
    { 61, 0x2F800, 0x2FA1F, "CJK Compatibility Ideographs Supplement" },
    { 62, 0xFB00, 0xFB4F, "Alphabetical Presentation Forms" },
    { 63, 0xFB50, 0xFDFF, "Arabic Presentation Forms-A" },
    { 64, 0xFE20, 0xFE2F, "Combining Half Marks" },
    { 65, 0xFE10, 0xFE1F, "Vertical Forms" },
    { 65, 0xFE30, 0xFE4F, "CJK Compatibility Forms" },
    { 66, 0xFE50, 0xFE6F, "Small Form Variants" },
    { 67, 0xFE70, 0xFEFE, "Arabic Presentation Forms-B" },
    { 68, 0xFF00, 0xFFEF, "Halfwidth and Fullwidth Forms" },
    { 69, 0xFFF0, 0xFFFF, "Specials" },
    { 70, 0x0F00, 0x0FFF, "Tibetan" },
    { 71, 0x0700, 0x074F, "Syriac" },
    { 72, 0x0780, 0x07BF, "Thaana" },
    { 73, 0x0D80, 0x0DFF, "Sinhala" },
    { 74, 0x1000, 0x109F, "Myanmar" },
    { 75, 0x1200, 0x137F, "Ethiopic" },
    { 75, 0x1380, 0x139F, "Ethiopic Supplement" },
    { 75, 0x2D80, 0x2DDF, "Ethiopic Extended" },
    { 76, 0x13A0, 0x13FF, "Cherokee" },
    { 77, 0x1400, 0x167F, "Canadian Aboriginal Syllabics" },
    { 78, 0x1680, 0x169F, "Ogham" },
    { 79, 0x16A0, 0x16FF, "Runic" },
    { 80, 0x1780, 0x17FF, "Khmer" },
    { 80, 0x19E0, 0x19FF, "Khmer Symbols" },
    { 81, 0x1800, 0x18AF, "Mongolian" },
    { 82, 0x2800, 0x28FF, "Braille" },
    { 83, 0xA000, 0xA48F, "Yi" },
    { 83, 0xA490, 0xA4CF, "Yi Radicals" },
    { 84, 0x1700, 0x171F, "Tagalog" },
    { 84, 0x1720, 0x173F, "Hanunoo" },
    { 84, 0x1740, 0x175F, "Buhid" },
    { 84, 0x1760, 0x177F, "Tagbanwa" },
    { 85, 0x10300, 0x1032F, "Old Italic" },
    { 86, 0x10330, 0x1034F, "Gothic" },
    { 87, 0x10440, 0x1044F, "Deseret" },
    { 88, 0x1D000, 0x1D0FF, "Byzantine Musical Symbols" },
    { 88, 0x1D100, 0x1D1FF, "Musical Symbols" },
    { 88, 0x1D200, 0x1D24F, "Ancient Greek Musical Notation" },
    { 89, 0x1D400, 0x1D7FF, "Mathematical Alphanumeric Symbols" },
    { 90, 0xFF000, 0xFFFFD, "Private Use (Plane 15)" },
    { 90, 0x100000, 0x10FFFD, "Private Use (Plane 16)" },
    { 91, 0xFE00, 0xFE0F, "Variation Selectors" },
    { 91, 0xE0100, 0xE01EF, "Variation Selectors Supplement" },
    { 92, 0xE0000, 0xE007F, "Tags" },
    { 93, 0x1900, 0x194F, "Limbu" },
    { 94, 0x1950, 0x197F, "Tai Le" },
    { 95, 0x1980, 0x19DF, "New Tai Lue" },
    { 96, 0x1A00, 0x1A1F, "Buginese" },
    { 97, 0x2C00, 0x2C5F, "Glagolitic" },
    { 98, 0x2D40, 0x2D7F, "Tifinagh" },
    { 99, 0x4DC0, 0x4DFF, "Yijing Hexagram Symbols" },
    { 100, 0xA800, 0xA82F, "Syloti Nagri" },
    { 101, 0x10000, 0x1007F, "Linear B Syllabary" },
    { 101, 0x10080, 0x100FF, "Linear B Ideograms" },
    { 101, 0x10100, 0x1013F, "Aegean Numbers" },
    { 102, 0x10140, 0x1018F, "Ancient Greek Numbers" },
    { 103, 0x10380, 0x1039F, "Ugaritic" },
    { 104, 0x103A0, 0x103DF, "Old Persian" },
    { 105, 0x10450, 0x1047F, "Shavian" },
    { 106, 0x10480, 0x104AF, "Osmanya" },
    { 107, 0x10800, 0x1083F, "Cypriot Syllabary" },
    { 108, 0x10A00, 0x10A5F, "Kharoshthi" },
    { 109, 0x1D300, 0x1D35F, "Tai Xuan Jing Symbols" },
    { 110, 0x12000, 0x123FF, "Cuneiform" },
    { 110, 0x12400, 0x1247F, "Cuneiform Numbers and Punctuation" },
    { 111, 0x1D360, 0x1D37F, "Counting Rod Numerals" }
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

protected:
    static PRBool MakeFont(const nsAString& fontName,
                           const nsACString& genericName,
                           void *closure);

    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const char *aString, PRUint32 aLength);
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

    void InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

private:
    nsCString mGenericFamily;
};

#endif /* GFX_WINDOWSFONTS_H */
