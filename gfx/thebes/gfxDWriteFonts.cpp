/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteFonts.h"

#include <algorithm>
#include "gfxDWriteFontList.h"
#include "gfxContext.h"
#include "gfxTextRun.h"

#include "harfbuzz/hb.h"
#include "mozilla/FontPropertyTypes.h"

using namespace mozilla;
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

bool gfxDWriteFont::sUseClearType = true;

// This function is expensive so we only want to call it when we have to.
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
gfxDWriteFont::gfxDWriteFont(const RefPtr<UnscaledFontDWrite>& aUnscaledFont,
                             gfxFontEntry *aFontEntry,
                             const gfxFontStyle *aFontStyle,
                             bool aNeedsBold,
                             AntialiasOption anAAOption)
    : gfxFont(aUnscaledFont, aFontEntry, aFontStyle, anAAOption)
    , mCairoFontFace(nullptr)
    , mMetrics(nullptr)
    , mSpaceGlyph(0)
    , mNeedsBold(aNeedsBold)
    , mUseSubpixelPositions(false)
    , mAllowManualShowGlyphs(true)
    , mAzureScaledFontUsedClearType(false)
{
    mFontFace = aUnscaledFont->GetFontFace();

    // If the IDWriteFontFace1 interface is available, we can use that for
    // faster glyph width retrieval.
    mFontFace->QueryInterface(__uuidof(IDWriteFontFace1),
                              (void**)getter_AddRefs(mFontFace1));

    ComputeMetrics(anAAOption);
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

void
gfxDWriteFont::UpdateClearTypeUsage()
{
    Factory::UpdateSystemTextQuality();
    // Check if ClearType status has changed; if so, toggle our flag,
    // flush cached stuff that depended on the old setting, and force
    // reflow everywhere to ensure we are using correct glyph metrics.
    if (sUseClearType != UsingClearType()) {
        sUseClearType = !sUseClearType;
        gfxPlatform::FlushFontAndWordCaches();
        gfxPlatform::ForceGlobalReflow();
    }
}

UniquePtr<gfxFont>
gfxDWriteFont::CopyWithAntialiasOption(AntialiasOption anAAOption)
{
    auto entry = static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
    RefPtr<UnscaledFontDWrite> unscaledFont = static_cast<UnscaledFontDWrite*>(mUnscaledFont.get());
    return MakeUnique<gfxDWriteFont>(unscaledFont, entry, &mStyle, mNeedsBold, anAAOption);
}

const gfxFont::Metrics&
gfxDWriteFont::GetHorizontalMetrics()
{
    return *mMetrics;
}

bool
gfxDWriteFont::GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS *aFontMetrics)
{
    gfxFontStyle style(mStyle);
    style.weight = FontWeight(700);
    bool needsBold;

    gfxFontEntry* fe =
        gfxPlatformFontList::PlatformFontList()->
            FindFontForFamily(NS_LITERAL_STRING("Arial"), &style, needsBold);
    if (!fe || fe == mFontEntry) {
        return false;
    }

    RefPtr<gfxFont> font = fe->FindOrMakeFont(&style, needsBold);
    gfxDWriteFont *dwFont = static_cast<gfxDWriteFont*>(font.get());
    dwFont->mFontFace->GetMetrics(aFontMetrics);

    return true;
}

