/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxGDIFont.h"
#include "gfxGDIShaper.h"
#include "gfxUniscribeShaper.h"
#include "gfxHarfBuzzShaper.h"
#ifdef MOZ_GRAPHITE
#include "gfxGraphiteShaper.h"
#endif
#include "gfxWindowsPlatform.h"
#include "gfxContext.h"

#include "cairo-win32.h"

#define ROUND(x) floor((x) + 0.5)

static inline cairo_antialias_t
GetCairoAntialiasOption(gfxFont::AntialiasOption anAntialiasOption)
{
    switch (anAntialiasOption) {
    default:
    case gfxFont::kAntialiasDefault:
        return CAIRO_ANTIALIAS_DEFAULT;
    case gfxFont::kAntialiasNone:
        return CAIRO_ANTIALIAS_NONE;
    case gfxFont::kAntialiasGrayscale:
        return CAIRO_ANTIALIAS_GRAY;
    case gfxFont::kAntialiasSubpixel:
        return CAIRO_ANTIALIAS_SUBPIXEL;
    }
}

gfxGDIFont::gfxGDIFont(GDIFontEntry *aFontEntry,
                       const gfxFontStyle *aFontStyle,
                       bool aNeedsBold,
                       AntialiasOption anAAOption)
    : gfxFont(aFontEntry, aFontStyle, anAAOption),
      mFont(NULL),
      mFontFace(nullptr),
      mMetrics(nullptr),
      mSpaceGlyph(0),
      mNeedsBold(aNeedsBold)
{
#ifdef MOZ_GRAPHITE
    if (FontCanSupportGraphite()) {
        mGraphiteShaper = new gfxGraphiteShaper(this);
    }
#endif
    if (FontCanSupportHarfBuzz()) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
}

gfxGDIFont::~gfxGDIFont()
{
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    if (mFontFace) {
        cairo_font_face_destroy(mFontFace);
    }
    if (mFont) {
        ::DeleteObject(mFont);
    }
    delete mMetrics;
}

void
gfxGDIFont::CreatePlatformShaper()
{
    mPlatformShaper = new gfxGDIShaper(this);
}

gfxFont*
gfxGDIFont::CopyWithAntialiasOption(AntialiasOption anAAOption)
{
    return new gfxGDIFont(static_cast<GDIFontEntry*>(mFontEntry.get()),
                          &mStyle, mNeedsBold, anAAOption);
}

static bool
UseUniscribe(gfxShapedText *aShapedText,
             const PRUnichar *aText,
             uint32_t aLength)
{
    uint32_t flags = aShapedText->Flags();
    bool useGDI;

    bool isXP = (gfxWindowsPlatform::WindowsOSVersion() 
                       < gfxWindowsPlatform::kWindowsVista);

    // bug 561304 - Uniscribe bug produces bad positioning at certain
    // font sizes on XP, so default to GDI on XP using logic of 3.6

    useGDI = isXP &&
             (flags &
               (gfxTextRunFactory::TEXT_OPTIMIZE_SPEED | 
                gfxTextRunFactory::TEXT_IS_RTL)
             ) == gfxTextRunFactory::TEXT_OPTIMIZE_SPEED;

    return !useGDI ||
        ScriptIsComplex(aText, aLength, SIC_COMPLEX) == S_OK;
}

