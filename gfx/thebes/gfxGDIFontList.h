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
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Masayuki Nakano <masayuki@d-toybox.com>
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

#ifndef GFX_GDIFONTLIST_H
#define GFX_GDIFONTLIST_H

#include "gfxWindowsPlatform.h"
#include "gfxPlatformFontList.h"
#include "gfxAtoms.h"

#include <windows.h>

class AutoDC // get the global device context, and auto-release it on destruction
{
public:
    AutoDC() {
        mDC = ::GetDC(NULL);
    }

    ~AutoDC() {
        ::ReleaseDC(NULL, mDC);
    }

    HDC GetDC() {
        return mDC;
    }

private:
    HDC mDC;
};

class AutoSelectFont // select a font into the given DC, and auto-restore
{
public:
    AutoSelectFont(HDC aDC, LOGFONTW *aLogFont)
        : mOwnsFont(PR_FALSE)
    {
        mFont = ::CreateFontIndirectW(aLogFont);
        if (mFont) {
            mOwnsFont = PR_TRUE;
            mDC = aDC;
            mOldFont = (HFONT)::SelectObject(aDC, mFont);
        } else {
            mOldFont = NULL;
        }
    }

    AutoSelectFont(HDC aDC, HFONT aFont)
        : mOwnsFont(PR_FALSE)
    {
        mDC = aDC;
        mFont = aFont;
        mOldFont = (HFONT)::SelectObject(aDC, aFont);
    }

    ~AutoSelectFont() {
        if (mOldFont) {
            ::SelectObject(mDC, mOldFont);
            if (mOwnsFont) {
                ::DeleteObject(mFont);
            }
        }
    }

    PRBool IsValid() const {
        return mFont != NULL;
    }

    HFONT GetFont() const {
        return mFont;
    }

private:
    HDC    mDC;
    HFONT  mFont;
    HFONT  mOldFont;
    PRBool mOwnsFont;
};

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

// A single member of a font family (i.e. a single face, such as Times Italic)
// represented as a LOGFONT that will resolve to the correct face.
// This replaces FontEntry from gfxWindowsFonts.h/cpp.
class GDIFontEntry : public gfxFontEntry
{
public:
    LPLOGFONTW GetLogFont() { return &mLogFont; }

    nsresult ReadCMAP();

    virtual PRBool IsSymbolFont();

    void FillLogFont(LOGFONTW *aLogFont, PRBool aItalic,
                     PRUint16 aWeight, gfxFloat aSize, PRBool aUseCleartype);

    static gfxWindowsFontType DetermineFontType(const NEWTEXTMETRICW& metrics, 
                                                DWORD fontType)
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

