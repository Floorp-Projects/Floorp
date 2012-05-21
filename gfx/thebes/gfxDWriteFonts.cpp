/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteFonts.h"
#include "gfxDWriteShaper.h"
#include "gfxHarfBuzzShaper.h"
#ifdef MOZ_GRAPHITE
#include "gfxGraphiteShaper.h"
#endif
#include "gfxDWriteFontList.h"
#include "gfxContext.h"
#include <dwrite.h>

#include "gfxDWriteTextAnalysis.h"

#include "harfbuzz/hb.h"

// Chosen this as to resemble DWrite's own oblique face style.
#define OBLIQUE_SKEW_FACTOR 0.3

using namespace mozilla::gfx;

// This is also in gfxGDIFont.cpp. Would be nice to put it somewhere common,
// but we can't declare it in the gfxFont.h or gfxFontUtils.h headers
// because those are exported, and the cairo headers aren't.
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

// Code to determine whether Windows is set to use ClearType font smoothing;
// based on private functions in cairo-win32-font.c

#ifndef SPI_GETFONTSMOOTHINGTYPE
#define SPI_GETFONTSMOOTHINGTYPE 0x200a
#endif
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#define FE_FONTSMOOTHINGCLEARTYPE 2
#endif

static bool
UsingClearType()
{
    BOOL fontSmoothing;
    if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &fontSmoothing, 0) ||
        !fontSmoothing)
    {
        return false;
    }

    UINT type;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &type, 0) &&
        type == FE_FONTSMOOTHINGCLEARTYPE)
    {
        return true;
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFont
gfxDWriteFont::gfxDWriteFont(gfxFontEntry *aFontEntry,
                             const gfxFontStyle *aFontStyle,
                             bool aNeedsBold,
                             AntialiasOption anAAOption)
    : gfxFont(aFontEntry, aFontStyle, anAAOption)
    , mCairoFontFace(nsnull)
    , mMetrics(nsnull)
    , mNeedsOblique(false)
    , mNeedsBold(aNeedsBold)
    , mUseSubpixelPositions(false)
    , mAllowManualShowGlyphs(true)
{
    gfxDWriteFontEntry *fe =
        static_cast<gfxDWriteFontEntry*>(aFontEntry);
    nsresult rv;
    DWRITE_FONT_SIMULATIONS sims = DWRITE_FONT_SIMULATIONS_NONE;
    if ((GetStyle()->style & (NS_FONT_STYLE_ITALIC | NS_FONT_STYLE_OBLIQUE)) &&
        !fe->IsItalic()) {
            // For this we always use the font_matrix for uniformity. Not the
            // DWrite simulation.
            mNeedsOblique = true;
    }
    if (aNeedsBold) {
        sims |= DWRITE_FONT_SIMULATIONS_BOLD;
    }

    rv = fe->CreateFontFace(getter_AddRefs(mFontFace), sims);

    if (NS_FAILED(rv)) {
        mIsValid = false;
        return;
    }

    ComputeMetrics(anAAOption);

#ifdef MOZ_GRAPHITE
    if (FontCanSupportGraphite()) {
        mGraphiteShaper = new gfxGraphiteShaper(this);
    }
#endif

    if (FontCanSupportHarfBuzz()) {
        mHarfBuzzShaper = new gfxHarfBuzzShaper(this);
    }
}

gfxDWriteFont::~gfxDWriteFont()
{
    if (mCairoFontFace) {
        cairo_font_face_destroy(mCairoFontFace);
    }
    if (mScaledFont) {
        cairo_scaled_font_destroy(mScaledFont);
    }
    delete mMetrics;
}

gfxFont*
gfxDWriteFont::CopyWithAntialiasOption(AntialiasOption anAAOption)
{
    return new gfxDWriteFont(static_cast<gfxDWriteFontEntry*>(mFontEntry.get()),
                             &mStyle, mNeedsBold, anAAOption);
}

void
gfxDWriteFont::CreatePlatformShaper()
{
    mPlatformShaper = new gfxDWriteShaper(this);
}

const gfxFont::Metrics&
gfxDWriteFont::GetMetrics()
{
    return *mMetrics;
}

bool
gfxDWriteFont::GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS *aFontMetrics)
{
    gfxFontStyle style(mStyle);
    style.weight = 700;
    bool needsBold;
    gfxFontEntry *fe = mFontEntry->Family()->FindFontForStyle(style, needsBold);
    if (!fe || fe == mFontEntry) {
        return false;
    }

    nsRefPtr<gfxFont> font = fe->FindOrMakeFont(&style, needsBold);
    gfxDWriteFont *dwFont = static_cast<gfxDWriteFont*>(font.get());
    dwFont->mFontFace->GetMetrics(aFontMetrics);

    return true;
}