bool
gfxGDIFont::ShapeText(gfxContext      *aContext,
                      const PRUnichar *aText,
                      uint32_t         aOffset,
                      uint32_t         aLength,
                      int32_t          aScript,
                      gfxShapedText   *aShapedText,
                      bool             aPreferPlatformShaping)
{
    if (!mMetrics) {
        Initialize();
    }
    if (!mIsValid) {
        NS_WARNING("invalid font! expect incorrect text rendering");
        return false;
    }

    bool ok = false;

    // Ensure the cairo font is set up, so there's no risk it'll fall back to
    // creating a "toy" font internally (see bug 544617).
    // We must check that this succeeded, otherwise we risk cairo creating the
    // wrong kind of font internally as a fallback (bug 744480).
    if (!SetupCairoFont(aContext)) {
        return false;
    }

#ifdef MOZ_GRAPHITE
    if (mGraphiteShaper && gfxPlatform::GetPlatform()->UseGraphiteShaping()) {
        ok = mGraphiteShaper->ShapeText(aContext, aText,
                                        aOffset, aLength,
                                        aScript, aShapedText);
    }
#endif

    if (!ok && mHarfBuzzShaper) {
        if (gfxPlatform::GetPlatform()->UseHarfBuzzForScript(aScript)) {
            ok = mHarfBuzzShaper->ShapeText(aContext, aText, aOffset, aLength,
                                            aScript, aShapedText);
        }
    }

    if (!ok) {
        GDIFontEntry *fe = static_cast<GDIFontEntry*>(GetFontEntry());
        bool preferUniscribe =
            (!fe->IsTrueType() || fe->IsSymbolFont()) && !fe->mForceGDI;

        if (preferUniscribe || UseUniscribe(aShapedText, aText, aLength)) {
            // first try Uniscribe
            if (!mUniscribeShaper) {
                mUniscribeShaper = new gfxUniscribeShaper(this);
            }

            ok = mUniscribeShaper->ShapeText(aContext, aText, aOffset, aLength,
                                             aScript, aShapedText);
            if (!ok) {
                // fallback to GDI shaping
                if (!mPlatformShaper) {
                    CreatePlatformShaper();
                }

                ok = mPlatformShaper->ShapeText(aContext, aText, aOffset,
                                                aLength, aScript, aShapedText);
            }
        } else {
            // first use GDI
            if (!mPlatformShaper) {
                CreatePlatformShaper();
            }

            ok = mPlatformShaper->ShapeText(aContext, aText, aOffset, aLength,
                                            aScript, aShapedText);
            if (!ok) {
                // try Uniscribe if GDI failed
                if (!mUniscribeShaper) {
                    mUniscribeShaper = new gfxUniscribeShaper(this);
                }

                // use Uniscribe shaping
                ok = mUniscribeShaper->ShapeText(aContext, aText,
                                                 aOffset, aLength,
                                                 aScript, aShapedText);
            }
        }

#if DEBUG
        if (!ok) {
            NS_ConvertUTF16toUTF8 name(GetName());
            char msg[256];

            sprintf(msg, 
                    "text shaping with both uniscribe and GDI failed for"
                    " font: %s",
                    name.get());
            NS_WARNING(msg);
        }
#endif
    }

    PostShapingFixup(aContext, aText, aOffset, aLength, aShapedText);

    return ok;
}

const gfxFont::Metrics&
gfxGDIFont::GetMetrics()
{
    if (!mMetrics) {
        Initialize();
    }
    return *mMetrics;
}

uint32_t
gfxGDIFont::GetSpaceGlyph()
{
    if (!mMetrics) {
        Initialize();
    }
    return mSpaceGlyph;
}

