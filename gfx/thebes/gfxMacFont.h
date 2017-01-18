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
    uint32_t GetSpaceGlyph() override {
        return mSpaceGlyph;
    }

    bool SetupCairoFont(DrawTarget* aDrawTarget) override;

    /* override Measure to add padding for antialiasing */
    RunMetrics Measure(const gfxTextRun *aTextRun,
                       uint32_t aStart, uint32_t aEnd,
                       BoundingBoxType aBoundingBoxType,
                       DrawTarget *aDrawTargetForTightBoundingBox,
                       Spacing *aSpacing,
                       uint16_t aOrientation) override;

    // We need to provide hinted (non-linear) glyph widths if using a font
    // with embedded color bitmaps (Apple Color Emoji), as Core Text renders
    // the glyphs with non-linear scaling at small pixel sizes.
    bool ProvidesGlyphWidths() const override {
        return mVariationFont ||
               mFontEntry->HasFontTable(TRUETYPE_TAG('s','b','i','x'));
    }

    int32_t GetGlyphWidth(DrawTarget& aDrawTarget,
                          uint16_t aGID) override;

    already_AddRefed<mozilla::gfx::ScaledFont>
    GetScaledFont(mozilla::gfx::DrawTarget *aTarget) override;

    already_AddRefed<mozilla::gfx::GlyphRenderingOptions>
      GetGlyphRenderingOptions(const TextRunDrawParams* aRunParams = nullptr) override;

    void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const override;
    void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                FontCacheSizes* aSizes) const override;

    FontType GetType() const override { return FONT_TYPE_MAC; }

    // Helper to create a CTFont from a CGFont, with optional font descriptor
    // (for features), and copying any variations that were set on the CGFont.
    // This is public so that gfxCoreTextShaper can also use it.
    static CTFontRef
    CreateCTFontFromCGFontWithVariations(CGFontRef aCGFont,
                                         CGFloat aSize,
                                         CTFontDescriptorRef aFontDesc = nullptr);

protected:
    const Metrics& GetHorizontalMetrics() override {
        return mMetrics;
    }

    // override to prefer CoreText shaping with fonts that depend on AAT
    bool ShapeText(DrawTarget     *aDrawTarget,
                   const char16_t *aText,
                   uint32_t        aOffset,
                   uint32_t        aLength,
                   Script          aScript,
                   bool            aVertical,
                   gfxShapedText  *aShapedText) override;

    void InitMetrics();
    void InitMetricsFromPlatform();

    // Get width and glyph ID for a character; uses aConvFactor
    // to convert font units as returned by CG to actual dimensions
    gfxFloat GetCharWidth(CFDataRef aCmap, char16_t aUniChar,
                          uint32_t *aGlyphID, gfxFloat aConvFactor);

    // a strong reference to the CoreGraphics font
    CGFontRef             mCGFont;

    // a Core Text font reference, created only if we're using CT to measure
    // glyph widths; otherwise null.
    CTFontRef             mCTFont;

    cairo_font_face_t    *mFontFace;

    mozilla::UniquePtr<gfxFontShaper> mCoreTextShaper;

    Metrics               mMetrics;
    uint32_t              mSpaceGlyph;

    bool                  mVariationFont; // true if variations are in effect
};

#endif /* GFX_MACFONT_H */
