/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMacFont.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"

#include "gfxCoreTextShaper.h"
#include <algorithm>
#include "gfxPlatformMac.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"
#include "gfxMacPlatformFontList.h"
#include "gfxFontConstants.h"
#include "gfxTextRun.h"
#include "nsCocoaFeatures.h"

#include "cairo-quartz.h"

using namespace mozilla;
using namespace mozilla::gfx;

gfxMacFont::gfxMacFont(const RefPtr<UnscaledFontMac>& aUnscaledFont,
                       MacOSFontEntry *aFontEntry,
                       const gfxFontStyle *aFontStyle,
                       bool aNeedsBold)
    : gfxFont(aUnscaledFont, aFontEntry, aFontStyle),
      mCGFont(nullptr),
      mCTFont(nullptr),
      mFontFace(nullptr),
      mFontSmoothingBackgroundColor(aFontStyle->fontSmoothingBackgroundColor),
      mVariationFont(aFontEntry->HasVariations())
{
    mApplySyntheticBold = aNeedsBold;

    if (mVariationFont && aFontStyle->variationSettings.Length() > 0) {
        CGFontRef baseFont = aUnscaledFont->GetFont();
        if (!baseFont) {
            mIsValid = false;
            return;
        }
        mCGFont =
            UnscaledFontMac::CreateCGFontWithVariations(
                baseFont,
                aFontStyle->variationSettings.Length(),
                aFontStyle->variationSettings.Elements());
        if (!mCGFont) {
          ::CFRetain(baseFont);
          mCGFont = baseFont;
        }
    } else {
        mCGFont = aUnscaledFont->GetFont();
        if (!mCGFont) {
            mIsValid = false;
            return;
        }
        ::CFRetain(mCGFont);
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
        SprintfLiteral(warnBuf,
                       "Failed to create Cairo font face: %s status: %d",
                       NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
        return;
    }

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    cairo_font_options_t *fontOptions = cairo_font_options_create();

    // turn off font anti-aliasing based on user pref setting
    if (mAdjustedSize <=
        (gfxFloat)gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_NONE);
        mAntialiasOption = kAntialiasNone;
    } else if (mStyle.useGrayscaleAntialiasing) {
        cairo_font_options_set_antialias(fontOptions, CAIRO_ANTIALIAS_GRAY);
        mAntialiasOption = kAntialiasGrayscale;
    }

    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix, &ctm,
                                           fontOptions);
    cairo_font_options_destroy(fontOptions);

    cairoerr = cairo_scaled_font_status(mScaledFont);
    if (cairoerr != CAIRO_STATUS_SUCCESS) {
        mIsValid = false;
#ifdef DEBUG
        char warnBuf[1024];
        SprintfLiteral(warnBuf, "Failed to create scaled font: %s status: %d",
                       NS_ConvertUTF16toUTF8(GetName()).get(), cairoerr);
        NS_WARNING(warnBuf);
#endif
    }
}

gfxMacFont::~gfxMacFont()
{
    if (mCGFont) {
        ::CFRelease(mCGFont);
    }
    if (mCTFont) {
        ::CFRelease(mCTFont);
    }
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }
}