bool
gfxGDIFont::SetupCairoFont(gfxContext *aContext)
{
    if (!mMetrics) {
        Initialize();
    }
    if (!mScaledFont ||
        cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(aContext->GetCairo(), mScaledFont);
    return true;
}

gfxFont::RunMetrics
gfxGDIFont::Measure(gfxTextRun *aTextRun,
                    uint32_t aStart, uint32_t aEnd,
                    BoundingBoxType aBoundingBoxType,
                    gfxContext *aRefContext,
                    Spacing *aSpacing)
{
    gfxFont::RunMetrics metrics =
        gfxFont::Measure(aTextRun, aStart, aEnd,
                         aBoundingBoxType, aRefContext, aSpacing);

    // if aBoundingBoxType is LOOSE_INK_EXTENTS
    // and the underlying cairo font may be antialiased,
    // we can't trust Windows to have considered all the pixels
    // so we need to add "padding" to the bounds.
    // (see bugs 475968, 439831, compare also bug 445087)
    if (aBoundingBoxType == LOOSE_INK_EXTENTS &&
        mAntialiasOption != kAntialiasNone &&
        metrics.mBoundingBox.width > 0) {
        metrics.mBoundingBox.x -= aTextRun->GetAppUnitsPerDevUnit();
        metrics.mBoundingBox.width += aTextRun->GetAppUnitsPerDevUnit() * 3;
    }

    return metrics;
}

#define OBLIQUE_SKEW_FACTOR 0.3

void
gfxGDIFont::Initialize()
{
    NS_ASSERTION(!mMetrics, "re-creating metrics? this will leak");

    LOGFONTW logFont;

    // Figure out if we want to do synthetic oblique styling.
    GDIFontEntry* fe = static_cast<GDIFontEntry*>(GetFontEntry());
    bool wantFakeItalic =
        (mStyle.style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) &&
        !fe->IsItalic();

    // If the font's family has an actual italic face (but font matching
    // didn't choose it), we have to use a cairo transform instead of asking
    // GDI to italicize, because that would use a different face and result
    // in a possible glyph ID mismatch between shaping and rendering.
    //
    // We use the mFamilyHasItalicFace flag in the entry in case of user fonts,
    // where the *CSS* family may not know about italic faces that are present
    // in the *GDI* family, and which GDI would use if we asked it to perform
    // the "italicization".
    bool useCairoFakeItalic = wantFakeItalic && fe->mFamilyHasItalicFace;

    if (mAdjustedSize == 0.0) {
        mAdjustedSize = mStyle.size;
        if (mStyle.sizeAdjust != 0.0 && mAdjustedSize > 0.0) {
            // to implement font-size-adjust, we first create the "unadjusted" font
            FillLogFont(logFont, mAdjustedSize,
                        wantFakeItalic && !useCairoFakeItalic);
            mFont = ::CreateFontIndirectW(&logFont);

            // initialize its metrics so we can calculate size adjustment
            Initialize();

            // calculate the properly adjusted size, and then proceed
            // to recreate mFont and recalculate metrics
            gfxFloat aspect = mMetrics->xHeight / mMetrics->emHeight;
            mAdjustedSize = mStyle.GetAdjustedSize(aspect);

            // delete the temporary font and metrics
            ::DeleteObject(mFont);
            mFont = nullptr;
            delete mMetrics;
            mMetrics = nullptr;
        }
    }

    // (bug 724231) for local user fonts, we don't use GDI's synthetic bold,
    // as it could lead to a different, incompatible face being used
    // but instead do our own multi-striking
    if (mNeedsBold && GetFontEntry()->IsLocalUserFont()) {
        mApplySyntheticBold = true;
    }

    // this may end up being zero
    mAdjustedSize = ROUND(mAdjustedSize);
    FillLogFont(logFont, mAdjustedSize, wantFakeItalic && !useCairoFakeItalic);
    mFont = ::CreateFontIndirectW(&logFont);

    mMetrics = new gfxFont::Metrics;
    ::memset(mMetrics, 0, sizeof(*mMetrics));

    AutoDC dc;
    SetGraphicsMode(dc.GetDC(), GM_ADVANCED);
    AutoSelectFont selectFont(dc.GetDC(), mFont);

    // Get font metrics if size > 0
    if (mAdjustedSize > 0.0) {

        OUTLINETEXTMETRIC oMetrics;
        TEXTMETRIC& metrics = oMetrics.otmTextMetrics;

        if (0 < GetOutlineTextMetrics(dc.GetDC(), sizeof(oMetrics), &oMetrics)) {
            mMetrics->superscriptOffset = (double)oMetrics.otmptSuperscriptOffset.y;
            // Some fonts have wrong sign on their subscript offset, bug 410917.
            mMetrics->subscriptOffset = fabs((double)oMetrics.otmptSubscriptOffset.y);
            mMetrics->strikeoutSize = (double)oMetrics.otmsStrikeoutSize;
            mMetrics->strikeoutOffset = (double)oMetrics.otmsStrikeoutPosition;
            mMetrics->underlineSize = (double)oMetrics.otmsUnderscoreSize;
            mMetrics->underlineOffset = (double)oMetrics.otmsUnderscorePosition;

            const MAT2 kIdentityMatrix = { {0, 1}, {0, 0}, {0, 0}, {0, 1} };
            GLYPHMETRICS gm;
            DWORD len = GetGlyphOutlineW(dc.GetDC(), PRUnichar('x'), GGO_METRICS, &gm, 0, nullptr, &kIdentityMatrix);
            if (len == GDI_ERROR || gm.gmptGlyphOrigin.y <= 0) {
                // 56% of ascent, best guess for true type
                mMetrics->xHeight =
                    ROUND((double)metrics.tmAscent * DEFAULT_XHEIGHT_FACTOR);
            } else {
                mMetrics->xHeight = gm.gmptGlyphOrigin.y;
            }
            mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
            gfxFloat typEmHeight = (double)oMetrics.otmAscent - (double)oMetrics.otmDescent;
            mMetrics->emAscent = ROUND(mMetrics->emHeight * (double)oMetrics.otmAscent / typEmHeight);
            mMetrics->emDescent = mMetrics->emHeight - mMetrics->emAscent;
            if (oMetrics.otmEMSquare > 0) {
                mFUnitsConvFactor = float(mAdjustedSize / oMetrics.otmEMSquare);
            }
        } else {
            // Make a best-effort guess at extended metrics
            // this is based on general typographic guidelines

            // GetTextMetrics can fail if the font file has been removed
            // or corrupted recently.
            BOOL result = GetTextMetrics(dc.GetDC(), &metrics);
            if (!result) {
                NS_WARNING("Missing or corrupt font data, fasten your seatbelt");
                mIsValid = false;
                memset(mMetrics, 0, sizeof(*mMetrics));
                return;
            }

            mMetrics->xHeight =
                ROUND((float)metrics.tmAscent * DEFAULT_XHEIGHT_FACTOR);
            mMetrics->superscriptOffset = mMetrics->xHeight;
            mMetrics->subscriptOffset = mMetrics->xHeight;
            mMetrics->strikeoutSize = 1;
            mMetrics->strikeoutOffset = ROUND(mMetrics->xHeight * 0.5f); // 50% of xHeight
            mMetrics->underlineSize = 1;
            mMetrics->underlineOffset = -ROUND((float)metrics.tmDescent * 0.30f); // 30% of descent
            mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
            mMetrics->emAscent = metrics.tmAscent - metrics.tmInternalLeading;
            mMetrics->emDescent = metrics.tmDescent;
        }

        mMetrics->internalLeading = metrics.tmInternalLeading;
        mMetrics->externalLeading = metrics.tmExternalLeading;
        mMetrics->maxHeight = metrics.tmHeight;
        mMetrics->maxAscent = metrics.tmAscent;
        mMetrics->maxDescent = metrics.tmDescent;
        mMetrics->maxAdvance = metrics.tmMaxCharWidth;
        mMetrics->aveCharWidth = NS_MAX<gfxFloat>(1, metrics.tmAveCharWidth);
        // The font is monospace when TMPF_FIXED_PITCH is *not* set!
        // See http://msdn2.microsoft.com/en-us/library/ms534202(VS.85).aspx
        if (!(metrics.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
            mMetrics->maxAdvance = mMetrics->aveCharWidth;
        }

        // Cache the width of a single space.
        SIZE size;
        GetTextExtentPoint32W(dc.GetDC(), L" ", 1, &size);
        mMetrics->spaceWidth = ROUND(size.cx);

        // Cache the width of digit zero.
        // XXX MSDN (http://msdn.microsoft.com/en-us/library/ms534223.aspx)
        // does not say what the failure modes for GetTextExtentPoint32 are -
        // is it safe to assume it will fail iff the font has no '0'?
        if (GetTextExtentPoint32W(dc.GetDC(), L"0", 1, &size)) {
            mMetrics->zeroOrAveCharWidth = ROUND(size.cx);
        } else {
            mMetrics->zeroOrAveCharWidth = mMetrics->aveCharWidth;
        }

        WORD glyph;
        DWORD ret = GetGlyphIndicesW(dc.GetDC(), L" ", 1, &glyph,
                                     GGI_MARK_NONEXISTING_GLYPHS);
        if (ret != GDI_ERROR && glyph != 0xFFFF) {
            mSpaceGlyph = glyph;
        }

        SanitizeMetrics(mMetrics, GetFontEntry()->mIsBadUnderlineFont);
    }

    if (IsSyntheticBold()) {
        mMetrics->aveCharWidth += GetSyntheticBoldOffset();
        mMetrics->maxAdvance += GetSyntheticBoldOffset();
    }

    mFontFace = cairo_win32_font_face_create_for_logfontw_hfont(&logFont,
                                                                mFont);

    cairo_matrix_t sizeMatrix, ctm;
    cairo_matrix_init_identity(&ctm);
    cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);

    if (useCairoFakeItalic) {
        // Skew the matrix to do fake italic if it wasn't already applied
        // via the LOGFONT
        double skewfactor = OBLIQUE_SKEW_FACTOR;
        cairo_matrix_t style;
        cairo_matrix_init(&style,
                          1,                //xx
                          0,                //yx
                          -1 * skewfactor,  //xy
                          1,                //yy
                          0,                //x0
                          0);               //y0
        cairo_matrix_multiply(&sizeMatrix, &sizeMatrix, &style);
    }

    cairo_font_options_t *fontOptions = cairo_font_options_create();
    if (mAntialiasOption != kAntialiasDefault) {
        cairo_font_options_set_antialias(fontOptions,
            GetCairoAntialiasOption(mAntialiasOption));
    }
    mScaledFont = cairo_scaled_font_create(mFontFace, &sizeMatrix,
                                           &ctm, fontOptions);
    cairo_font_options_destroy(fontOptions);

    if (!mScaledFont ||
        cairo_scaled_font_status(mScaledFont) != CAIRO_STATUS_SUCCESS) {
#ifdef DEBUG
        char warnBuf[1024];
        sprintf(warnBuf, "Failed to create scaled font: %s status: %d",
                NS_ConvertUTF16toUTF8(mFontEntry->Name()).get(),
                mScaledFont ? cairo_scaled_font_status(mScaledFont) : 0);
        NS_WARNING(warnBuf);
#endif
        mIsValid = false;
    } else {
        mIsValid = true;
    }

#if 0
    printf("Font: %p (%s) size: %f adjusted size: %f valid: %s\n", this,
           NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size, mAdjustedSize, (mIsValid ? "yes" : "no"));
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics->emHeight, mMetrics->emAscent, mMetrics->emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics->internalLeading, mMetrics->externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f xHeight: %f\n", mMetrics->spaceWidth, mMetrics->aveCharWidth, mMetrics->xHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n",
           mMetrics->underlineOffset, mMetrics->underlineSize, mMetrics->strikeoutOffset, mMetrics->strikeoutSize,
           mMetrics->superscriptOffset, mMetrics->subscriptOffset);
#endif
}

