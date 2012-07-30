/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMacFont.h"
#include "gfxCoreTextShaper.h"
#include "gfxHarfBuzzShaper.h"
#ifdef MOZ_GRAPHITE
#include "gfxGraphiteShaper.h"
#endif
#include "gfxPlatformMac.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"

#include "cairo-quartz.h"

using namespace mozilla;
using namespace mozilla::gfx;

gfxMacFont::gfxMacFont(MacOSFontEntry *aFontEntry, const gfxFontStyle *aFontStyle,
                       bool aNeedsBold)
    : gfxFont(aFontEntry, aFontStyle),
      mCGFont(nullptr),
      mFontFace(nullptr)
{
    mApplySyntheticBold = aNeedsBold;

    mCGFont = aFontEntry->GetFontRef();
    if (!mCGFont) {
        mIsValid = false;
        return;
    }

    // InitMetrics will handle the sizeAdjust factor and set mAdjustedSize
    InitMetrics();
    if (!mIsValid) {
        return;
    }

    mFontFace = cairo_quartz_font_face_create_for_cgfont(mCGFont);

    cairo_status_t cairoerr = cairo_font_face_status(mFontFace);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = false;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create Cairo font face: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
        return;
    }

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    // synthetic oblique by skewing via the font matrix
    bool needsOblique =
        (mFontEntry != NULL) &&
        (!mFontEntry->IsItalic() &&
         (mStyle.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)));

    if (needsOblique) {
        double skewfactor = (needsOblique ? Fix2X(kATSItalicQDSkew) : 0);

        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * skewfactor,   //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <=
        (gfxFloat)gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE);
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm,
                                           fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = false;
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d",
                NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }

#ifdef MOZ_GRAPHITE
    if (FontCanSupportGraphite()) {
        mGraphiteShaper = new gfxGraphiteShaper(this);
    }
#endif
    if (FontCanSupportHarfBuzz()) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
}

gfxMacFont::~gfxMacFont()
{
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }
}

bool
gfxMacFont::ShapeWord(gfxContext *aContext,
                      gfxShapedWord *aShapedWord,
                      const PRUnichar *aText,
                      bool aPreferPlatformShaping)
{
    if (!mIsValid) {
        NS_WARNING("invalid font! expect incorrect text rendering");
        return false;
    }

    bool requiresAAT =
        static_cast<MacOSFontEntry*>(GetFontEntry())->RequiresAATLayout();
    return gfxFont::ShapeWord(aContext, aShapedWord, aText, requiresAAT);
}

void
gfxMacFont::CreatePlatformShaper()
{
    mPlatformShaper = new gfxCoreTextShaper(this);
}

