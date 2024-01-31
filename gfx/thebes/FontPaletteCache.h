/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef FONT_PALETTE_CACHE_H
#define FONT_PALETTE_CACHE_H

#include "gfxFontEntry.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/MruCache.h"
#include "mozilla/HashFunctions.h"
#include "nsAtom.h"
#include <utility>

namespace mozilla::gfx {

class FontPaletteValueSet;

// MRU cache used for resolved color-font palettes, to avoid reconstructing
// the palette for each glyph rendered with a given font.
using CacheKey = std::pair<RefPtr<gfxFontEntry>, RefPtr<nsAtom>>;
struct CacheData {
  CacheKey mKey;
  mozilla::UniquePtr<nsTArray<mozilla::gfx::sRGBColor>> mPalette;
};

class PaletteCache
    : public mozilla::MruCache<CacheKey, CacheData, PaletteCache> {
 public:
  explicit PaletteCache(const FontPaletteValueSet* aPaletteValueSet = nullptr)
      : mPaletteValueSet(aPaletteValueSet) {}

  void SetPaletteValueSet(const FontPaletteValueSet* aSet);

  nsTArray<mozilla::gfx::sRGBColor>* GetPaletteFor(gfxFontEntry* aFontEntry,
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
