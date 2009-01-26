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
 *   John Daggett <jdaggett@mozilla.com>
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
#include "gfxUserFontSet.h"

#include "nsDataHashtable.h"

#include <usp10.h>
#include <cairo-win32.h>

/**
 * List of different types of fonts we support on Windows.
 * These can generally be lumped in to 3 categories where we have to
 * do special things:  Really old fonts bitmap and vector fonts (device
 * and raster), Type 1 fonts, and TrueType/OpenType fonts.
 * 
 * This list is sorted in order from least prefered to most prefered.
 * We prefer Type1 fonts over OpenType fonts to avoid falling back to
 * things like Arial (opentype) when you ask for Helvetica (type1)
 **/
enum gfxWindowsFontType {
    GFX_FONT_TYPE_UNKNOWN = 0,
    GFX_FONT_TYPE_DEVICE,
    GFX_FONT_TYPE_RASTER,
    GFX_FONT_TYPE_TRUETYPE,
    GFX_FONT_TYPE_PS_OPENTYPE,
    GFX_FONT_TYPE_TT_OPENTYPE,
    GFX_FONT_TYPE_TYPE1
};

/**
 * FontFamily is a class that describes one of the fonts on the users system.  It holds
 * each FontEntry (maps more directly to a font face) which holds font type, charset info
 * and character map info.
 */
class FontEntry;
class FontFamily : public gfxFontFamily
{
public:
    FontFamily(const nsAString& aName) :
        gfxFontFamily(aName), mIsBadUnderlineFontFamily(PR_FALSE), mHasStyles(PR_FALSE) { }

    FontEntry *FindFontEntry(const gfxFontStyle& aFontStyle);

private:
    friend class gfxWindowsPlatform;

    void FindStyleVariations();

    static int CALLBACK FamilyAddStylesProc(const ENUMLOGFONTEXW *lpelfe,
                                            const NEWTEXTMETRICEXW *nmetrics,
                                            DWORD fontType, LPARAM data);

protected:
    PRBool FindWeightsForStyle(gfxFontEntry* aFontsForWeights[], const gfxFontStyle& aFontStyle);

public:
    nsTArray<nsRefPtr<FontEntry> > mVariations;
    PRPackedBool mIsBadUnderlineFontFamily;

private:
    PRPackedBool mHasStyles;
};

class FontEntry : public gfxFontEntry
{
public:
    FontEntry(const nsAString& aFaceName) : 
        gfxFontEntry(aFaceName), mFontType(GFX_FONT_TYPE_UNKNOWN),
        mForceGDI(PR_FALSE), mUnknownCMAP(PR_FALSE),
        mCharset(0), mUnicodeRanges(0)
    {
        mUnicodeFont = PR_FALSE;
        mSymbolFont = PR_FALSE;
    }

    FontEntry(const FontEntry& aFontEntry) :
        gfxFontEntry(aFontEntry),
        mWindowsFamily(aFontEntry.mWindowsFamily),
        mWindowsPitch(aFontEntry.mWindowsPitch),
        mFontType(aFontEntry.mFontType),
        mForceGDI(aFontEntry.mForceGDI),
        mUnknownCMAP(aFontEntry.mUnknownCMAP),
        mCharset(aFontEntry.mCharset),
        mUnicodeRanges(aFontEntry.mUnicodeRanges)
    {

    }
    static void InitializeFontEmbeddingProcs();