bool
gfxMacFont::ShapeText(DrawTarget     *aDrawTarget,
                      const char16_t *aText,
                      uint32_t        aOffset,
                      uint32_t        aLength,
                      Script          aScript,
                      bool            aVertical,
                      RoundingFlags   aRounding,
                      gfxShapedText  *aShapedText)
{
    if (!mIsValid) {
        NS_WARNING("invalid font! expect incorrect text rendering");
        return false;
    }

    // Currently, we don't support vertical shaping via CoreText,
    // so we ignore RequiresAATLayout if vertical is requested.
    auto macFontEntry = static_cast<MacOSFontEntry*>(GetFontEntry());
    if (macFontEntry->RequiresAATLayout() &&
        !aVertical) {
        if (!mCoreTextShaper) {
            mCoreTextShaper = MakeUnique<gfxCoreTextShaper>(this);
        }
        if (mCoreTextShaper->ShapeText(aDrawTarget, aText, aOffset, aLength,
                                       aScript, aVertical, aRounding,
                                       aShapedText)) {
            PostShapingFixup(aDrawTarget, aText, aOffset,
                             aLength, aVertical, aShapedText);

            if (macFontEntry->HasTrackingTable()) {
                // Convert font size from device pixels back to CSS px
                // to use in selecting tracking value
                float trackSize = GetAdjustedSize() *
                    aShapedText->GetAppUnitsPerDevUnit() /
                    AppUnitsPerCSSPixel();
                float tracking =
                    macFontEntry->TrackingForCSSPx(trackSize) *
                    mFUnitsConvFactor;
                // Applying tracking is a lot like the adjustment we do for
                // synthetic bold: we want to apply between clusters, not to
                // non-spacing glyphs within a cluster. So we can reuse that
                // helper here.
                aShapedText->AdjustAdvancesForSyntheticBold(tracking,
                                                            aOffset, aLength);
            }

            return true;
        }
    }

    return gfxFont::ShapeText(aDrawTarget, aText, aOffset, aLength, aScript,
                              aVertical, aRounding, aShapedText);
}

bool
gfxMacFont::SetupCairoFont(DrawTarget* aDrawTarget)
{
    if (cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(gfxFont::RefCairo(aDrawTarget), mScaledFont);
    return true;
}

gfxFont::RunMetrics
gfxMacFont::Measure(const gfxTextRun *aTextRun,
                    uint32_t aStart, uint32_t aEnd,
                    BoundingBoxType aBoundingBoxType,
                    DrawTarget *aRefDrawTarget,
                    Spacing *aSpacing,
                    gfx::ShapedTextFlags aOrientation)
{
    gfxFont::RunMetrics metrics =
        gfxFont::Measure(aTextRun, aStart, aEnd,
                         aBoundingBoxType, aRefDrawTarget, aSpacing,
                         aOrientation);

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

    uint32_t upem = 0;

    // try to get unitsPerEm from sfnt head table, to avoid calling CGFont
    // if possible (bug 574368) and because CGFontGetUnitsPerEm does not
    // return the true value for OpenType/CFF fonts (it normalizes to 1000,
    // which then leads to metrics errors when we read the 'hmtx' table to
    // get glyph advances for HarfBuzz, see bug 580863)
    CFDataRef headData =
        ::CGFontCopyTableForTag(mCGFont, TRUETYPE_TAG('h','e','a','d'));
    if (headData) {
        if (size_t(::CFDataGetLength(headData)) >= sizeof(HeadTable)) {
            const HeadTable *head =
                reinterpret_cast<const HeadTable*>(::CFDataGetBytePtr(headData));
            upem = head->unitsPerEm;
        }
        ::CFRelease(headData);
    }
    if (!upem) {
        upem = ::CGFontGetUnitsPerEm(mCGFont);
    }

    if (upem < 16 || upem > 16384) {
        // See http://www.microsoft.com/typography/otspec/head.htm
#ifdef DEBUG
        char warnBuf[1024];
        SprintfLiteral(warnBuf,
                       "Bad font metrics for: %s (invalid unitsPerEm value)",
                       NS_ConvertUTF16toUTF8(mFontEntry->Name()).get());
        NS_WARNING(warnBuf);
#endif
        return;
    }

    mAdjustedSize = std::max(mStyle.size, 1.0);
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

    if (mMetrics.capHeight == 0.0) {
        mMetrics.capHeight = ::CGFontGetCapHeight(mCGFont) * cgConvFactor;
    }

    if (mStyle.sizeAdjust > 0.0 && mStyle.size > 0.0 &&
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

    uint32_t glyphID;
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
    fprintf (stderr, "    spaceWidth: %f aveCharWidth: %f xHeight: %f capHeight: %f\n", mMetrics.spaceWidth, mMetrics.aveCharWidth, mMetrics.xHeight, mMetrics.capHeight);
    fprintf (stderr, "    uOff: %f uSize: %f stOff: %f stSize: %f\n", mMetrics.underlineOffset, mMetrics.underlineSize, mMetrics.strikeoutOffset, mMetrics.strikeoutSize);
#endif
}

gfxFloat
gfxMacFont::GetCharWidth(CFDataRef aCmap, char16_t aUniChar,
                         uint32_t *aGlyphID, gfxFloat aConvFactor)
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

/* static */
CTFontRef
gfxMacFont::CreateCTFontFromCGFontWithVariations(CGFontRef aCGFont,
                                                 CGFloat aSize,
                                                 CTFontDescriptorRef aFontDesc)
{
    // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
    // versions (see bug 1331683)
    if (!nsCocoaFeatures::OnSierraOrLater()) {
        return CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr, aFontDesc);
    }

    CFDictionaryRef variations = ::CGFontCopyVariations(aCGFont);
    CTFontRef ctFont;
    if (variations) {
        CFDictionaryRef varAttr =
            ::CFDictionaryCreate(nullptr,
                                 (const void**)&kCTFontVariationAttribute,
                                 (const void**)&variations, 1,
                                 &kCFTypeDictionaryKeyCallBacks,
                                 &kCFTypeDictionaryValueCallBacks);
        ::CFRelease(variations);

        CTFontDescriptorRef varDesc = aFontDesc
            ? ::CTFontDescriptorCreateCopyWithAttributes(aFontDesc, varAttr)
            : ::CTFontDescriptorCreateWithAttributes(varAttr);
        ::CFRelease(varAttr);

        ctFont = ::CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr, varDesc);
        ::CFRelease(varDesc);
    } else {
        ctFont = ::CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr,
                                                aFontDesc);
    }
    return ctFont;
}

