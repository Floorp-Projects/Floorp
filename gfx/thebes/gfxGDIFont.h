/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GDIFONT_H
#define GFX_GDIFONT_H

#include "mozilla/MemoryReporting.h"
#include "gfxFont.h"
#include "gfxGDIFontList.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

#include "cairo.h"
#include "usp10.h"

class gfxGDIFont : public gfxFont
{
public:
    gfxGDIFont(GDIFontEntry *aFontEntry,
               const gfxFontStyle *aFontStyle,
               bool aNeedsBold,
               AntialiasOption anAAOption = kAntialiasDefault);

    virtual ~gfxGDIFont();

    HFONT GetHFONT() { return mFont; }

    cairo_font_face_t   *CairoFontFace() { return mFontFace; }
    cairo_scaled_font_t *CairoScaledFont() { return mScaledFont; }

    /* overrides for the pure virtual methods in gfxFont */
    virtual uint32_t GetSpaceGlyph() override;

    virtual bool SetupCairoFont(DrawTarget* aDrawTarget) override;

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(const gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               DrawTarget *aDrawTargetForTightBoundingBox,
                               Spacing *aSpacing,
                               uint16_t aOrientation) override;

    /* required for MathML to suppress effects of ClearType "padding" */
    mozilla::UniquePtr<gfxFont>
    CopyWithAntialiasOption(AntialiasOption anAAOption) override;

    // If the font has a cmap table, we handle it purely with harfbuzz;
    // but if not (e.g. .fon fonts), we'll use a GDI callback to get glyphs.
    virtual bool ProvidesGetGlyph() const override {
        return !mFontEntry->HasCmapTable();
    }

    virtual uint32_t GetGlyph(uint32_t aUnicode,
                              uint32_t aVarSelector) override;

    virtual bool ProvidesGlyphWidths() const override { return true; }

    // get hinted glyph width in pixels as 16.16 fixed-point value
    virtual int32_t GetGlyphWidth(DrawTarget& aDrawTarget,
                                  uint16_t aGID) override;

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;

    virtual FontType GetType() const override { return FONT_TYPE_GDI; }

protected:
    virtual const Metrics& GetHorizontalMetrics() override;

    /* override to ensure the cairo font is set up properly */
    virtual bool ShapeText(DrawTarget     *aDrawTarget,
                           const char16_t *aText,
                           uint32_t        aOffset,
                           uint32_t        aLength,
                           Script          aScript,
                           bool            aVertical,
                           gfxShapedText  *aShapedText) override;

    void Initialize(); // creates metrics and Cairo fonts

    // Fill the given LOGFONT record according to our style, but don't adjust
    // the lfItalic field if we're going to use a cairo transform for fake
    // italics.
    void FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize, bool aUseGDIFakeItalic);

    HFONT                 mFont;
    cairo_font_face_t    *mFontFace;

    Metrics              *mMetrics;
    uint32_t              mSpaceGlyph;

    bool                  mNeedsBold;

    // cache of glyph IDs (used for non-sfnt fonts only)
    mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey,uint32_t> > mGlyphIDs;
    SCRIPT_CACHE          mScriptCache;

    // cache of glyph widths in 16.16 fixed-point pixels
    mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey,int32_t> > mGlyphWidths;
};

#endif /* GFX_GDIFONT_H */
