/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_MACFONT_H
#define GFX_MACFONT_H

#include "gfxFont.h"
#include "gfxMacPlatformFontList.h"
#include "mozilla/gfx/2D.h"

#include "cairo.h"

class gfxMacFont : public gfxFont
{
public:
    gfxMacFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
               bool aNeedsBold);

    virtual ~gfxMacFont();

    CGFontRef GetCGFontRef() const { return mCGFont; }

    /* overrides for the pure virtual methods in gfxFont */
    virtual const gfxFont::Metrics& GetMetrics() {
        return mMetrics;
    }

    virtual uint32_t GetSpaceGlyph() {
        return mSpaceGlyph;
    }

    virtual bool SetupCairoFont(gfxContext *aContext);

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont> GetScaledFont(mozilla::gfx::DrawTarget *aTarget);

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;

    virtual FontType GetType() const { return FONT_TYPE_MAC; }

protected:
    virtual void CreatePlatformShaper();

    // override to prefer CoreText shaping with fonts that depend on AAT
    virtual bool ShapeText(gfxContext      *aContext,
                           const PRUnichar *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText,
                           bool             aPreferPlatformShaping = false);

    void InitMetrics();
    void InitMetricsFromPlatform();

    // Get width and glyph ID for a character; uses aConvFactor
    // to convert font units as returned by CG to actual dimensions
    gfxFloat GetCharWidth(CFDataRef aCmap, PRUnichar aUniChar,
                          uint32_t *aGlyphID, gfxFloat aConvFactor);

    // a weak reference to the CoreGraphics font: this is owned by the
    // MacOSFontEntry, it is not retained or released by gfxMacFont
    CGFontRef             mCGFont;

    cairo_font_face_t    *mFontFace;

    Metrics               mMetrics;
    uint32_t              mSpaceGlyph;
};

#endif /* GFX_MACFONT_H */
