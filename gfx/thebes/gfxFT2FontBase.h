/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_FT2FONTBASE_H
#define GFX_FT2FONTBASE_H

#include "cairo.h"
#include "gfxContext.h"
#include "gfxFont.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/UnscaledFontFreeType.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

class gfxFT2FontBase : public gfxFont {
 public:
  gfxFT2FontBase(
      const RefPtr<mozilla::gfx::UnscaledFontFreeType>& aUnscaledFont,
      cairo_scaled_font_t* aScaledFont, gfxFontEntry* aFontEntry,
      const gfxFontStyle* aFontStyle);
  virtual ~gfxFT2FontBase();

  uint32_t GetGlyph(uint32_t aCharCode);
  void GetGlyphExtents(uint32_t aGlyph, cairo_text_extents_t* aExtents);
  uint32_t GetSpaceGlyph() override;
  bool ProvidesGetGlyph() const override { return true; }
  virtual uint32_t GetGlyph(uint32_t unicode,
                            uint32_t variation_selector) override;
  bool ProvidesGlyphWidths() const override { return true; }
  int32_t GetGlyphWidth(uint16_t aGID) override;

  bool SetupCairoFont(DrawTarget* aDrawTarget) override;

  FontType GetType() const override { return FONT_TYPE_FT2; }

  static void SetupVarCoords(FT_MM_Var* aMMVar,
                             const nsTArray<gfxFontVariation>& aVariations,
                             FT_Face aFTFace);

 private:
  uint32_t GetCharExtents(char aChar, cairo_text_extents_t* aExtents);
  uint32_t GetCharWidth(char aChar, gfxFloat* aWidth);

  // Get advance of a single glyph from FreeType, and return true;
  // or return false if we should fall back to getting the glyph
  // extents from cairo instead.
  bool GetFTGlyphAdvance(uint16_t aGID, int32_t* aWidth);

  void InitMetrics();

 protected:
  const Metrics& GetHorizontalMetrics() override;

  uint32_t mSpaceGlyph;
  Metrics mMetrics;
  bool mEmbolden;

  // For variation/multiple-master fonts, this will be an array of the values
  // for each axis, as specified by mStyle.variationSettings (or the font's
  // default for axes not present in variationSettings). Values here are in
  // freetype's 16.16 fixed-point format, and clamped to the valid min/max
  // range reported by the face.
  nsTArray<FT_Fixed> mCoords;

  mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey, int32_t>> mGlyphWidths;
};

// Helper classes used for clearing out user font data when FT font
// face is destroyed. Since multiple faces may use the same data, be
// careful to assure that the data is only cleared out when all uses
// expire. The font entry object contains a refptr to FTUserFontData and
// each FT face created from that font entry contains a refptr to that
// same FTUserFontData object.

class FTUserFontData final
    : public mozilla::gfx::SharedFTFaceRefCountedData<FTUserFontData> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FTUserFontData)

  FTUserFontData(const uint8_t* aData, uint32_t aLength)
      : mFontData(aData), mLength(aLength) {}

  const uint8_t* FontData() const { return mFontData; }

  already_AddRefed<mozilla::gfx::SharedFTFace> CloneFace(
      int aFaceIndex = 0) override;

 private:
  ~FTUserFontData() {
    if (mFontData) {
      free((void*)mFontData);
    }
  }

  const uint8_t* mFontData;
  uint32_t mLength;
};

#endif /* GFX_FT2FONTBASE_H */
