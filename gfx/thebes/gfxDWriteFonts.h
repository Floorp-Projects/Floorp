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

    virtual PRUint32 GetSpaceGlyph();

    virtual bool SetupCairoFont(gfxContext *aContext);

    virtual bool AllowSubpixelAA() { return mAllowManualShowGlyphs; }

    virtual bool IsValid();

    gfxFloat GetAdjustedSize() {
        return mAdjustedSize;
    }

    IDWriteFontFace *GetFontFace();

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               PRUint32 aStart, PRUint32 aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    // override gfxFont table access function to bypass gfxFontEntry cache,
    // use DWrite API to get direct access to system font data
    virtual hb_blob_t *GetFontTable(PRUint32 aTag);

    virtual bool ProvidesGlyphWidths();

    virtual PRInt32 GetGlyphWidth(gfxContext *aCtx, PRUint16 aGID);

    virtual mozilla::TemporaryRef<mozilla::gfx::GlyphRenderingOptions> GetGlyphRenderingOptions();

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;

    virtual FontType GetType() const { return FONT_TYPE_DWRITE; }

protected:
    friend class gfxDWriteShaper;

    virtual void CreatePlatformShaper();

    bool GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS *aFontMetrics);

    void ComputeMetrics(AntialiasOption anAAOption);

    bool HasBitmapStrikeForSize(PRUint32 aSize);

    cairo_font_face_t *CairoFontFace();

    cairo_scaled_font_t *CairoScaledFont();

    gfxFloat MeasureGlyphWidth(PRUint16 aGlyph);

    static void DestroyBlobFunc(void* userArg);

    DWRITE_MEASURING_MODE GetMeasuringMode();
    bool GetForceGDIClassic();

    nsRefPtr<IDWriteFontFace> mFontFace;
    cairo_font_face_t *mCairoFontFace;

    gfxFont::Metrics          *mMetrics;

    // cache of glyph widths in 16.16 fixed-point pixels
    nsDataHashtable<nsUint32HashKey,PRInt32>    mGlyphWidths;

    bool mNeedsOblique;
    bool mNeedsBold;
    bool mUseSubpixelPositions;
    bool mAllowManualShowGlyphs;
};

#endif
