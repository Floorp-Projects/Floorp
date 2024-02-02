/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FONT_PALETTE_CACHE_H
#define FONT_PALETTE_CACHE_H

#include "mozilla/gfx/Types.h"
#include "mozilla/MruCache.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/RefPtr.h"
#include "nsAtom.h"
#include "nsTArray.h"
#include <utility>

class gfxFontEntry;

namespace mozilla::gfx {

class FontPaletteValueSet;

// A resolved font palette as an array of colors.
class FontPalette {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FontPalette);

 public:
  FontPalette() = default;
  explicit FontPalette(nsTArray<mozilla::gfx::sRGBColor>&& aColors)
      : mColors(std::move(aColors)) {}

  const nsTArray<mozilla::gfx::sRGBColor>* Colors() const { return &mColors; }

 private:
  ~FontPalette() = default;

  nsTArray<mozilla::gfx::sRGBColor> mColors;
};

// MRU cache used for resolved color-font palettes, to avoid reconstructing
// the palette for each glyph rendered with a given font.
using CacheKey = std::pair<RefPtr<gfxFontEntry>, RefPtr<nsAtom>>;
struct CacheData {
  CacheKey mKey;
  RefPtr<FontPalette> mPalette;
};

class PaletteCache
    : public mozilla::MruCache<CacheKey, CacheData, PaletteCache> {
 public:
  explicit PaletteCache(const FontPaletteValueSet* aPaletteValueSet = nullptr)
      : mPaletteValueSet(aPaletteValueSet) {}

  void SetPaletteValueSet(const FontPaletteValueSet* aSet);

  already_AddRefed<FontPalette> GetPaletteFor(gfxFontEntry* aFontEntry,
                                              nsAtom* aPaletteName);

  static mozilla::HashNumber Hash(const CacheKey& aKey) {
    return mozilla::HashGeneric(aKey.first.get(), aKey.second.get());
  }
  static bool Match(const CacheKey& aKey, const CacheData& aVal) {
    return aVal.mKey == aKey;
  }

 protected:
  const FontPaletteValueSet* mPaletteValueSet = nullptr;
};

}  // namespace mozilla::gfx

#endif
