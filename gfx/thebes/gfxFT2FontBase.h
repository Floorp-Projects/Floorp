/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
    gfxFT2FontBase(const RefPtr<mozilla::gfx::UnscaledFontFreeType>& aUnscaledFont,
                   cairo_scaled_font_t *aScaledFont,
                   gfxFontEntry *aFontEntry,
                   const gfxFontStyle *aFontStyle,
                   bool aEmbolden);
    virtual ~gfxFT2FontBase();

    uint32_t GetGlyph(uint32_t aCharCode);
    void GetGlyphExtents(uint32_t aGlyph,
                         cairo_text_extents_t* aExtents);
    virtual uint32_t GetSpaceGlyph() override;
    virtual bool ProvidesGetGlyph() const override { return true; }
    virtual uint32_t GetGlyph(uint32_t unicode,
                              uint32_t variation_selector) override;
    virtual bool ProvidesGlyphWidths() const override { return true; }
    virtual int32_t GetGlyphWidth(DrawTarget& aDrawTarget,
                                  uint16_t aGID) override;

    virtual bool SetupCairoFont(DrawTarget* aDrawTarget) override;

    virtual FontType GetType() const override { return FONT_TYPE_FT2; }

    static void SetupVarCoords(FT_Face aFace,
                               const nsTArray<gfxFontVariation>& aVariations,
                               nsTArray<FT_Fixed>* aCoords);

private:
    uint32_t GetCharExtents(char aChar, cairo_text_extents_t* aExtents);
    uint32_t GetCharWidth(char aChar, gfxFloat* aWidth);
    FT_Fixed GetFTGlyphAdvance(uint16_t aGID);
    void InitMetrics();

protected:
    virtual const Metrics& GetHorizontalMetrics() override;

    uint32_t mSpaceGlyph;
    Metrics mMetrics;
    bool    mEmbolden;

    // For variation/multiple-master fonts, this will be an array of the values
    // for each axis, as specified by mStyle.variationSettings (or the font's
    // default for axes not present in variationSettings). Values here are in
    // freetype's 16.16 fixed-point format, and clamped to the valid min/max
    // range reported by the face.
    nsTArray<FT_Fixed> mCoords;

    mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey,int32_t>> mGlyphWidths;
};

#endif /* GFX_FT2FONTBASE_H */