void
gfxDWriteFont::ComputeMetrics(AntialiasOption anAAOption)
{
    DWRITE_FONT_METRICS fontMetrics;
    if (!(mFontEntry->Weight() == 900 &&
          !mFontEntry->IsUserFont() &&
          mFontEntry->FamilyName().EqualsLiteral("Arial") &&
          GetFakeMetricsForArialBlack(&fontMetrics)))
    {
        mFontFace->GetMetrics(&fontMetrics);
    }

    if (mStyle.sizeAdjust != 0.0) {
        gfxFloat aspect = (gfxFloat)fontMetrics.xHeight /
                   fontMetrics.designUnitsPerEm;
        mAdjustedSize = mStyle.GetAdjustedSize(aspect);
    } else {
        mAdjustedSize = mStyle.size;
    }

    // Note that GetMeasuringMode depends on mAdjustedSize
    if ((anAAOption == gfxFont::kAntialiasDefault &&
         UsingClearType() &&
         GetMeasuringMode() == DWRITE_MEASURING_MODE_NATURAL) ||
        anAAOption == gfxFont::kAntialiasSubpixel)
    {
        mUseSubpixelPositions = true;
        // note that this may be reset to FALSE if we determine that a bitmap
        // strike is going to be used
    }

    gfxDWriteFontEntry *fe =
        static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
    if (fe->IsCJKFont() && HasBitmapStrikeForSize(NS_lround(mAdjustedSize))) {
        mAdjustedSize = NS_lround(mAdjustedSize);
        mUseSubpixelPositions = false;
        // if we have bitmaps, we need to tell Cairo NOT to use subpixel AA,
        // to avoid the manual-subpixel codepath in cairo-d2d-surface.cpp
        // which fails to render bitmap glyphs (see bug 626299).
        // This option will be passed to the cairo_dwrite_scaled_font_t
        // after creation.
        mAllowManualShowGlyphs = false;
    }

    mMetrics = new gfxFont::Metrics;
    ::memset(mMetrics, 0, sizeof(*mMetrics));

    mFUnitsConvFactor = float(mAdjustedSize / fontMetrics.designUnitsPerEm);

    mMetrics->xHeight = fontMetrics.xHeight * mFUnitsConvFactor;

    mMetrics->maxAscent = ceil(fontMetrics.ascent * mFUnitsConvFactor);
    mMetrics->maxDescent = ceil(fontMetrics.descent * mFUnitsConvFactor);
    mMetrics->maxHeight = mMetrics->maxAscent + mMetrics->maxDescent;

    mMetrics->emHeight = mAdjustedSize;
    mMetrics->emAscent = mMetrics->emHeight *
        mMetrics->maxAscent / mMetrics->maxHeight;
    mMetrics->emDescent = mMetrics->emHeight - mMetrics->emAscent;

    mMetrics->maxAdvance = mAdjustedSize;

    // try to get the true maxAdvance value from 'hhea'
    PRUint8 *tableData;
    PRUint32 len;
    void *tableContext = NULL;
    BOOL exists;
    HRESULT hr =
        mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('h', 'h', 'e', 'a'),
                                   (const void**)&tableData,
                                   &len,
                                   &tableContext,
                                   &exists);
    if (SUCCEEDED(hr)) {
        if (exists && len >= sizeof(mozilla::HheaTable)) {
            const mozilla::HheaTable* hhea =
                reinterpret_cast<const mozilla::HheaTable*>(tableData);
            mMetrics->maxAdvance =
                PRUint16(hhea->advanceWidthMax) * mFUnitsConvFactor;
        }
        mFontFace->ReleaseFontTable(tableContext);
    }

    mMetrics->internalLeading = NS_MAX(mMetrics->maxHeight - mMetrics->emHeight, 0.0);
    mMetrics->externalLeading = ceil(fontMetrics.lineGap * mFUnitsConvFactor);

    UINT16 glyph = (PRUint16)GetSpaceGlyph();
    mMetrics->spaceWidth = MeasureGlyphWidth(glyph);

    // try to get aveCharWidth from the OS/2 table, fall back to measuring 'x'
    // if the table is not available or if using hinted/pixel-snapped widths
    if (mUseSubpixelPositions) {
        mMetrics->aveCharWidth = 0;
        hr = mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('O', 'S', '/', '2'),
                                        (const void**)&tableData,
                                        &len,
                                        &tableContext,
                                        &exists);
        if (SUCCEEDED(hr)) {
            if (exists && len >= 4) {
                // Not checking against sizeof(mozilla::OS2Table) here because older
                // versions of the table have different sizes; we only need the first
                // two 16-bit fields here.
                const mozilla::OS2Table* os2 =
                    reinterpret_cast<const mozilla::OS2Table*>(tableData);
                mMetrics->aveCharWidth =
                    PRInt16(os2->xAvgCharWidth) * mFUnitsConvFactor;
            }
            mFontFace->ReleaseFontTable(tableContext);
        }
    }

    UINT32 ucs;
    if (mMetrics->aveCharWidth < 1) {
        ucs = L'x';
        if (SUCCEEDED(mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph))) {
            mMetrics->aveCharWidth = MeasureGlyphWidth(glyph);
        }
        if (mMetrics->aveCharWidth < 1) {
            // Let's just assume the X is square.
            mMetrics->aveCharWidth = fontMetrics.xHeight * mFUnitsConvFactor;
        }
    }

    ucs = L'0';
    if (SUCCEEDED(mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph))) {
        mMetrics->zeroOrAveCharWidth = MeasureGlyphWidth(glyph);
    }
    if (mMetrics->zeroOrAveCharWidth < 1) {
        mMetrics->zeroOrAveCharWidth = mMetrics->aveCharWidth;
    }

    mMetrics->underlineOffset =
        fontMetrics.underlinePosition * mFUnitsConvFactor;
    mMetrics->underlineSize = 
        fontMetrics.underlineThickness * mFUnitsConvFactor;
    mMetrics->strikeoutOffset =
        fontMetrics.strikethroughPosition * mFUnitsConvFactor;
    mMetrics->strikeoutSize =
        fontMetrics.strikethroughThickness * mFUnitsConvFactor;
    mMetrics->superscriptOffset = 0;
    mMetrics->subscriptOffset = 0;

    SanitizeMetrics(mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    printf("Font: %p (%s) size: %f\n", this,
           NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics->emHeight, mMetrics->emAscent, mMetrics->emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics->internalLeading, mMetrics->externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f zeroOrAve: %f xHeight: %f\n",
           mMetrics->spaceWidth, mMetrics->aveCharWidth, mMetrics->zeroOrAveCharWidth, mMetrics->xHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f supOff: %f subOff: %f\n",
           mMetrics->underlineOffset, mMetrics->underlineSize, mMetrics->strikeoutOffset, mMetrics->strikeoutSize,
           mMetrics->superscriptOffset, mMetrics->subscriptOffset);
#endif
}