bool
gfxMacFont::SetupCairoFont(gfxContext *aContext)
{
    if (cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(aContext->GetCairo(), mScaledFont);
    return true;
}

gfxFont::RunMetrics
gfxMacFont::Measure(gfxTextRun *aTextRun,
                    PRUint32 aStart, PRUint32 aEnd,
                    BoundingBoxType aBoundingBoxType,
                    gfxContext *aRefContext,
                    Spacing *aSpacing)
{
    gfxFont::RunMetrics metrics =
        gfxFont::Measure(aTextRun, aStart, aEnd,
                         aBoundingBoxType, aRefContext, aSpacing);

    // if aBoundingBoxType is not TIGHT_HINTED_OUTLINE_EXTENTS then we need to add
    // a pixel column each side of the bounding box in case of antialiasing "bleed"
    if (aBoundingBoxType != TIGHT_HINTED_OUTLINE_EXTENTS &&
        metrics.mBoundingBox.width > 0) {
        metrics.mBoundingBox.x -= aTextRun->GetAppUnitsPerDevUnit();
        metrics.mBoundingBox.width += aTextRun->GetAppUnitsPerDevUnit() * 2;
    }

    return metrics;
}

void
gfxMacFont::InitMetrics()
{
    mIsValid = false;
    ::memset(&mMetrics, 0, sizeof(mMetrics));

    PRUint32 upem = 0;

    // try to get unitsPerEm from sfnt head table, to avoid calling CGFont
    // if possible (bug 574368) and because CGFontGetUnitsPerEm does not
    // return the true value for OpenType/CFF fonts (it normalizes to 1000,
    // which then leads to metrics errors when we read the 'hmtx' table to
    // get glyph advances for HarfBuzz, see bug 580863)
    const PRUint32 kHeadTableTag = TRUETYPE_TAG('h','e','a','d');
    AutoFallibleTArray<PRUint8,sizeof(HeadTable)> headData;
    if (NS_SUCCEEDED(mFontEntry->GetFontTable(kHeadTableTag, headData)) &&
        headData.Length() >= sizeof(HeadTable)) {
        HeadTable *head = reinterpret_cast<HeadTable*>(headData.Elements());
        upem = head->unitsPerEm;
    } else {
        upem = ::CGFontGetUnitsPerEm(mCGFont);
    }

    if (upem < 16 || upem > 16384) {
        // See http://www.microsoft.com/typography/otspec/head.htm
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s (invalid unitsPerEm value)",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get());
        NS_WARNING(warnBuf);
#endif
        return;
    }

    mAdjustedSize = NS_MAX(mStyle.size, 1.0);
    mFUnitsConvFactor = mAdjustedSize / upem;

    // For CFF fonts, when scaling values read from CGFont* APIs, we need to
    // use CG's idea of unitsPerEm, which may differ from the "true" value in
    // the head table of the font (see bug 580863)
    gfxFloat cgConvFactor;
    if (static_cast<MacOSFontEntry*>(mFontEntry.get())->IsCFF()) {
        cgConvFactor = mAdjustedSize / ::CGFontGetUnitsPerEm(mCGFont);
    } else {
        cgConvFactor = mFUnitsConvFactor;
    }

    // Try to read 'sfnt' metrics; for local, non-sfnt fonts ONLY, fall back to
    // platform APIs. The InitMetrics...() functions will set mIsValid on success.
    if (!InitMetricsFromSfntTables(mMetrics) &&
        (!mFontEntry->IsUserFont() || mFontEntry->IsLocalUserFont())) {
        InitMetricsFromPlatform();
    }
    if (!mIsValid) {
        return;
    }

    if (mMetrics.xHeight == 0.0) {
        mMetrics.xHeight = ::CGFontGetXHeight(mCGFont) * cgConvFactor;
    }

    if (mStyle.sizeAdjust != 0.0 && mStyle.size > 0.0 &&
        mMetrics.xHeight > 0.0) {
        // apply font-size-adjust, and recalculate metrics
        gfxFloat aspect = mMetrics.xHeight / mStyle.size;
        mAdjustedSize = mStyle.GetAdjustedSize(aspect);
        mFUnitsConvFactor = mAdjustedSize / upem;
        if (static_cast<MacOSFontEntry*>(mFontEntry.get())->IsCFF()) {
            cgConvFactor = mAdjustedSize / ::CGFontGetUnitsPerEm(mCGFont);
        } else {
            cgConvFactor = mFUnitsConvFactor;
        }
        mMetrics.xHeight = 0.0;
        if (!InitMetricsFromSfntTables(mMetrics) &&
            (!mFontEntry->IsUserFont() || mFontEntry->IsLocalUserFont())) {
            InitMetricsFromPlatform();
        }
        if (!mIsValid) {
            // this shouldn't happen, as we succeeded earlier before applying
            // the size-adjust factor! But check anyway, for paranoia's sake.
            return;
        }
        if (mMetrics.xHeight == 0.0) {
            mMetrics.xHeight = ::CGFontGetXHeight(mCGFont) * cgConvFactor;
        }
    }

    // Once we reach here, we've got basic metrics and set mIsValid = TRUE;
    // there should be no further points of actual failure in InitMetrics().
    // (If one is introduced, be sure to reset mIsValid to FALSE!)

    mMetrics.emHeight = mAdjustedSize;

    // Measure/calculate additional metrics, independent of whether we used
    // the tables directly or ATS metrics APIs

    CFDataRef cmap =
        ::CGFontCopyTableForTag(mCGFont, TRUETYPE_TAG('c','m','a','p'));

    PRUint32 glyphID;
    if (mMetrics.aveCharWidth <= 0) {
        mMetrics.aveCharWidth = GetCharWidth(cmap, 'x', &glyphID,
                                             cgConvFactor);
        if (glyphID == 0) {
            // we didn't find 'x', so use maxAdvance rather than zero
            mMetrics.aveCharWidth = mMetrics.maxAdvance;
        }
    }
    if (IsSyntheticBold()) {
        mMetrics.aveCharWidth += GetSyntheticBoldOffset();
        mMetrics.maxAdvance += GetSyntheticBoldOffset();
    }

    mMetrics.spaceWidth = GetCharWidth(cmap, ' ', &glyphID, cgConvFactor);
    if (glyphID == 0) {
        // no space glyph?!
        mMetrics.spaceWidth = mMetrics.aveCharWidth;
    }
    mSpaceGlyph = glyphID;

    mMetrics.zeroOrAveCharWidth = GetCharWidth(cmap, '0', &glyphID,
                                               cgConvFactor);
    if (glyphID == 0) {
        mMetrics.zeroOrAveCharWidth = mMetrics.aveCharWidth;
    }

    if (cmap) {
        ::CFRelease(cmap);
    }

    CalculateDerivedMetrics(mMetrics);

    SanitizeMetrics(&mMetrics, mFontEntry->mIsBadUnderlineFont);

#if 0
    fprintf (stderr, "Font: %p (%s) size: %f\n", this,
             NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
//    fprintf (stderr, "    fbounds.origin.x %f y %f size.width %f height %f\n", fbounds.origin.x, fbounds.origin.y, fbounds.size.width, fbounds.size.height);
    fprintf (stderr, "    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics.emHeight, mMetrics.emAscent, mMetrics.emDescent);
    fprintf (stderr, "    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics.maxAscent, mMetrics.maxDescent, mMetrics.maxAdvance);
    fprintf (stderr, "    internalLeading: %f externalLeading: %f\n", mMetrics.internalLeading, mMetrics.externalLeading);
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize, mMetrics.superscriptOffset, mMetrics.subscriptOffset);
#endif
}

gfxFloat
gfxMacFont::GetCharWidth(CFDataRef aCmap, PRUnichar aUniChar,
                         PRUint32 *aGlyphID, gfxFloat aConvFactor)
{
    CGGlyph glyph = 0;
    
    if (aCmap) {
        glyph = gfxFontUtils::MapCharToGlyph(::CFDataGetBytePtr(aCmap),
                                             ::CFDataGetLength(aCmap),
                                             aUniChar);
    }

    if (aGlyphID) {
        *aGlyphID = glyph;
    }

    if (glyph) {
        int advance;
        if (::CGFontGetGlyphAdvances(mCGFont, &glyph, 1, &advance)) {
            return advance * aConvFactor;
        }
    }

    return 0;
}

/*static*/ void
gfxMacFont::DestroyBlobFunc(void* aUserData)
{
    ::CFRelease((CFDataRef)aUserData);
}

hb_blob_t *
gfxMacFont::GetFontTable(PRUint32 aTag)
{
    CFDataRef dataRef = ::CGFontCopyTableForTag(mCGFont, aTag);
    if (dataRef) {
        return hb_blob_create((const char*)::CFDataGetBytePtr(dataRef),
                              ::CFDataGetLength(dataRef),
                              HB_MEMORY_MODE_READONLY,
                              (void*)dataRef, DestroyBlobFunc);
    }

    if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
        // for downloaded fonts, there may be layout tables cached in the entry
        // even though they're absent from the sanitized platform font
        hb_blob_t *blob;
        if (mFontEntry->GetExistingFontTable(aTag, &blob)) {
            return blob;
        }
    }

    return nullptr;
}

// Try to initialize font metrics via platform APIs (CG/CT),
// and set mIsValid = TRUE on success.
// We ONLY call this for local (platform) fonts that are not sfnt format;
// for sfnts, including ALL downloadable fonts, we prefer to use
// InitMetricsFromSfntTables and avoid platform APIs.
void
gfxMacFont::InitMetricsFromPlatform()
{
    if (gfxMacPlatformFontList::UseATSFontEntry()) {
        ATSFontEntry *fe = static_cast<ATSFontEntry*>(GetFontEntry());
        InitMetricsFromATSMetrics(fe->GetATSFontRef());
        return;
    }

    CTFontRef ctFont = ::CTFontCreateWithGraphicsFont(mCGFont,
                                                      mAdjustedSize,
                                                      NULL, NULL);
    if (!ctFont) {
        return;
    }

    mMetrics.underlineOffset = ::CTFontGetUnderlinePosition(ctFont);
    mMetrics.underlineSize = ::CTFontGetUnderlineThickness(ctFont);

    mMetrics.externalLeading = ::CTFontGetLeading(ctFont);

    mMetrics.maxAscent = ::CTFontGetAscent(ctFont);
    mMetrics.maxDescent = ::CTFontGetDescent(ctFont);

    // this is not strictly correct, but neither CTFont nor CGFont seems to
    // provide maxAdvance, unless we were to iterate over all the glyphs
    // (which isn't worth the cost here)
    CGRect r = ::CTFontGetBoundingBox(ctFont);
    mMetrics.maxAdvance = r.size.width;

    // aveCharWidth is also not provided, so leave it at zero
    // (fallback code in gfxMacFont::InitMetrics will then try measuring 'x');
    // this could lead to less-than-"perfect" text field sizing when width is
    // specified as a number of characters, and the font in use is a non-sfnt
    // legacy font, but that's a sufficiently obscure edge case that we can
    // ignore the potential discrepancy.
    mMetrics.aveCharWidth = 0;

    mMetrics.xHeight = ::CTFontGetXHeight(ctFont);

    ::CFRelease(ctFont);

    mIsValid = true;
}

// For OS X 10.5, try to initialize font metrics via ATS font metrics APIs,
// and set mIsValid = TRUE on success.
void
gfxMacFont::InitMetricsFromATSMetrics(ATSFontRef aFontRef)
{
    ATSFontMetrics atsMetrics;
    OSStatus err;

    err = ::ATSFontGetHorizontalMetrics(aFontRef, kATSOptionFlagsDefault,
                                        &atsMetrics);
    if (err != noErr) {
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Bad font metrics for: %s err: %8.8x",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(), PRUint32(err));
        NS_WARNING(warnBuf);
#endif
        return;
    }

    mMetrics.underlineOffset = atsMetrics.underlinePosition * mAdjustedSize;
    mMetrics.underlineSize = atsMetrics.underlineThickness * mAdjustedSize;

    mMetrics.externalLeading = atsMetrics.leading * mAdjustedSize;

    mMetrics.maxAscent = atsMetrics.ascent * mAdjustedSize;
    mMetrics.maxDescent = -atsMetrics.descent * mAdjustedSize;

    mMetrics.maxAdvance = atsMetrics.maxAdvanceWidth * mAdjustedSize;
    mMetrics.aveCharWidth = atsMetrics.avgAdvanceWidth * mAdjustedSize;
    mMetrics.xHeight = atsMetrics.xHeight * mAdjustedSize;

    mIsValid = true;
}

RefPtr<ScaledFont>
gfxMacFont::GetScaledFont()
{
  if (!mAzureFont) {
    NativeFont nativeFont;
    nativeFont.mType = NATIVE_FONT_MAC_FONT_FACE;
    nativeFont.mFont = GetCGFontRef();
    mAzureFont = mozilla::gfx::Factory::CreateScaledFontWithCairo(nativeFont, GetAdjustedSize(), mScaledFont);
  }

  return mAzureFont;
}

void
gfxMacFont::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    gfxFont::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    // mCGFont is shared with the font entry, so not counted here;
    // and we don't have APIs to measure the cairo mFontFace object
}

void
gfxMacFont::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
