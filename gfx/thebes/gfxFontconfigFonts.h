/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FONTCONFIG_FONTS_H
#define GFX_FONTCONFIG_FONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxTextRun.h"

#include "nsAutoRef.h"
#include "nsTArray.h"

#include <pango/pango.h>

class gfxFcFontSet;
class gfxFcFont;
typedef struct _FcPattern FcPattern;
typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_  *FT_Library;

class gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup(const mozilla::FontFamilyList& aFontFamilyList,
                      const gfxFontStyle *aStyle,
                      gfxUserFontSet *aUserFontSet,
                      gfxFloat aDevToCssSize);
    virtual ~gfxPangoFontGroup();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    virtual gfxFont* GetFirstValidFont(uint32_t aCh = 0x20);

    virtual void UpdateUserFonts();

    virtual already_AddRefed<gfxFont>
        FindFontForChar(uint32_t aCh, uint32_t aPrevCh, uint32_t aNextCh,
                        Script aRunScript, gfxFont *aPrevMatchedFont,
                        uint8_t *aMatchType);

    static void Shutdown();

    // Used for @font-face { src: local(); }
    static gfxFontEntry *NewFontEntry(const nsAString& aFontName,
                                      uint16_t aWeight,
                                      int16_t aStretch,
                                      uint8_t aStyle);
    // Used for @font-face { src: url(); }
    static gfxFontEntry *NewFontEntry(const nsAString& aFontName,
                                      uint16_t aWeight,
                                      int16_t aStretch,
                                      uint8_t aStyle,
                                      const uint8_t* aFontData,
                                      uint32_t aLength);

private:

    virtual gfxFont *GetFontAt(int32_t i, uint32_t aCh = 0x20);

    // @param aLang [in] language to use for pref fonts and system default font
    //        selection, or nullptr for the language guessed from the
    //        gfxFontStyle.
    // The FontGroup holds a reference to this set.
    gfxFcFontSet *GetFontSet(PangoLanguage *aLang = nullptr);

    class FontSetByLangEntry {
    public:
        FontSetByLangEntry(PangoLanguage *aLang, gfxFcFontSet *aFontSet);
        PangoLanguage *mLang;
        RefPtr<gfxFcFontSet> mFontSet;
    };
    // There is only one of entry in this array unless characters from scripts
    // of other languages are measured.
    AutoTArray<FontSetByLangEntry,1> mFontSets;

    gfxFloat mSizeAdjustFactor;
    PangoLanguage *mPangoLanguage;

    // @param aLang [in] language to use for pref fonts and system font
    //        resolution, or nullptr to guess a language from the gfxFontStyle.
    // @param aMatchPattern [out] if non-nullptr, will return the pattern used.
    already_AddRefed<gfxFcFontSet>
    MakeFontSet(PangoLanguage *aLang, gfxFloat aSizeAdjustFactor,
                nsAutoRef<FcPattern> *aMatchPattern = nullptr);

    gfxFcFontSet *GetBaseFontSet();
    gfxFcFont *GetBaseFont();

    gfxFloat GetSizeAdjustFactor()
    {
        if (mFontSets.Length() == 0)
            GetBaseFontSet();
        return mSizeAdjustFactor;
    }

    // old helper methods from gfxFontGroup, moved here so that those methods
    // can be revamped without affecting the legacy code here

    // iterate over the fontlist, lookup names and expand generics
    void EnumerateFontListPFG(nsIAtom *aLanguage, void *aClosure);

    // expand a generic to a list of specific names based on prefs
    void FindGenericFontsPFG(mozilla::FontFamilyType aGenericType,
                             nsIAtom *aLanguage,
                             void *aClosure);

    // lookup and add a font with a given name (i.e. *not* a generic!)
    void FindPlatformFontPFG(const nsAString& aName,
                             bool aUseFontSet,
                             void *aClosure);

    static void
    ResolveGenericFontNamesPFG(mozilla::FontFamilyType aGenericType,
                               nsIAtom *aLanguage,
                               nsTArray<nsString>& aGenericFamilies);


    friend class gfxSystemFcFontEntry;
    static FT_Library GetFTLibrary();
};

#endif /* GFX_FONTCONFIG_FONTS_H */