using namespace mozilla; // for AutoSwap_* types

struct EBLCHeader {
    AutoSwap_PRUint32 version;
    AutoSwap_PRUint32 numSizes;
};

struct SbitLineMetrics {
    PRInt8  ascender;
    PRInt8  descender;
    PRUint8 widthMax;
    PRInt8  caretSlopeNumerator;
    PRInt8  caretSlopeDenominator;
    PRInt8  caretOffset;
    PRInt8  minOriginSB;
    PRInt8  minAdvanceSB;
    PRInt8  maxBeforeBL;
    PRInt8  minAfterBL;
    PRInt8  pad1;
    PRInt8  pad2;
};

struct BitmapSizeTable {
    AutoSwap_PRUint32 indexSubTableArrayOffset;
    AutoSwap_PRUint32 indexTablesSize;
    AutoSwap_PRUint32 numberOfIndexSubTables;
    AutoSwap_PRUint32 colorRef;
    SbitLineMetrics   hori;
    SbitLineMetrics   vert;
    AutoSwap_PRUint16 startGlyphIndex;
    AutoSwap_PRUint16 endGlyphIndex;
    PRUint8           ppemX;
    PRUint8           ppemY;
    PRUint8           bitDepth;
    PRUint8           flags;
};

typedef EBLCHeader EBSCHeader;

