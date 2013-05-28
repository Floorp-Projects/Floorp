/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WINDOWSDWRITEFONTS_H
#define GFX_WINDOWSDWRITEFONTS_H

#include <dwrite.h>

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "cairo-win32.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

/**
 * \brief Class representing a font face for a font entry.
 */
class gfxDWriteFont : public gfxFont 
{
public:
    gfxDWriteFont(gfxFontEntry *aFontEntry,
                  const gfxFontStyle *aFontStyle,
                  bool aNeedsBold = false,
                  AntialiasOption = kAntialiasDefault);
    ~gfxDWriteFont();

    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption);

    virtual const gfxFont::Metrics& GetMetrics();

    virtual uint32_t GetSpaceGlyph();

    virtual bool SetupCairoFont(gfxContext *aContext);

    virtual bool AllowSubpixelAA() { return mAllowManualShowGlyphs; }

    virtual bool IsValid();

    gfxFloat GetAdjustedSize() {
        return mAdjustedSize;
    }

    IDWriteFontFace *GetFontFace();

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    virtual bool ProvidesGlyphWidths();

    virtual int32_t GetGlyphWidth(gfxContext *aCtx, uint16_t aGID);

    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions> GetGlyphRenderingOptions();

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;

    virtual FontType GetType() const { return FONT_TYPE_DWRITE; }

    virtual mozilla::TemporaryRef<mozilla::gfx::ScaledFont> GetScaledFont(mozilla::gfx::DrawTarget *aTarget);

    virtual cairo_scaled_font_t *GetCairoScaledFont();

protected:
    friend class gfxDWriteShaper;

    virtual void CreatePlatformShaper();

    bool GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS *aFontMetrics);

    void ComputeMetrics(AntialiasOption anAAOption);

    bool HasBitmapStrikeForSize(uint32_t aSize);

    cairo_font_face_t *CairoFontFace();

    gfxFloat MeasureGlyphWidth(uint16_t aGlyph);

    DWRITE_MEASURING_MODE GetMeasuringMode();
    bool GetForceGDIClassic();

    nsRefPtr<IDWriteFontFace> mFontFace;
    cairo_font_face_t *mCairoFontFace;

    gfxFont::Metrics          *mMetrics;

    // cache of glyph widths in 16.16 fixed-point pixels
    nsDataHashtable<nsUint32HashKey,int32_t>    mGlyphWidths;

    bool mNeedsOblique;
    bool mNeedsBold;
    bool mUseSubpixelPositions;
    bool mAllowManualShowGlyphs;
    bool mAzureScaledFontIsCairo;
};

#endif
