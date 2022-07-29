/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTS_H
#define GFX_FT2FONTS_H

#include "mozilla/MemoryReporting.h"
#include "gfxTypes.h"
#include "gfxFont.h"
#include "gfxFT2FontBase.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"
#include "gfxUserFontSet.h"

class FT2FontEntry;

class gfxFT2Font final : public gfxFT2FontBase {
 public:  // new functions
  gfxFT2Font(const RefPtr<mozilla::gfx::UnscaledFontFreeType>& aUnscaledFont,
             RefPtr<mozilla::gfx::SharedFTFace>&& aFTFace,
             FT2FontEntry* aFontEntry, const gfxFontStyle* aFontStyle,
             int aLoadFlags);

  FT2FontEntry* GetFontEntry();

  already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(
      const TextRunDrawParams& aRunParams) override;

  bool ShouldHintMetrics() const override;

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const override;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const override;

 protected:
  ~gfxFT2Font() override;

  struct CachedGlyphData {
    CachedGlyphData() : glyphIndex(0xffffffffU) {}

    explicit CachedGlyphData(uint32_t gid) : glyphIndex(gid) {}

    uint32_t glyphIndex;
    int32_t lsbDelta;
    int32_t rsbDelta;
    int32_t xAdvance;
  };

  const CachedGlyphData* GetGlyphDataForChar(FT_Face aFace, uint32_t ch) {
    CharGlyphMapEntryType* entry = mCharGlyphCache.PutEntry(ch);

    if (!entry) return nullptr;

    if (entry->GetData().glyphIndex == 0xffffffffU) {
      // this is a new entry, fill it
      FillGlyphDataForChar(aFace, ch, entry->GetModifiableData());
    }

    return &entry->GetData();
  }

  bool ShapeText(DrawTarget* aDrawTarget, const char16_t* aText,
                 uint32_t aOffset, uint32_t aLength, Script aScript,
                 nsAtom* aLanguage, bool aVertical, RoundingFlags aRounding,
                 gfxShapedText* aShapedText) override;

  void FillGlyphDataForChar(FT_Face face, uint32_t ch, CachedGlyphData* gd);

  void AddRange(const char16_t* aText, uint32_t aOffset, uint32_t aLength,
                gfxShapedText* aShapedText);

  typedef nsBaseHashtableET<nsUint32HashKey, CachedGlyphData>
      CharGlyphMapEntryType;
  typedef nsTHashtable<CharGlyphMapEntryType> CharGlyphMap;
  CharGlyphMap mCharGlyphCache;
};

#endif /* GFX_FT2FONTS_H */