void
gfxGDIFont::FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize,
                        bool aUseGDIFakeItalic)
{
    GDIFontEntry *fe = static_cast<GDIFontEntry*>(GetFontEntry());

    uint16_t weight;
    if (fe->IsUserFont()) {
        if (fe->IsLocalUserFont()) {
            // for local user fonts, don't change the original weight
            // in the entry's logfont, because that could alter the
            // choice of actual face used (bug 724231)
            weight = 0;
        } else {
            // avoid GDI synthetic bold which occurs when weight
            // specified is >= font data weight + 200
            weight = mNeedsBold ? 700 : 200;
        }
    } else {
        weight = mNeedsBold ? 700 : fe->Weight();
    }

    fe->FillLogFont(&aLogFont, weight, aSize, 
                    (mAntialiasOption == kAntialiasSubpixel) ? true : false);

    // If GDI synthetic italic is wanted, force the lfItalic field to true
    if (aUseGDIFakeItalic) {
        aLogFont.lfItalic = 1;
    }
}

int32_t
gfxGDIFont::GetGlyphWidth(gfxContext *aCtx, uint16_t aGID)
{
    if (!mGlyphWidths.IsInitialized()) {
        mGlyphWidths.Init(200);
    }

    int32_t width;
    if (mGlyphWidths.Get(aGID, &width)) {
        return width;
    }

    DCFromContext dc(aCtx);
    AutoSelectFont fs(dc, GetHFONT());

    int devWidth;
    if (GetCharWidthI(dc, aGID, 1, NULL, &devWidth)) {
        // ensure width is positive, 16.16 fixed-point value
        width = (devWidth & 0x7fff) << 16;
        mGlyphWidths.Put(aGID, width);
        return width;
    }

    return -1;
}

void
gfxGDIFont::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    gfxFont::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances += aMallocSizeOf(mMetrics) +
        mGlyphWidths.SizeOfExcludingThis(nullptr, aMallocSizeOf);
}

void
gfxGDIFont::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                FontCacheSizes*   aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
