/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FontPaletteCache.h"
#include "COLRFonts.h"

using namespace mozilla;

void gfx::PaletteCache::SetPaletteValueSet(
    const gfx::FontPaletteValueSet* aSet) {
  mPaletteValueSet = aSet;
  Clear();
}

nsTArray<gfx::sRGBColor>* gfx::PaletteCache::GetPaletteFor(
    gfxFontEntry* aFontEntry, nsAtom* aPaletteName) {
  auto entry = Lookup(std::pair(aFontEntry, aPaletteName));
  if (!entry) {
    CacheData newData;
    newData.mKey = std::pair(aFontEntry, aPaletteName);

    gfxFontEntry::AutoHBFace face = aFontEntry->GetHBFace();
    newData.mPalette = gfx::COLRFonts::SetupColorPalette(
        face, mPaletteValueSet, aPaletteName, aFontEntry->FamilyName());

    entry.Set(std::move(newData));
  }
  return entry.Data().mPalette.get();
}
