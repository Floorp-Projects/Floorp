/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTLIST_H
#define GFX_FT2FONTLIST_H

#ifdef XP_WIN
#include "gfxWindowsPlatform.h"
#include <windows.h>
#endif
#include "gfxPlatformFontList.h"

namespace mozilla {
    namespace dom {
        class FontListEntry;
    };
};
using mozilla::dom::FontListEntry;

class FontNameCache;
typedef struct FT_FaceRec_* FT_Face;

class FT2FontEntry : public gfxFontEntry
{
public:
    FT2FontEntry(const nsAString& aFaceName) :
        gfxFontEntry(aFaceName)
    {
        mFTFace = nullptr;
        mFontFace = nullptr;
        mFTFontIndex = 0;
    }

    ~FT2FontEntry();

    const nsString& GetName() const {
        return Name();
    }

    // create a font entry for a downloaded font
    static FT2FontEntry* 
    CreateFontEntry(const gfxProxyFontEntry &aProxyEntry,
                    const uint8_t *aFontData, uint32_t aLength);

    // create a font entry representing an installed font, identified by
    // a FontListEntry; the freetype and cairo faces will not be instantiated
    // until actually needed
    static FT2FontEntry*
    CreateFontEntry(const FontListEntry& aFLE);

    // Create a font entry for a given freetype face; if it is an installed font,
    // also record the filename and index
    // aFontData (if non-NULL) is NS_Malloc'ed data that aFace depends on,
    // to be freed after the face is destroyed
    static FT2FontEntry* 
    CreateFontEntry(FT_Face aFace, const char *aFilename, uint8_t aIndex,
                    const nsAString& aName,
                    const uint8_t *aFontData = nullptr);

    virtual gfxFont *CreateFontInstance(const gfxFontStyle *aFontStyle,
                                        bool aNeedsBold);

    cairo_font_face_t *CairoFontFace();
    cairo_scaled_font_t *CreateScaledFont(const gfxFontStyle *aStyle);

    nsresult ReadCMAP();
    nsresult GetFontTable(uint32_t aTableTag, FallibleTArray<uint8_t>& aBuffer);

    // Check for various kinds of brokenness, and set flags on the entry
    // accordingly so that we avoid using bad font tables
    void CheckForBrokenFont(gfxFontFamily *aFamily);

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontListSizes*    aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontListSizes*    aSizes) const;

    FT_Face mFTFace;
    cairo_font_face_t *mFontFace;

    nsCString mFilename;
    uint8_t mFTFontIndex;
};

class FT2FontFamily : public gfxFontFamily
{
public:
    FT2FontFamily(const nsAString& aName) :
        gfxFontFamily(aName) { }

    // Append this family's faces to the IPC fontlist
    void AddFacesToFontList(InfallibleTArray<FontListEntry>* aFontList);
};

class gfxFT2FontList : public gfxPlatformFontList
{
public:
    gfxFT2FontList();

    virtual gfxFontFamily* GetDefaultFont(const gfxFontStyle* aStyle);

    virtual gfxFontEntry* LookupLocalFont(const gfxProxyFontEntry *aProxyEntry,
                                          const nsAString& aFontName);

    virtual gfxFontEntry* MakePlatformFont(const gfxProxyFontEntry *aProxyEntry,
                                           const uint8_t *aFontData,
                                           uint32_t aLength);

    void GetFontList(InfallibleTArray<FontListEntry>* retValue);

    static gfxFT2FontList* PlatformFontList() {
        return static_cast<gfxFT2FontList*>(gfxPlatformFontList::PlatformFontList());
    }

protected:
    virtual nsresult InitFontList();

    void AppendFaceFromFontListEntry(const FontListEntry& aFLE,
                                     bool isStdFile);

    void AppendFacesFromFontFile(nsCString& aFileName,
                                 bool isStdFile = false,
                                 FontNameCache *aCache = nullptr);

    void AppendFacesFromCachedFaceList(nsCString& aFileName,
                                       bool isStdFile,
                                       nsCString& aFaceList);

    void FindFonts();

#ifdef ANDROID
    void FindFontsInDir(const nsCString& aDir, FontNameCache* aFNC);
#endif
};

#endif /* GFX_FT2FONTLIST_H */
