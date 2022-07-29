/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GDIFONT_H
#define GFX_GDIFONT_H

#include "mozilla/MemoryReporting.h"
#include "gfxFont.h"
#include "gfxGDIFontList.h"

#include "nsTHashMap.h"
#include "nsHashKeys.h"

#include "usp10.h"

class gfxGDIFont final : public gfxFont {
 public:
  gfxGDIFont(GDIFontEntry* aFontEntry, const gfxFontStyle* aFontStyle,
             AntialiasOption anAAOption = kAntialiasDefault);

  HFONT GetHFONT() const { return mFont; }

  already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(
      const TextRunDrawParams& aRunParams) override;

  /* override Measure to add padding for antialiasing */
  RunMetrics Measure(const gfxTextRun* aTextRun, uint32_t aStart, uint32_t aEnd,
                     BoundingBoxType aBoundingBoxType,
                     DrawTarget* aDrawTargetForTightBoundingBox,
                     Spacing* aSpacing,
                     mozilla::gfx::ShapedTextFlags aOrientation) override;

  /* required for MathML to suppress effects of ClearType "padding" */
  gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption) const override;

  // If the font has a cmap table, we handle it purely with harfbuzz;
  // but if not (e.g. .fon fonts), we'll use a GDI callback to get glyphs.
  bool ProvidesGetGlyph() const override { return !mFontEntry->HasCmapTable(); }

  uint32_t GetGlyph(uint32_t aUnicode, uint32_t aVarSelector) override;

  bool ProvidesGlyphWidths() const override { return true; }

  // get hinted glyph width in pixels as 16.16 fixed-point value
  int32_t GetGlyphWidth(uint16_t aGID) override;

  bool GetGlyphBounds(uint16_t aGID, gfxRect* aBounds,
                      bool aTight) const override;

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const;

  FontType GetType() const override { return FONT_TYPE_GDI; }

 protected:
  ~gfxGDIFont() override;

  const Metrics& GetHorizontalMetrics() const override { return *mMetrics; }

  bool ShapeText(DrawTarget* aDrawTarget, const char16_t* aText,
                 uint32_t aOffset, uint32_t aLength, Script aScript,
                 nsAtom* aLanguage, bool aVertical, RoundingFlags aRounding,
                 gfxShapedText* aShapedText) override;

  void Initialize();  // creates metrics and Cairo fonts

  // Fill the given LOGFONT record according to our size.
  // (Synthetic italic is *not* handled here, because GDI may not reliably
  // use the face we expect if we tweak the lfItalic field, and because we
  // have generic support for this in gfxFont::Draw instead.)
  void FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize);

  HFONT mFont;

  Metrics* mMetrics;
  bool mIsBitmap;

  bool mNeedsSyntheticBold;

  // cache of glyph IDs (used for non-sfnt fonts only)
  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, uint32_t> > mGlyphIDs;
  SCRIPT_CACHE mScriptCache;

  // cache of glyph widths in 16.16 fixed-point pixels
  mozilla::UniquePtr<nsTHashMap<nsUint32HashKey, int32_t> > mGlyphWidths;
};

#endif /* GFX_GDIFONT_H */