struct BitmapScaleTable {
    SbitLineMetrics   hori;
    SbitLineMetrics   vert;
    PRUint8           ppemX;
    PRUint8           ppemY;
    PRUint8           substitutePpemX;
    PRUint8           substitutePpemY;
};

bool
gfxDWriteFont::HasBitmapStrikeForSize(PRUint32 aSize)
{
    PRUint8 *tableData;
    PRUint32 len;
    void *tableContext;
    BOOL exists;
    HRESULT hr =
        mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('E', 'B', 'L', 'C'),
                                   (const void**)&tableData, &len,
                                   &tableContext, &exists);
    if (FAILED(hr)) {
        return false;
    }

    bool hasStrike = false;
    // not really a loop, but this lets us use 'break' to skip out of the block
    // as soon as we know the answer, and skips it altogether if the table is
    // not present
    while (exists) {
        if (len < sizeof(EBLCHeader)) {
            break;
        }
        const EBLCHeader *hdr = reinterpret_cast<const EBLCHeader*>(tableData);
        if (hdr->version != 0x00020000) {
            break;
        }
        PRUint32 numSizes = hdr->numSizes;
        if (numSizes > 0xffff) { // sanity-check, prevent overflow below
            break;
        }
        if (len < sizeof(EBLCHeader) + numSizes * sizeof(BitmapSizeTable)) {
            break;
        }
        const BitmapSizeTable *sizeTable =
            reinterpret_cast<const BitmapSizeTable*>(hdr + 1);
        for (PRUint32 i = 0; i < numSizes; ++i, ++sizeTable) {
            if (sizeTable->ppemX == aSize && sizeTable->ppemY == aSize) {
                // we ignore a strike that contains fewer than 4 glyphs,
                // as that probably indicates a font such as Courier New
                // that provides bitmaps ONLY for the "shading" characters
                // U+2591..2593
                hasStrike = (PRUint16(sizeTable->endGlyphIndex) >=
                             PRUint16(sizeTable->startGlyphIndex) + 3);
                break;
            }
        }
        // if we reach here, we didn't find a strike; unconditionally break
        // out of the while-loop block
        break;
    }
    mFontFace->ReleaseFontTable(tableContext);

    if (hasStrike) {
        return true;
    }

    // if we didn't find a real strike, check if the font calls for scaling
    // another bitmap to this size
    hr = mFontFace->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('E', 'B', 'S', 'C'),
                                    (const void**)&tableData, &len,
                                    &tableContext, &exists);
    if (FAILED(hr)) {
        return false;
    }

    while (exists) {
        if (len < sizeof(EBSCHeader)) {
            break;
        }
        const EBSCHeader *hdr = reinterpret_cast<const EBSCHeader*>(tableData);
        if (hdr->version != 0x00020000) {
            break;
        }
        PRUint32 numSizes = hdr->numSizes;
        if (numSizes > 0xffff) {
            break;
        }
        if (len < sizeof(EBSCHeader) + numSizes * sizeof(BitmapScaleTable)) {
            break;
        }
        const BitmapScaleTable *scaleTable =
            reinterpret_cast<const BitmapScaleTable*>(hdr + 1);
        for (PRUint32 i = 0; i < numSizes; ++i, ++scaleTable) {
            if (scaleTable->ppemX == aSize && scaleTable->ppemY == aSize) {
                hasStrike = true;
                break;
            }
        }
        break;
    }
    mFontFace->ReleaseFontTable(tableContext);

    return hasStrike;
}

