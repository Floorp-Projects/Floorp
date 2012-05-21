/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GDIFONT_H
#define GFX_GDIFONT_H

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

    virtual PRUint32 GetSpaceGlyph();

    virtual bool SetupCairoFont(gfxContext *aContext);

    /* override Measure to add padding for antialiasing */
    virtual RunMetrics Measure(gfxTextRun *aTextRun,
                               PRUint32 aStart, PRUint32 aEnd,
                               BoundingBoxType aBoundingBoxType,
                               gfxContext *aContextForTightBoundingBox,
                               Spacing *aSpacing);

    /* required for MathML to suppress effects of ClearType "padding" */
    virtual gfxFont* CopyWithAntialiasOption(AntialiasOption anAAOption);

    virtual bool ProvidesGlyphWidths() { return true; }

    // get hinted glyph width in pixels as 16.16 fixed-point value
    virtual PRInt32 GetGlyphWidth(gfxContext *aCtx, PRUint16 aGID);

    virtual void SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;
    virtual void SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                     FontCacheSizes*   aSizes) const;

    virtual FontType GetType() const { return FONT_TYPE_GDI; }

protected:
    virtual void CreatePlatformShaper();

    /* override to check for uniscribe failure and fall back to GDI */
    virtual bool ShapeWord(gfxContext *aContext,
                           gfxShapedWord *aShapedWord,
                           const PRUnichar *aString,
                           bool aPreferPlatformShaping = false);

    void Initialize(); // creates metrics and Cairo fonts

    void FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize);

    // mPlatformShaper is used for the GDI shaper, mUniscribeShaper
    // for the Uniscribe version if needed
    nsAutoPtr<gfxFontShaper>   mUniscribeShaper;

    HFONT                 mFont;
    cairo_font_face_t    *mFontFace;

    Metrics              *mMetrics;
    PRUint32              mSpaceGlyph;

    bool                  mNeedsBold;

    // cache of glyph widths in 16.16 fixed-point pixels
    nsDataHashtable<nsUint32HashKey,PRInt32>    mGlyphWidths;
};

#endif /* GFX_GDIFONT_H */
