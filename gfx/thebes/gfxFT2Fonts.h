/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTS_H
#define GFX_FT2FONTS_H

#include "mozilla/MemoryReporting.h"
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
    gfxFT2Font(const RefPtr<mozilla::gfx::UnscaledFontFreeType>& aUnscaledFont,
               cairo_scaled_font_t *aCairoFont,
               FT_Face aFTFace,
               FT2FontEntry *aFontEntry,
               const gfxFontStyle *aFontStyle);
    virtual ~gfxFT2Font ();

    FT2FontEntry *GetFontEntry();

    virtual already_AddRefed<mozilla::gfx::ScaledFont>
    GetScaledFont(DrawTarget *aTarget) override;

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const override;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const override;

protected:
    struct CachedGlyphData {
        CachedGlyphData()
            : glyphIndex(0xffffffffU) { }

        explicit CachedGlyphData(uint32_t gid)
            : glyphIndex(gid) { }

        uint32_t glyphIndex;
        int32_t lsbDelta;
        int32_t rsbDelta;
        int32_t xAdvance;
    };

    const CachedGlyphData* GetGlyphDataForChar(FT_Face aFace, uint32_t ch) {
        CharGlyphMapEntryType *entry = mCharGlyphCache.PutEntry(ch);

        if (!entry)
            return nullptr;

        if (entry->mData.glyphIndex == 0xffffffffU) {
            // this is a new entry, fill it
            FillGlyphDataForChar(aFace, ch, &entry->mData);
        }

        return &entry->mData;
    }

    bool ShapeText(DrawTarget      *aDrawTarget,
                   const char16_t  *aText,
                   uint32_t         aOffset,
                   uint32_t         aLength,
                   Script           aScript,
                   bool             aVertical,
                   RoundingFlags    aRounding,
                   gfxShapedText   *aShapedText) override;

    void FillGlyphDataForChar(FT_Face face, uint32_t ch, CachedGlyphData *gd);

    void AddRange(const char16_t *aText,
                  uint32_t         aOffset,
                  uint32_t         aLength,
                  gfxShapedText   *aShapedText);

    typedef nsBaseHashtableET<nsUint32HashKey, CachedGlyphData> CharGlyphMapEntryType;
    typedef nsTHashtable<CharGlyphMapEntryType> CharGlyphMap;
    CharGlyphMap mCharGlyphCache;
    FT_Face mFTFace;
};

#endif /* GFX_FT2FONTS_H */

