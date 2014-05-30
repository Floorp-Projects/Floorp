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
    virtual const gfxFont::Metrics& GetMetrics();
    virtual uint32_t GetSpaceGlyph();
    virtual bool ProvidesGetGlyph() const { return true; }
    virtual uint32_t GetGlyph(uint32_t unicode, uint32_t variation_selector);
    virtual bool ProvidesGlyphWidths() const { return true; }
    virtual int32_t GetGlyphWidth(gfxContext *aCtx, uint16_t aGID);

    cairo_scaled_font_t *CairoScaledFont() { return mScaledFont; };
    virtual bool SetupCairoFont(gfxContext *aContext);

    virtual FontType GetType() const { return FONT_TYPE_FT2; }

    mozilla::gfx::FontOptions* GetFontOptions() { return &mFontOptions; }
protected:
    uint32_t mSpaceGlyph;
    bool mHasMetrics;
    Metrics mMetrics;

    // Azure font description
    mozilla::gfx::FontOptions  mFontOptions;
    void ConstructFontOptions();
};

#endif /* GFX_FT2FONTBASE_H */