PRUint32
gfxDWriteFont::GetSpaceGlyph()
{
    UINT32 ucs = L' ';
    UINT16 glyph;
    HRESULT hr;
    hr = mFontFace->GetGlyphIndicesA(&ucs, 1, &glyph);
    if (FAILED(hr)) {
        return 0;
    }
    return glyph;
}

bool
gfxDWriteFont::SetupCairoFont(gfxContext *aContext)
{
    cairo_scaled_font_t *scaledFont = CairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(aContext->GetCairo(), scaledFont);
    return true;
}

bool
gfxDWriteFont::IsValid()
{
    return mFontFace != NULL;
}

IDWriteFontFace*
gfxDWriteFont::GetFontFace()
{
    return  mFontFace.get();
}

cairo_font_face_t *
gfxDWriteFont::CairoFontFace()
{
    if (!mCairoFontFace) {
#ifdef CAIRO_HAS_DWRITE_FONT
        mCairoFontFace = 
            cairo_dwrite_font_face_create_for_dwrite_fontface(
            ((gfxDWriteFontEntry*)mFontEntry.get())->mFont, mFontFace);
#endif
    }
    return mCairoFontFace;
}


cairo_scaled_font_t *
gfxDWriteFont::CairoScaledFont()
{
    if (!mScaledFont) {
        cairo_matrix_t sizeMatrix;
        cairo_matrix_t identityMatrix;

        cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);
        cairo_matrix_init_identity(&identityMatrix);

        cairo_font_options_t *fontOptions = cairo_font_options_create();
        if (mNeedsOblique) {
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

        if (mAntialiasOption != kAntialiasDefault) {
            cairo_font_options_set_antialias(fontOptions,
                GetCairoAntialiasOption(mAntialiasOption));
        }

        mScaledFont = cairo_scaled_font_create(CairoFontFace(),
                                                    &sizeMatrix,
                                                    &identityMatrix,
                                                    fontOptions);
        cairo_font_options_destroy(fontOptions);

        cairo_dwrite_scaled_font_allow_manual_show_glyphs(mScaledFont,
                                                          mAllowManualShowGlyphs);

        gfxDWriteFontEntry *fe =
            static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
        cairo_dwrite_scaled_font_set_force_GDI_classic(mScaledFont,
                                                       GetForceGDIClassic());
    }

    NS_ASSERTION(mAdjustedSize == 0.0 ||
                 cairo_scaled_font_status(mScaledFont) 
                   == CAIRO_STATUS_SUCCESS,
                 "Failed to make scaled font");

    return mScaledFont;
}

gfxFont::RunMetrics
gfxDWriteFont::Measure(gfxTextRun *aTextRun,
                    PRUint32 aStart, PRUint32 aEnd,
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
        GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_CLASSIC &&
        metrics.mBoundingBox.width > 0) {
        metrics.mBoundingBox.x -= aTextRun->GetAppUnitsPerDevUnit();
        metrics.mBoundingBox.width += aTextRun->GetAppUnitsPerDevUnit() * 3;
    }

    return metrics;
}

// Access to font tables packaged in hb_blob_t form

// object attached to the Harfbuzz blob, used to release
// the table when the blob is destroyed
class FontTableRec {
public:
    FontTableRec(IDWriteFontFace *aFontFace, void *aContext)
        : mFontFace(aFontFace), mContext(aContext)
    { }

    ~FontTableRec() {
        mFontFace->ReleaseFontTable(mContext);
    }

private:
    IDWriteFontFace *mFontFace;
    void            *mContext;
};

/*static*/ void
gfxDWriteFont::DestroyBlobFunc(void* aUserData)
{
    FontTableRec *ftr = static_cast<FontTableRec*>(aUserData);
    delete ftr;
}