void
gfxDWriteFont::ComputeMetrics(AntialiasOption anAAOption)
{
    DWRITE_FONT_METRICS fontMetrics;
    if (!(mFontEntry->Weight() == FontWeight(900) &&
          !mFontEntry->IsUserFont() &&
          mFontEntry->Name().EqualsLiteral("Arial Black") &&
          GetFakeMetricsForArialBlack(&fontMetrics)))
    {
        mFontFace->GetMetrics(&fontMetrics);
    }

    if (mStyle.sizeAdjust >= 0.0) {
        gfxFloat aspect = (gfxFloat)fontMetrics.xHeight /
                   fontMetrics.designUnitsPerEm;
        mAdjustedSize = mStyle.GetAdjustedSize(aspect);
    } else {
        mAdjustedSize = mStyle.size;
    }

    // Note that GetMeasuringMode depends on mAdjustedSize
    if ((anAAOption == gfxFont::kAntialiasDefault &&
         sUseClearType &&
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
    mMetrics->capHeight = fontMetrics.capHeight * mFUnitsConvFactor;

    mMetrics->maxAscent = ceil(fontMetrics.ascent * mFUnitsConvFactor);
    mMetrics->maxDescent = ceil(fontMetrics.descent * mFUnitsConvFactor);
    mMetrics->maxHeight = mMetrics->maxAscent + mMetrics->maxDescent;

    mMetrics->emHeight = mAdjustedSize;
    mMetrics->emAscent = mMetrics->emHeight *
        mMetrics->maxAscent / mMetrics->maxHeight;
    mMetrics->emDescent = mMetrics->emHeight - mMetrics->emAscent;

    mMetrics->maxAdvance = mAdjustedSize;

    // try to get the true maxAdvance value from 'hhea'
    gfxFontEntry::AutoTable hheaTable(GetFontEntry(),
                                      TRUETYPE_TAG('h','h','e','a'));
    if (hheaTable) {
        uint32_t len;
        const MetricsHeader* hhea =
            reinterpret_cast<const MetricsHeader*>
                (hb_blob_get_data(hheaTable, &len));
        if (len >= sizeof(MetricsHeader)) {
            mMetrics->maxAdvance =
                uint16_t(hhea->advanceWidthMax) * mFUnitsConvFactor;
        }
    }

    mMetrics->internalLeading = std::max(mMetrics->maxHeight - mMetrics->emHeight, 0.0);
    mMetrics->externalLeading = ceil(fontMetrics.lineGap * mFUnitsConvFactor);

    UINT32 ucs = L' ';
    UINT16 glyph;
    HRESULT hr = mFontFace->GetGlyphIndicesW(&ucs, 1, &glyph);
    if (FAILED(hr)) {
        mMetrics->spaceWidth = 0;
    } else {
        mSpaceGlyph = glyph;
        mMetrics->spaceWidth = MeasureGlyphWidth(glyph);
    }

    // try to get aveCharWidth from the OS/2 table, fall back to measuring 'x'
    // if the table is not available or if using hinted/pixel-snapped widths
    if (mUseSubpixelPositions) {
        mMetrics->aveCharWidth = 0;
        gfxFontEntry::AutoTable os2Table(GetFontEntry(),
                                         TRUETYPE_TAG('O','S','/','2'));
        if (os2Table) {
            uint32_t len;
            const OS2Table* os2 =
                reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
            if (len >= 4) {
                // Not checking against sizeof(mozilla::OS2Table) here because older
                // versions of the table have different sizes; we only need the first
                // two 16-bit fields here.
                mMetrics->aveCharWidth =
                    int16_t(os2->xAvgCharWidth) * mFUnitsConvFactor;
            }
        }
    }

    if (mMetrics->aveCharWidth < 1) {
        ucs = L'x';
        if (SUCCEEDED(mFontFace->GetGlyphIndicesW(&ucs, 1, &glyph))) {
            mMetrics->aveCharWidth = MeasureGlyphWidth(glyph);
        }
        if (mMetrics->aveCharWidth < 1) {
            // Let's just assume the X is square.
            mMetrics->aveCharWidth = fontMetrics.xHeight * mFUnitsConvFactor;
        }
    }

    ucs = L'0';
    if (SUCCEEDED(mFontFace->GetGlyphIndicesW(&ucs, 1, &glyph))) {
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

    SanitizeMetrics(mMetrics, GetFontEntry()->mIsBadUnderlineFont);

#if 0
    printf("Font: %p (%s) size: %f\n", this,
           NS_ConvertUTF16toUTF8(GetName()).get(), mStyle.size);
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics->emHeight, mMetrics->emAscent, mMetrics->emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics->internalLeading, mMetrics->externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f zeroOrAve: %f\n",
           mMetrics->spaceWidth, mMetrics->aveCharWidth, mMetrics->zeroOrAveCharWidth);
    printf("    xHeight: %f capHeight: %f\n", mMetrics->xHeight, mMetrics->capHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f\n",
           mMetrics->underlineOffset, mMetrics->underlineSize, mMetrics->strikeoutOffset, mMetrics->strikeoutSize);
#endif
}

using namespace mozilla; // for AutoSwap_* types

struct EBLCHeader {
    AutoSwap_PRUint32 version;
    AutoSwap_PRUint32 numSizes;
};

struct SbitLineMetrics {
    int8_t  ascender;
    int8_t  descender;
    uint8_t widthMax;
    int8_t  caretSlopeNumerator;
    int8_t  caretSlopeDenominator;
    int8_t  caretOffset;
    int8_t  minOriginSB;
    int8_t  minAdvanceSB;
    int8_t  maxBeforeBL;
    int8_t  minAfterBL;
    int8_t  pad1;
    int8_t  pad2;
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
    uint8_t           ppemX;
    uint8_t           ppemY;
    uint8_t           bitDepth;
    uint8_t           flags;
};

typedef EBLCHeader EBSCHeader;

struct BitmapScaleTable {
    SbitLineMetrics   hori;
    SbitLineMetrics   vert;
    uint8_t           ppemX;
    uint8_t           ppemY;
    uint8_t           substitutePpemX;
    uint8_t           substitutePpemY;
};

bool
gfxDWriteFont::HasBitmapStrikeForSize(uint32_t aSize)
{
    uint8_t *tableData;
    uint32_t len;
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
        uint32_t numSizes = hdr->numSizes;
        if (numSizes > 0xffff) { // sanity-check, prevent overflow below
            break;
        }
        if (len < sizeof(EBLCHeader) + numSizes * sizeof(BitmapSizeTable)) {
            break;
        }
        const BitmapSizeTable *sizeTable =
            reinterpret_cast<const BitmapSizeTable*>(hdr + 1);
        for (uint32_t i = 0; i < numSizes; ++i, ++sizeTable) {
            if (sizeTable->ppemX == aSize && sizeTable->ppemY == aSize) {
                // we ignore a strike that contains fewer than 4 glyphs,
                // as that probably indicates a font such as Courier New
                // that provides bitmaps ONLY for the "shading" characters
                // U+2591..2593
                hasStrike = (uint16_t(sizeTable->endGlyphIndex) >=
                             uint16_t(sizeTable->startGlyphIndex) + 3);
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
        uint32_t numSizes = hdr->numSizes;
        if (numSizes > 0xffff) {
            break;
        }
        if (len < sizeof(EBSCHeader) + numSizes * sizeof(BitmapScaleTable)) {
            break;
        }
        const BitmapScaleTable *scaleTable =
            reinterpret_cast<const BitmapScaleTable*>(hdr + 1);
        for (uint32_t i = 0; i < numSizes; ++i, ++scaleTable) {
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

uint32_t
gfxDWriteFont::GetSpaceGlyph()
{
    return mSpaceGlyph;
}

bool
gfxDWriteFont::SetupCairoFont(DrawTarget* aDrawTarget)
{
    cairo_scaled_font_t *scaledFont = InitCairoScaledFont();
    if (cairo_scaled_font_status(scaledFont) != CAIRO_STATUS_SUCCESS) {
        // Don't cairo_set_scaled_font as that would propagate the error to
        // the cairo_t, precluding any further drawing.
        return false;
    }
    cairo_set_scaled_font(gfxFont::RefCairo(aDrawTarget), scaledFont);
    return true;
}

bool
gfxDWriteFont::IsValid() const
{
    return mFontFace != nullptr;
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
gfxDWriteFont::InitCairoScaledFont()
{
    if (!mScaledFont) {
        cairo_matrix_t sizeMatrix;
        cairo_matrix_t identityMatrix;

        cairo_matrix_init_scale(&sizeMatrix, mAdjustedSize, mAdjustedSize);
        cairo_matrix_init_identity(&identityMatrix);

        cairo_font_options_t *fontOptions = cairo_font_options_create();

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
gfxDWriteFont::Measure(const gfxTextRun* aTextRun,
                       uint32_t aStart, uint32_t aEnd,
                       BoundingBoxType aBoundingBoxType,
                       DrawTarget* aRefDrawTarget,
                       Spacing* aSpacing,
                       gfx::ShapedTextFlags aOrientation)
{
    gfxFont::RunMetrics metrics =
        gfxFont::Measure(aTextRun, aStart, aEnd, aBoundingBoxType,
                         aRefDrawTarget, aSpacing, aOrientation);

    // if aBoundingBoxType is LOOSE_INK_EXTENTS
    // and the underlying cairo font may be antialiased,
    // we can't trust Windows to have considered all the pixels
    // so we need to add "padding" to the bounds.
    // (see bugs 475968, 439831, compare also bug 445087)
    if (aBoundingBoxType == LOOSE_INK_EXTENTS &&
        mAntialiasOption != kAntialiasNone &&
        GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_CLASSIC &&
        metrics.mBoundingBox.Width() > 0) {
        metrics.mBoundingBox.MoveByX(-aTextRun->GetAppUnitsPerDevUnit());
        metrics.mBoundingBox.SetWidth(metrics.mBoundingBox.Width() + aTextRun->GetAppUnitsPerDevUnit() * 3);
    }

    return metrics;
}

bool
gfxDWriteFont::ProvidesGlyphWidths() const
{
    return !mUseSubpixelPositions ||
           (mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD) ||
           (((gfxDWriteFontEntry*)(GetFontEntry()))->HasVariations() &&
            !mStyle.variationSettings.IsEmpty());
}

int32_t
gfxDWriteFont::GetGlyphWidth(DrawTarget& aDrawTarget, uint16_t aGID)
{
    if (!mGlyphWidths) {
        mGlyphWidths = MakeUnique<nsDataHashtable<nsUint32HashKey,int32_t>>(128);
    }

    int32_t width = -1;
    if (mGlyphWidths->Get(aGID, &width)) {
        return width;
    }

    width = NS_lround(MeasureGlyphWidth(aGID) * 65536.0);
    mGlyphWidths->Put(aGID, width);
    return width;
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
gfxDWriteFont::MeasureGlyphWidth(uint16_t aGlyph)
{
    HRESULT hr;
    if (mFontFace1) {
        int32_t advance;
        if (mUseSubpixelPositions) {
            hr = mFontFace1->GetDesignGlyphAdvances(1, &aGlyph, &advance, FALSE);
            if (SUCCEEDED(hr)) {
                return advance * mFUnitsConvFactor;
            }
        } else {
            hr = mFontFace1->GetGdiCompatibleGlyphAdvances(
                      FLOAT(mAdjustedSize), 1.0f, nullptr,
                      GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_NATURAL,
                      FALSE, 1, &aGlyph, &advance);
            if (SUCCEEDED(hr)) {
                return NS_lround(advance * mFUnitsConvFactor);
            }
        }
    } else {
        DWRITE_GLYPH_METRICS metrics;
        if (mUseSubpixelPositions) {
            hr = mFontFace->GetDesignGlyphMetrics(&aGlyph, 1, &metrics, FALSE);
            if (SUCCEEDED(hr)) {
                return metrics.advanceWidth * mFUnitsConvFactor;
            }
        } else {
            hr = mFontFace->GetGdiCompatibleGlyphMetrics(
                      FLOAT(mAdjustedSize), 1.0f, nullptr,
                      GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_NATURAL,
                      &aGlyph, 1, &metrics, FALSE);
            if (SUCCEEDED(hr)) {
                return NS_lround(metrics.advanceWidth * mFUnitsConvFactor);
            }
        }
    }
    return 0;
}

void
gfxDWriteFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                      FontCacheSizes* aSizes) const
{
    gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
    aSizes->mFontInstances += aMallocSizeOf(mMetrics);
    if (mGlyphWidths) {
        aSizes->mFontInstances +=
            mGlyphWidths->ShallowSizeOfIncludingThis(aMallocSizeOf);
    }
}

void
gfxDWriteFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                      FontCacheSizes* aSizes) const
{
    aSizes->mFontInstances += aMallocSizeOf(this);
    AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

already_AddRefed<ScaledFont>
gfxDWriteFont::GetScaledFont(mozilla::gfx::DrawTarget *aTarget)
{
    if (mAzureScaledFontUsedClearType != sUseClearType) {
        mAzureScaledFont = nullptr;
    }
    if (!mAzureScaledFont) {
        gfxDWriteFontEntry *fe =
            static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
        bool useEmbeddedBitmap =
            fe->IsCJKFont() &&
            HasBitmapStrikeForSize(NS_lround(mAdjustedSize));
        bool forceGDI = GetForceGDIClassic();

        IDWriteRenderingParams* params = gfxWindowsPlatform::GetPlatform()->GetRenderingParams(
            sUseClearType ?
                (forceGDI ?
                    gfxWindowsPlatform::TEXT_RENDERING_GDI_CLASSIC :
                    gfxWindowsPlatform::TEXT_RENDERING_NORMAL) :
                gfxWindowsPlatform::TEXT_RENDERING_NO_CLEARTYPE);

        const gfxFontStyle* fontStyle = GetStyle();
        mAzureScaledFont =
            Factory::CreateScaledFontForDWriteFont(mFontFace, fontStyle,
                                                   GetUnscaledFont(),
                                                   GetAdjustedSize(),
                                                   useEmbeddedBitmap,
                                                   forceGDI,
                                                   params,
                                                   params->GetGamma(),
                                                   params->GetEnhancedContrast());
        if (!mAzureScaledFont) {
            return nullptr;
        }
        mAzureScaledFontUsedClearType = sUseClearType;
    }

    if (aTarget->GetBackendType() == BackendType::CAIRO) {
        if (!mAzureScaledFont->GetCairoScaledFont()) {
            cairo_scaled_font_t* cairoScaledFont = InitCairoScaledFont();
            if (!cairoScaledFont) {
                return nullptr;
            }
            mAzureScaledFont->SetCairoScaledFont(cairoScaledFont);
        }
    }

    RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
    return scaledFont.forget();
}