    virtual PRBool MatchesGenericFamily(const nsACString& aGeneric) const {
        if (aGeneric.IsEmpty()) {
            return PR_TRUE;
        }

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
            return PR_FALSE;
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

    virtual PRBool SupportsLangGroup(nsIAtom* aLangGroup) const {
        if (!aLangGroup || aLangGroup == gfxAtoms::x_unicode) {
            return PR_TRUE;
        }

        PRInt16 bit = -1;

        /* map our langgroup names in to Windows charset bits */
        if (aLangGroup == gfxAtoms::x_western) {
            bit = ANSI_CHARSET;
        } else if (aLangGroup == gfxAtoms::ja) {
            bit = SHIFTJIS_CHARSET;
        } else if (aLangGroup == gfxAtoms::ko) {
            bit = HANGEUL_CHARSET;
        } else if (aLangGroup == gfxAtoms::ko_xxx) {
            bit = JOHAB_CHARSET;
        } else if (aLangGroup == gfxAtoms::zh_cn) {
            bit = GB2312_CHARSET;
        } else if (aLangGroup == gfxAtoms::zh_tw) {
            bit = CHINESEBIG5_CHARSET;
        } else if (aLangGroup == gfxAtoms::el) {
            bit = GREEK_CHARSET;
        } else if (aLangGroup == gfxAtoms::tr) {
            bit = TURKISH_CHARSET;
        } else if (aLangGroup == gfxAtoms::he) {
            bit = HEBREW_CHARSET;
        } else if (aLangGroup == gfxAtoms::ar) {
            bit = ARABIC_CHARSET;
        } else if (aLangGroup == gfxAtoms::x_baltic) {
            bit = BALTIC_CHARSET;
        } else if (aLangGroup == gfxAtoms::x_cyrillic) {
            bit = RUSSIAN_CHARSET;
        } else if (aLangGroup == gfxAtoms::th) {
            bit = THAI_CHARSET;
        } else if (aLangGroup == gfxAtoms::x_central_euro) {
            bit = EASTEUROPE_CHARSET;
        } else if (aLangGroup == gfxAtoms::x_symbol) {
            bit = SYMBOL_CHARSET;
        }

        if (bit != -1) {
            return mCharset.test(bit);
        }

        return PR_FALSE;
    }

    virtual PRBool SupportsRange(PRUint8 range) {
        return mUnicodeRanges.test(range);
    }

    virtual PRBool SkipDuringSystemFallback() { 
        return !HasCmapTable(); // explicitly skip non-SFNT fonts
    }

    virtual PRBool TestCharacterMap(PRUint32 aCh);

    // create a font entry for a font with a given name
    static GDIFontEntry* CreateFontEntry(const nsAString& aName,
                                         gfxWindowsFontType aFontType,
                                         PRBool aItalic,
                                         PRUint16 aWeight, PRInt16 aStretch,
                                         gfxUserFontData* aUserFontData);

    // create a font entry for a font referenced by its fullname
    static GDIFontEntry* LoadLocalFont(const gfxProxyFontEntry &aProxyEntry,
                                       const nsAString& aFullname);

    PRUint8 mWindowsFamily;
    PRUint8 mWindowsPitch;

    gfxWindowsFontType mFontType;
    PRPackedBool mForceGDI    : 1;
    PRPackedBool mUnknownCMAP : 1;

    gfxSparseBitSet mCharset;
    gfxSparseBitSet mUnicodeRanges;

protected:
    friend class gfxWindowsFont;

    GDIFontEntry(const nsAString& aFaceName, gfxWindowsFontType aFontType,
                 PRBool aItalic, PRUint16 aWeight, PRInt16 aStretch,
                 gfxUserFontData *aUserFontData);

    void InitLogFont(const nsAString& aName, gfxWindowsFontType aFontType);

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle, PRBool aNeedsBold);

    virtual nsresult GetFontTable(PRUint32 aTableTag,
                                  FallibleTArray<PRUint8>& aBuffer);

    LOGFONTW mLogFont;
};

// a single font family, referencing one or more faces
class GDIFontFamily : public gfxFontFamily
{
public:
    GDIFontFamily(nsAString &aName) :
        gfxFontFamily(aName) {}

    virtual void FindStyleVariations();

private:
    static int CALLBACK FamilyAddStylesProc(const ENUMLOGFONTEXW *lpelfe,
                                            const NEWTEXTMETRICEXW *nmetrics,
                                            DWORD fontType, LPARAM data);
};

class gfxGDIFontList : public gfxPlatformFontList {
public:
    static gfxGDIFontList* PlatformFontList() {
        return static_cast<gfxGDIFontList*>(sPlatformFontList);
    }

    // initialize font lists
    virtual nsresult InitFontList();

    virtual gfxFontEntry* GetDefaultFont(const gfxFontStyle* aStyle, PRBool& aNeedsBold);

    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);

    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const PRUint8 *aFontData, PRUint32 aLength);

    virtual PRBool ResolveFontName(const nsAString& aFontName,
                                   nsAString& aResolvedFontName);

private:
    friend class gfxWindowsPlatform;

    gfxGDIFontList();

    void InitializeFontEmbeddingProcs();

    nsresult GetFontSubstitutes();

    static int CALLBACK EnumFontFamExProc(ENUMLOGFONTEXW *lpelfe,
                                          NEWTEXTMETRICEXW *lpntme,
                                          DWORD fontType,
                                          LPARAM lParam);

    typedef nsDataHashtable<nsStringHashKey, nsRefPtr<gfxFontFamily> > FontTable;

    FontTable mFontSubstitutes;
    nsTArray<nsString> mNonExistingFonts;
};

#endif /* GFX_GDIFONTLIST_H */
