/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_PANGOFONTS_H
#define GFX_PANGOFONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"

#include "nsAutoRef.h"
#include "nsTArray.h"

#include <pango/pango.h>

class gfxFcFontSet;
class gfxFcFont;
class gfxProxyFontEntry;
typedef struct _FcPattern FcPattern;
typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_  *FT_Library;

class gfxPangoFontGroup : public gfxFontGroup {
public:
    gfxPangoFontGroup (const nsAString& families,
                       const gfxFontStyle *aStyle,
                       gfxUserFontSet *aUserFontSet);
    virtual ~gfxPangoFontGroup ();

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);

    virtual gfxFont *GetFontAt(int32_t i);

    virtual void UpdateFontList();

    virtual already_AddRefed<gfxFont>
        FindFontForChar(uint32_t aCh, uint32_t aPrevCh, int32_t aRunScript,
                        gfxFont *aPrevMatchedFont,
                        uint8_t *aMatchType);

    static void Shutdown();

    // Used for @font-face { src: local(); }
    static gfxFontEntry *NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                      const nsAString &aFullname);
    // Used for @font-face { src: url(); }
    static gfxFontEntry *NewFontEntry(const gfxProxyFontEntry &aProxyEntry,
                                      const uint8_t *aFontData,
                                      uint32_t aLength);

private:
    // @param aLang [in] language to use for pref fonts and system default font
    //        selection, or NULL for the language guessed from the gfxFontStyle.
    // The FontGroup holds a reference to this set.
    gfxFcFontSet *GetFontSet(PangoLanguage *aLang = NULL);

    class FontSetByLangEntry {
    public:
        FontSetByLangEntry(PangoLanguage *aLang, gfxFcFontSet *aFontSet);
        PangoLanguage *mLang;
        nsRefPtr<gfxFcFontSet> mFontSet;
    };
    // There is only one of entry in this array unless characters from scripts
    // of other languages are measured.
    nsAutoTArray<FontSetByLangEntry,1> mFontSets;

    gfxFloat mSizeAdjustFactor;
    PangoLanguage *mPangoLanguage;

    void GetFcFamilies(nsTArray<nsString> *aFcFamilyList,
                       nsIAtom *aLanguage);

    // @param aLang [in] language to use for pref fonts and system font
    //        resolution, or NULL to guess a language from the gfxFontStyle.
    // @param aMatchPattern [out] if non-NULL, will return the pattern used.
    already_AddRefed<gfxFcFontSet>
    MakeFontSet(PangoLanguage *aLang, gfxFloat aSizeAdjustFactor,
                nsAutoRef<FcPattern> *aMatchPattern = NULL);

    gfxFcFontSet *GetBaseFontSet();
    gfxFcFont *GetBaseFont();

    gfxFloat GetSizeAdjustFactor()
    {
        if (mFontSets.Length() == 0)
            GetBaseFontSet();
        return mSizeAdjustFactor;
    }

    friend class gfxSystemFcFontEntry;
    static FT_Library GetFTLibrary();
};

#endif /* GFX_PANGOFONTS_H */
