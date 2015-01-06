/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACFONT_H
#define GFX_MACFONT_H

#include "mozilla/MemoryReporting.h"
#include "gfxFont.h"
#include "cairo.h"
#include <ApplicationServices/ApplicationServices.h>

class MacOSFontEntry;

class gfxMacFont : public gfxFont
{
public:
    gfxMacFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
               bool aNeedsBold);

    virtual ~gfxMacFont();

    CGFontRef GetCGFontRef() const { return mCGFont; }

    /* overrides for the pure virtual methods in gfxFont */
    virtual uint32_t GetSpaceGlyph() MOZ_OVERRIDE {
        return mSpaceGlyph;
    }

    virtual bool SetupCairoFont(gfxContext *aContext) MOZ_OVERRIDE;

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing,
                               uint16_t aOrientation) MOZ_OVERRIDE;

    // We need to provide hinted (non-linear) glyph widths if using a font
    // with embedded color bitmaps (Apple Color Emoji), as Core Text renders
    // the glyphs with non-linear scaling at small pixel sizes.
    virtual bool ProvidesGlyphWidths() const MOZ_OVERRIDE {
        return mFontEntry->HasFontTable(TRUETYPE_TAG('s','b','i','x'));
    }

    virtual int32_t GetGlyphWidth(DrawTarget& aDrawTarget,
                                  uint16_t aGID) MOZ_OVERRIDE;

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont>
    GetScaledFont(mozilla::gfx::DrawTarget *aTarget) MOZ_OVERRIDE;

    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions>
      GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams = nullptr) MOZ_OVERRIDE;

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const MOZ_OVERRIDE;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const MOZ_OVERRIDE;

    virtual FontType GetType() const MOZ_OVERRIDE { return FONT_TYPE_MAC; }

protected:
    virtual const Metrics& GetHorizontalMetrics() MOZ_OVERRIDE {
        return mMetrics;
    }

    // override to prefer CoreText shaping with fonts that depend on AAT
    virtual bool ShapeText(gfxContext     *aContext,
                           const char16_t *aText,
                           uint32_t        aOffset,
                           uint32_t        aLength,
                           int32_t         aScript,
                           bool            aVertical,
                           gfxShapedText  *aShapedText) MOZ_OVERRIDE;

    void InitMetrics();
    void InitMetricsFromPlatform();

    // Get width and glyph ID for a character; uses aConvFactor
    // to convert font units as returned by CG to actual dimensions
    gfxFloat GetCharWidth(CFDataRef aCmap, char16_t aUniChar,
                          uint32_t *aGlyphID, gfxFloat aConvFactor);

    // a weak reference to the CoreGraphics font: this is owned by the
    // MacOSFontEntry, it is not retained or released by gfxMacFont
    CGFontRef             mCGFont;

    // a Core Text font reference, created only if we're using CT to measure
    // glyph widths; otherwise null.
    CTFontRef             mCTFont;

    cairo_font_face_t    *mFontFace;

    nsAutoPtr<gfxFontShaper> mCoreTextShaper;

    Metrics               mMetrics;
    uint32_t              mSpaceGlyph;
};

#endif /* GFX_MACFONT_H */
