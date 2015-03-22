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

class gfxFT2FontBase : public gfxFont {
public:
    gfxFT2FontBase(cairo_scaled_font_t *aScaledFont,
                   gfxFontEntry *aFontEntry,
                   const gfxFontStyle *aFontStyle);
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

    cairo_scaled_font_t *CairoScaledFont() { return mScaledFont; };
    virtual bool SetupCairoFont(gfxContext *aContext) override;

    virtual FontType GetType() const override { return FONT_TYPE_FT2; }

protected:
    virtual const Metrics& GetHorizontalMetrics() override;

    uint32_t mSpaceGlyph;
    bool mHasMetrics;
    Metrics mMetrics;
};

#endif /* GFX_FT2FONTBASE_H */
