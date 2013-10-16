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

class gfxGDIFont : public gfxFont
{
public:
    gfxGDIFont(GDIFontEntry *aFontEntry,
               const gfxFontStyle *aFontStyle,
               bool aNeedsBold,
               AntialiasOption anAAOption = kAntialiasDefault);

    virtual ~gfxGDIFont();

    HFONT GetHFONT() { if (!mMetrics) Initialize(); return mFont; }

    gfxFloat GetAdjustedSize() { if (!mMetrics) Initialize(); return mAdjustedSize; }

    cairo_font_face_t   *CairoFontFace() { return mFontFace; }
    cairo_scaled_font_t *CairoScaledFont() { return mScaledFont; }

    /* overrides for the pure virtual methods in gfxFont */
    virtual const gfxFont::Metrics& GetMetrics();

    virtual uint32_t GetSpaceGlyph();

    virtual bool SetupCairoFont(gfxContext *aContext);

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               uint32_t aStart, uint32_t aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    /* required for MathML to suppress effects of ClearType "padding" */
    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption);

    virtual bool ProvidesGlyphWidths() { return true; }

    // get hinted glyph width in pixels as 16.16 fixed-point value
    virtual int32_t GetGlyphWidth(gfxContext *aCtx, uint16_t aGID);

    virtual void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;
    virtual void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const;

    virtual FontType GetType() const { return FONT_TYPE_GDI; }

protected:
    virtual void CreatePlatformShaper();

    /* override to check for uniscribe failure and fall back to GDI */
    virtual bool ShapeText(gfxContext      *aContext,
                           const PRUnichar *aText,
                           uint32_t         aOffset,
                           uint32_t         aLength,
                           int32_t          aScript,
                           gfxShapedText   *aShapedText,
                           bool             aPreferPlatformShaping);

    void Initialize(); // creates metrics and Cairo fonts

    // Fill the given LOGFONT record according to our style, but don't adjust
    // the lfItalic field if we're going to use a cairo transform for fake
    // italics.
    void FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize, bool aUseGDIFakeItalic);

    // mPlatformShaper is used for the GDI shaper, mUniscribeShaper
    // for the Uniscribe version if needed
    nsAutoPtr<gfxFontShaper>   mUniscribeShaper;

    HFONT                 mFont;
    cairo_font_face_t    *mFontFace;

    Metrics              *mMetrics;
    uint32_t              mSpaceGlyph;

    bool                  mNeedsBold;

    // cache of glyph widths in 16.16 fixed-point pixels
    nsAutoPtr<nsDataHashtable<nsUint32HashKey,int32_t> > mGlyphWidths;
};

#endif /* GFX_GDIFONT_H */