hb_blob_t *
gfxDWriteFont::GetFontTable(PRUint32 aTag)
{
    const void *data;
    UINT32      size;
    void       *context;
    BOOL        exists;
    HRESULT hr = mFontFace->TryGetFontTable(NS_SWAP32(aTag),
                                            &data, &size, &context, &exists);
    if (SUCCEEDED(hr) && exists) {
        FontTableRec *ftr = new FontTableRec(mFontFace, context);
        return hb_blob_create(static_cast<const char*>(data), size,
                              HB_MEMORY_MODE_READONLY,
                              ftr, DestroyBlobFunc);
    }

    if (mFontEntry->IsUserFont() && !mFontEntry->IsLocalUserFont()) {
        // for downloaded fonts, there may be layout tables cached in the entry
        // even though they're absent from the sanitized platform font
        hb_blob_t *blob;
        if (mFontEntry->GetExistingFontTable(aTag, &blob)) {
            return blob;
        }
    }

    return nsnull;
}

bool
gfxDWriteFont::ProvidesGlyphWidths()
{
    return !mUseSubpixelPositions ||
           (mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD);
}

PRInt32
gfxDWriteFont::GetGlyphWidth(gfxContext *aCtx, PRUint16 aGID)
{
    if (!mGlyphWidths.IsInitialized()) {
        mGlyphWidths.Init(200);
    }

    PRInt32 width = -1;
    if (mGlyphWidths.Get(aGID, &width)) {
        return width;
    }

    width = NS_lround(MeasureGlyphWidth(aGID) * 65536.0);
    mGlyphWidths.Put(aGID, width);
    return width;
}

TemporaryRef<GlyphRenderingOptions>
gfxDWriteFont::GetGlyphRenderingOptions()
{
  if (UsingClearType()) {
    return Factory::CreateDWriteGlyphRenderingOptions(
      gfxWindowsPlatform::GetPlatform()->GetRenderingParams(GetForceGDIClassic() ?
        gfxWindowsPlatform::TEXT_RENDERING_GDI_CLASSIC : gfxWindowsPlatform::TEXT_RENDERING_NORMAL));
  } else {
    return Factory::CreateDWriteGlyphRenderingOptions(gfxWindowsPlatform::GetPlatform()->
      GetRenderingParams(gfxWindowsPlatform::TEXT_RENDERING_NO_CLEARTYPE));
  }
}

bool
gfxDWriteFont::GetForceGDIClassic()
{
    return static_cast<gfxDWriteFontEntry*>(mFontEntry.get())->GetForceGDIClassic() &&
         cairo_dwrite_get_cleartype_rendering_mode() < 0 &&
         GetAdjustedSize() <=
            gfxDWriteFontList::PlatformFontList()->GetForceGDIClassicMaxFontSize();
}

DWRITE_MEASURING_MODE
gfxDWriteFont::GetMeasuringMode()
{
    return GetForceGDIClassic()
        ? DWRITE_MEASURING_MODE_GDI_CLASSIC
        : gfxWindowsPlatform::GetPlatform()->DWriteMeasuringMode();
}

gfxFloat
gfxDWriteFont::MeasureGlyphWidth(PRUint16 aGlyph)
{
    DWRITE_GLYPH_METRICS metrics;
    HRESULT hr;
    if (mUseSubpixelPositions) {
        hr = mFontFace->GetDesignGlyphMetrics(&aGlyph, 1, &metrics, FALSE);
        if (SUCCEEDED(hr)) {
            return metrics.advanceWidth * mFUnitsConvFactor;
        }
    } else {
        hr = mFontFace->GetGdiCompatibleGlyphMetrics(
                  FLOAT(mAdjustedSize), 1.0f, nsnull,
                  GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_NATURAL,
                  &aGlyph, 1, &metrics, FALSE);
        if (SUCCEEDED(hr)) {
            return NS_lround(metrics.advanceWidth * mFUnitsConvFactor);
        }
    }
    return 0;
}

void
gfxDWriteFont::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                   FontCacheSizes*   aSizes) const
{
    gfxFont::SizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances += aMallocSizeOf(mMetrics) +
        mGlyphWidths.SizeOfExcludingThis(nsnull, aMallocSizeOf);
}

void
gfxDWriteFont::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf,
                                   FontCacheSizes*   aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    SizeOfExcludingThis(aMallocSizeOf, aSizes);
}
