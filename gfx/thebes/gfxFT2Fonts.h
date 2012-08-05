/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTS_H
#define GFX_FT2FONTS_H

#include "cairo.h"
#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxFT2FontBase.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"
#include "gfxUserFontSet.h"

class FT2FontEntry;

class gfxFT2Font : public gfxFT2FontBase {
public: // new functions
    gfxFT2Font(cairo_scaled_font_t *aCairoFont,
               FT2FontEntry *aFontEntry,
               const gfxFontStyle *aFontStyle,
               bool aNeedsBold);
    virtual ~gfxFT2Font ();

    cairo_font_face_t *CairoFontFace();

    FT2FontEntry *GetFontEntry();

    static already_AddRefed<gfxFT2Font>
    GetOrMakeFont(const nsAString& aName, const gfxFontStyle *aStyle,
                  bool aNeedsBold = false);

    static already_AddRefed<gfxFT2Font>
    GetOrMakeFont(FT2FontEntry *aFontEntry, const gfxFontStyle *aStyle,
                  bool aNeedsBold = false);

    struct CachedGlyphData {
        CachedGlyphData()
            : glyphIndex(0xffffffffU) { }

        CachedGlyphData(uint32_t gid)
            : glyphIndex(gid) { }

        uint32_t glyphIndex;
        int32_t lsbDelta;
        int32_t rsbDelta;
        int32_t xAdvance;
    };

    const CachedGlyphData* GetGlyphDataForChar(uint32_t ch) {
        CharGlyphMapEntryType *entry = mCharGlyphCache.PutEntry(ch);

        if (!entry)
            return nullptr;

        if (entry->mData.glyphIndex == 0xffffffffU) {
            // this is a new entry, fill it
            FillGlyphDataForChar(ch, &entry->mData);
        }

        return &entry->mData;
    }

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;

#ifdef USE_SKIA
    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions> GetGlyphRenderingOptions();
#endif

protected:
    virtual bool ShapeText(gfxContext      *aContext,
                           const PRUnichar *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText,
                           bool             aPreferPlatformShaping);

    void FillGlyphDataForChar(uint32_t ch, CachedGlyphData *gd);

    void AddRange(const PRUnichar *aText,
                  uint32_t         aOffset,
                  uint32_t         aLength,
                  gfxShapedText   *aShapedText);

    typedef nsBaseHashtableET<nsUint32HashKey, CachedGlyphData> CharGlyphMapEntryType;
    typedef nsTHashtable<CharGlyphMapEntryType> CharGlyphMap;
    CharGlyphMap mCharGlyphCache;
};

#ifndef ANDROID // not needed on Android, uses the standard gfxFontGroup directly
class THEBES_API gfxFT2FontGroup : public gfxFontGroup {
public: // new functions
    gfxFT2FontGroup (const nsAString& families,
                    const gfxFontStyle *aStyle,
                    gfxUserFontSet *aUserFontSet);
    virtual ~gfxFT2FontGroup ();

protected: // from gfxFontGroup

    virtual gfxFontGroup *Copy(const gfxFontStyle *aStyle);


protected: // new functions

    static bool FontCallback (const nsAString & fontName, 
                                const nsACString & genericName, 
                                bool aUseFontSet,
                                void *closure);
    bool mEnableKerning;

    void GetPrefFonts(nsIAtom *aLangGroup,
                      nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList);
    void GetCJKPrefFonts(nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList);
    void FamilyListToArrayList(const nsString& aFamilies,
                               nsIAtom *aLangGroup,
                               nsTArray<nsRefPtr<gfxFontEntry> > *aFontEntryList);
    already_AddRefed<gfxFT2Font> WhichFontSupportsChar(const nsTArray<nsRefPtr<gfxFontEntry> >& aFontEntryList,
                                                       uint32_t aCh);
    already_AddRefed<gfxFont> WhichPrefFontSupportsChar(uint32_t aCh);
    already_AddRefed<gfxFont>
        WhichSystemFontSupportsChar(uint32_t aCh, int32_t aRunScript);

    nsTArray<gfxTextRange> mRanges;
    nsString mString;
};
#endif // !ANDROID

#endif /* GFX_FT2FONTS_H */

