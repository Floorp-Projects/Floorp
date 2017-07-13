/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WINDOWSDWRITEFONTS_H
#define GFX_WINDOWSDWRITEFONTS_H

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include <dwrite_1.h>

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "cairo-win32.h"

#include "nsDataHashtable.h"
#include "nsHashKeys.h"

#include "mozilla/gfx/UnscaledFontDWrite.h"

/**
 * \brief Class representing a font face for a font entry.
 */
class gfxDWriteFont : public gfxFont 
{
public:
    gfxDWriteFont(const RefPtr<mozilla::gfx::UnscaledFontDWrite>& aUnscaledFont,
                  gfxFontEntry *aFontEntry,
                  const gfxFontStyle *aFontStyle,
                  bool aNeedsBold = false,
                  AntialiasOption = kAntialiasDefault);
    ~gfxDWriteFont();

    static void UpdateClearTypeUsage();

    mozilla::UniquePtr<gfxFont>
    CopyWithAntialiasOption(AntialiasOption anAAOption) override;

    virtual uint32_t GetSpaceGlyph() override;

    virtual bool SetupCairoFont(DrawTarget* aDrawTarget) override;

    virtual bool AllowSubpixelAA() override
    { return mAllowManualShowGlyphs; }

    bool IsValid() const;

    IDWriteFontFace *GetFontFace();

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(const gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               DrawTarget *aDrawTargetForTightBoundingBox,
                               Spacing *aSpacing,
                               mozilla::gfx::ShapedTextFlags aOrientation) override;

    virtual bool ProvidesGlyphWidths() const override;

    virtual int32_t GetGlyphWidth(DrawTarget& aDrawTarget,
                                  uint16_t aGID) override;

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const override;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const override;

    virtual FontType GetType() const override { return FONT_TYPE_DWRITE; }

    virtual already_AddRefed<mozilla::gfx::ScaledFont>
    GetScaledFont(mozilla::gfx::DrawTarget *aTarget) override;

    virtual cairo_scaled_font_t *GetCairoScaledFont() override;

protected:
    virtual const Metrics& GetHorizontalMetrics() override;

    bool GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS *aFontMetrics);

    void ComputeMetrics(AntialiasOption anAAOption);

    bool HasBitmapStrikeForSize(uint32_t aSize);

    cairo_font_face_t *CairoFontFace();

    gfxFloat MeasureGlyphWidth(uint16_t aGlyph);

    DWRITE_MEASURING_MODE GetMeasuringMode();
    bool GetForceGDIClassic();

    RefPtr<IDWriteFontFace> mFontFace;
    RefPtr<IDWriteFontFace1> mFontFace1; // may be unavailable on older DWrite

    cairo_font_face_t *mCairoFontFace;

    Metrics *mMetrics;

    // cache of glyph widths in 16.16 fixed-point pixels
    mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey,int32_t>> mGlyphWidths;

    uint32_t mSpaceGlyph;

    bool mNeedsOblique;
    bool mNeedsBold;
    bool mUseSubpixelPositions;
    bool mAllowManualShowGlyphs;
    bool mAzureScaledFontIsCairo;
    static bool mUseClearType;
};

#endif