    static FontEntry* CreateFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                      nsISupports *aLoader,
                                      const PRUint8 *aFontData,
                                      PRUint32 aLength);
    
    static FontEntry* CreateFontEntry(const nsAString& aName, gfxWindowsFontType aFontType, PRBool aItalic, PRUint16 aWeight, gfxUserFontData* aUserFontData, HDC hdc = 0, LOGFONTW *aLogFont = nsnull);

    static void FillLogFont(LOGFONTW *aLogFont, FontEntry *aFontEntry, gfxFloat aSize, PRBool aItalic);

    static gfxWindowsFontType DetermineFontType(const NEWTEXTMETRICW& metrics, DWORD fontType)
    {
        gfxWindowsFontType feType;
        if (metrics.ntmFlags & NTM_TYPE1)
            feType = GFX_FONT_TYPE_TYPE1;
        else if (metrics.ntmFlags & NTM_PS_OPENTYPE)
            feType = GFX_FONT_TYPE_PS_OPENTYPE;
        else if (metrics.ntmFlags & NTM_TT_OPENTYPE)
            feType = GFX_FONT_TYPE_TT_OPENTYPE;
        else if (fontType == TRUETYPE_FONTTYPE)
            feType = GFX_FONT_TYPE_TRUETYPE;
        else if (fontType == RASTER_FONTTYPE)
            feType = GFX_FONT_TYPE_RASTER;
        else if (fontType == DEVICE_FONTTYPE)
            feType = GFX_FONT_TYPE_DEVICE;
        else
            feType = GFX_FONT_TYPE_UNKNOWN;
        
        return feType;
    }

    PRBool IsType1() const {
        return (mFontType == GFX_FONT_TYPE_TYPE1);
    }

    PRBool IsTrueType() const {
        return (mFontType == GFX_FONT_TYPE_TRUETYPE ||
                mFontType == GFX_FONT_TYPE_PS_OPENTYPE ||
                mFontType == GFX_FONT_TYPE_TT_OPENTYPE);
    }

    PRBool IsCrappyFont() const {
        /* return if it is a bitmap not a unicode font */
        return (!mUnicodeFont || mSymbolFont || IsType1());
    }

    PRBool MatchesGenericFamily(const nsACString& aGeneric) const {
        if (aGeneric.IsEmpty())
            return PR_TRUE;

        // Japanese 'Mincho' fonts do not belong to FF_MODERN even if
        // they are fixed pitch because they have variable stroke width.
        if (mWindowsFamily == FF_ROMAN && mWindowsPitch & FIXED_PITCH) {
            return aGeneric.EqualsLiteral("monospace");
        }

        // Japanese 'Gothic' fonts do not belong to FF_SWISS even if
        // they are variable pitch because they have constant stroke width.
        if (mWindowsFamily == FF_MODERN && mWindowsPitch & VARIABLE_PITCH) {
            return aGeneric.EqualsLiteral("sans-serif");
        }

        // All other fonts will be grouped correctly using family...
        switch (mWindowsFamily) {
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

    PRBool TestCharacterMap(PRUint32 aCh);

    PRUint8 mWindowsFamily;
    PRUint8 mWindowsPitch;

    gfxWindowsFontType mFontType;
    PRPackedBool mForceGDI    : 1;
    PRPackedBool mUnknownCMAP : 1;

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
    gfxWindowsFont(FontEntry *aFontEntry, const gfxFontStyle *aFontStyle);
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

    PRBool IsValid() { GetMetrics(); return mIsValid; }
    FontEntry *GetFontEntry();

    static already_AddRefed<gfxWindowsFont>
    GetOrMakeFont(FontEntry *aFontEntry, const gfxFontStyle *aStyle);

protected:
    HFONT MakeHFONT();
    void FillLogFont(gfxFloat aSize);

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

    virtual PRBool SetupCairoFont(gfxContext *aContext);
};

/**********************************************************************
 *
 * class gfxWindowsFontGroup
 *
 **********************************************************************/

class THEBES_API gfxWindowsFontGroup : public gfxFontGroup {

public:
    gfxWindowsFontGroup(const nsAString& aFamilies, const gfxFontStyle* aStyle, gfxUserFontSet *aUserFontSet);
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

    void GroupFamilyListToArrayList(nsTArray<nsRefPtr<FontEntry> > *list);
    void FamilyListToArrayList(const nsString& aFamilies,
                               const nsCString& aLangGroup,
                               nsTArray<nsRefPtr<FontEntry> > *list);

    void UpdateFontList();
    virtual gfxFloat GetUnderlineOffset();


protected:
    void InitFontList();
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const char *aString, PRUint32 aLength);
    void InitTextRunGDI(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

    void InitTextRunUniscribe(gfxContext *aContext, gfxTextRun *aRun, const PRUnichar *aString, PRUint32 aLength);

    already_AddRefed<gfxFont> WhichPrefFontSupportsChar(PRUint32 aCh);
    already_AddRefed<gfxFont> WhichSystemFontSupportsChar(PRUint32 aCh);

    already_AddRefed<gfxWindowsFont> WhichFontSupportsChar(const nsTArray<nsRefPtr<FontEntry> >& fonts, PRUint32 ch);
    void GetPrefFonts(const char *aLangGroup, nsTArray<nsRefPtr<FontEntry> >& array);
    void GetCJKPrefFonts(nsTArray<nsRefPtr<FontEntry> >& array);

private:

    nsCString mGenericFamily;
    nsTArray<nsRefPtr<FontEntry> > mFontEntries;

    const char *mItemLangGroup;  // used by pref-lang handling code

};

#endif /* GFX_WINDOWSFONTS_H */