int32_t
gfxMacFont::GetGlyphWidth(DrawTarget& aDrawTarget, uint16_t aGID)
{
    if (!mCTFont) {
        mCTFont = CreateCTFontFromCGFontWithVariations(mCGFont, mAdjustedSize);
        if (!mCTFont) { // shouldn't happen, but let's be safe
            NS_WARNING("failed to create CTFontRef to measure glyph width");
            return 0;
        }
    }

    CGSize advance;
    ::CTFontGetAdvancesForGlyphs(mCTFont, kCTFontDefaultOrientation, &aGID,
                                 &advance, 1);
    return advance.width * 0x10000;
}

// Try to initialize font metrics via platform APIs (CG/CT),
// and set mIsValid = TRUE on success.
// We ONLY call this for local (platform) fonts that are not sfnt format;
// for sfnts, including ALL downloadable fonts, we prefer to use
// InitMetricsFromSfntTables and avoid platform APIs.
void
gfxMacFont::InitMetricsFromPlatform()
{
    CTFontRef ctFont = ::CTFontCreateWithGraphicsFont(mCGFont,
                                                      mAdjustedSize,
                                                      nullptr, nullptr);
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
    mMetrics.capHeight = ::CTFontGetCapHeight(ctFont);

    ::CFRelease(ctFont);

    mIsValid = true;
}

already_AddRefed<ScaledFont>
gfxMacFont::GetScaledFont(DrawTarget *aTarget)
{
    if (!mAzureScaledFont) {
        mAzureScaledFont =
            Factory::CreateScaledFontForMacFont(GetCGFontRef(),
                                                GetUnscaledFont(),
                                                GetAdjustedSize(),
                                                Color::FromABGR(mFontSmoothingBackgroundColor),
                                                !mStyle.useGrayscaleAntialiasing,
                                                IsSyntheticBold(),
                                                IsSyntheticOblique());
        if (!mAzureScaledFont) {
            return nullptr;
        }

        mAzureScaledFont->SetCairoScaledFont(mScaledFont);
    }

    RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
    return scaledFont.forget();
}

void
gfxMacFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                   FontCacheSizes* aSizes) const
{
    gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    // mCGFont is shared with the font entry, so not counted here;
    // and we don't have APIs to measure the cairo mFontFace object
}

void
gfxMacFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                   FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}
