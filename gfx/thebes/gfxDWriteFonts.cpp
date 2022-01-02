/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxDWriteFonts.h"

#include <algorithm>
#include "gfxDWriteFontList.h"
#include "gfxContext.h"
#include "gfxTextRun.h"
#include "mozilla/gfx/2D.h"
#include "mozilla/gfx/DWriteSettings.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Preferences.h"

#include "harfbuzz/hb.h"
#include "mozilla/FontPropertyTypes.h"

using namespace mozilla;
using namespace mozilla::gfx;

// Code to determine whether Windows is set to use ClearType font smoothing;
// based on private functions in cairo-win32-font.c

#ifndef SPI_GETFONTSMOOTHINGTYPE
#  define SPI_GETFONTSMOOTHINGTYPE 0x200a
#endif
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#  define FE_FONTSMOOTHINGCLEARTYPE 2
#endif

// Cleartype can be dynamically enabled/disabled, so we have to allow for
// dynamically updating it.
static BYTE GetSystemTextQuality() {
  BOOL font_smoothing;
  UINT smoothing_type;

  if (!SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0)) {
    return DEFAULT_QUALITY;
  }

  if (font_smoothing) {
    if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing_type,
                              0)) {
      return DEFAULT_QUALITY;
    }

    if (smoothing_type == FE_FONTSMOOTHINGCLEARTYPE) {
      return CLEARTYPE_QUALITY;
    }

    return ANTIALIASED_QUALITY;
  }

  return DEFAULT_QUALITY;
}

#ifndef SPI_GETFONTSMOOTHINGCONTRAST
#  define SPI_GETFONTSMOOTHINGCONTRAST 0x200c
#endif

// "Retrieves a contrast value that is used in ClearType smoothing. Valid
// contrast values are from 1000 to 2200. The default value is 1400."
static FLOAT GetSystemGDIGamma() {
  UINT value = 0;
  if (!SystemParametersInfo(SPI_GETFONTSMOOTHINGCONTRAST, 0, &value, 0) ||
      value < 1000 || value > 2200) {
    value = 1400;
  }
  return value / 1000.0f;
}

////////////////////////////////////////////////////////////////////////////////
// gfxDWriteFont
gfxDWriteFont::gfxDWriteFont(const RefPtr<UnscaledFontDWrite>& aUnscaledFont,
                             gfxFontEntry* aFontEntry,
                             const gfxFontStyle* aFontStyle,
                             RefPtr<IDWriteFontFace> aFontFace,
                             AntialiasOption anAAOption)
    : gfxFont(aUnscaledFont, aFontEntry, aFontStyle, anAAOption),
      mFontFace(aFontFace ? aFontFace : aUnscaledFont->GetFontFace()),
      mMetrics(nullptr),
      mUseSubpixelPositions(false),
      mAllowManualShowGlyphs(true),
      mAzureScaledFontUsedClearType(false) {
  // If the IDWriteFontFace1 interface is available, we can use that for
  // faster glyph width retrieval.
  mFontFace->QueryInterface(__uuidof(IDWriteFontFace1),
                            (void**)getter_AddRefs(mFontFace1));

  ComputeMetrics(anAAOption);
}

gfxDWriteFont::~gfxDWriteFont() { delete mMetrics; }

/* static */
bool gfxDWriteFont::InitDWriteSupport() {
  if (!Factory::EnsureDWriteFactory()) {
    return false;
  }

  if (XRE_IsParentProcess()) {
    UpdateSystemTextVars();
  }
  DWriteSettings::Initialize();

  return true;
}

/* static */
void gfxDWriteFont::UpdateSystemTextVars() {
  MOZ_ASSERT(XRE_IsParentProcess());

  BYTE newQuality = GetSystemTextQuality();
  if (gfxVars::SystemTextQuality() != newQuality) {
    gfxVars::SetSystemTextQuality(newQuality);
  }

  FLOAT newGDIGamma = GetSystemGDIGamma();
  if (gfxVars::SystemGDIGamma() != newGDIGamma) {
    gfxVars::SetSystemGDIGamma(newGDIGamma);
  }

  UpdateClearTypeVars();
}

void gfxDWriteFont::SystemTextQualityChanged() {
  // If ClearType status has changed, update our value,
  Factory::SetSystemTextQuality(gfxVars::SystemTextQuality());
  // flush cached stuff that depended on the old setting, and force
  // reflow everywhere to ensure we are using correct glyph metrics.
  gfxPlatform::FlushFontAndWordCaches();
  gfxPlatform::ForceGlobalReflow(gfxPlatform::NeedsReframe::No);
}

mozilla::Atomic<bool> gfxDWriteFont::sForceGDIClassicEnabled{true};

/* static */
void gfxDWriteFont::UpdateClearTypeVars() {
  // We don't force GDI classic if the cleartype rendering mode pref is set to
  // something valid.
  int32_t renderingModePref =
      Preferences::GetInt(GFX_CLEARTYPE_PARAMS_MODE, -1);
  if (renderingModePref < 0 || renderingModePref > 5) {
    renderingModePref = -1;
  }
  sForceGDIClassicEnabled = (renderingModePref == -1);

  if (!XRE_IsParentProcess()) {
    return;
  }

  if (!Factory::GetDWriteFactory()) {
    return;
  }

  // First set sensible hard coded defaults.
  float clearTypeLevel = 1.0f;
  float enhancedContrast = 1.0f;
  float gamma = 2.2f;
  int pixelGeometry = DWRITE_PIXEL_GEOMETRY_RGB;
  int renderingMode = DWRITE_RENDERING_MODE_DEFAULT;

  // Override these from DWrite function if available.
  RefPtr<IDWriteRenderingParams> defaultRenderingParams;
  HRESULT hr = Factory::GetDWriteFactory()->CreateRenderingParams(
      getter_AddRefs(defaultRenderingParams));
  if (SUCCEEDED(hr) && defaultRenderingParams) {
    clearTypeLevel = defaultRenderingParams->GetClearTypeLevel();

    // For enhanced contrast, we only use the default if the user has set it
    // in the registry (by using the ClearType Tuner).
    // XXXbobowen it seems slightly odd that we do this and only for enhanced
    // contrast, but this reproduces previous functionality from
    // gfxWindowsPlatform::SetupClearTypeParams.
    HKEY hKey;
    LONG res = RegOpenKeyExW(DISPLAY1_REGISTRY_KEY, 0, KEY_READ, &hKey);
    if (res == ERROR_SUCCESS) {
      res = RegQueryValueExW(hKey, ENHANCED_CONTRAST_VALUE_NAME, nullptr,
                             nullptr, nullptr, nullptr);
      if (res == ERROR_SUCCESS) {
        enhancedContrast = defaultRenderingParams->GetEnhancedContrast();
      }
      RegCloseKey(hKey);
    }

    gamma = defaultRenderingParams->GetGamma();
    pixelGeometry = defaultRenderingParams->GetPixelGeometry();
    renderingMode = defaultRenderingParams->GetRenderingMode();
  } else {
    gfxWarning() << "Failed to create default rendering params";
  }

  // Finally override from prefs if valid values are set. If ClearType is
  // turned off we just use the default params, this reproduces the previous
  // functionality that was spread across gfxDWriteFont::GetScaledFont and
  // gfxWindowsPlatform::SetupClearTypeParams, but it seems odd because the
  // default params will still be the ClearType ones although we won't use the
  // anti-alias for ClearType because of GetSystemDefaultAAMode.
  if (gfxVars::SystemTextQuality() == CLEARTYPE_QUALITY) {
    int32_t prefInt = Preferences::GetInt(GFX_CLEARTYPE_PARAMS_LEVEL, -1);
    if (prefInt >= 0 && prefInt <= 100) {
      clearTypeLevel = float(prefInt / 100.0);
    }

    prefInt = Preferences::GetInt(GFX_CLEARTYPE_PARAMS_CONTRAST, -1);
    if (prefInt >= 0 && prefInt <= 1000) {
      enhancedContrast = float(prefInt / 100.0);
    }

    prefInt = Preferences::GetInt(GFX_CLEARTYPE_PARAMS_GAMMA, -1);
    if (prefInt >= 1000 && prefInt <= 2200) {
      gamma = float(prefInt / 1000.0);
    }

    prefInt = Preferences::GetInt(GFX_CLEARTYPE_PARAMS_STRUCTURE, -1);
    if (prefInt >= 0 && prefInt <= 2) {
      pixelGeometry = prefInt;
    }

    // renderingModePref is retrieved and validated above.
    if (renderingModePref != -1) {
      renderingMode = renderingModePref;
    }
  }

  if (gfxVars::SystemTextClearTypeLevel() != clearTypeLevel) {
    gfxVars::SetSystemTextClearTypeLevel(clearTypeLevel);
  }

  if (gfxVars::SystemTextEnhancedContrast() != enhancedContrast) {
    gfxVars::SetSystemTextEnhancedContrast(enhancedContrast);
  }

  if (gfxVars::SystemTextGamma() != gamma) {
    gfxVars::SetSystemTextGamma(gamma);
  }

  if (gfxVars::SystemTextPixelGeometry() != pixelGeometry) {
    gfxVars::SetSystemTextPixelGeometry(pixelGeometry);
  }

  if (gfxVars::SystemTextRenderingMode() != renderingMode) {
    gfxVars::SetSystemTextRenderingMode(renderingMode);
  }

  // Set cairo dwrite params in the parent process where it might still be
  // needed for printing. We use the validated pref int directly for rendering
  // mode, because a negative (i.e. not set) rendering mode is also used for
  // deciding on forcing GDI in cairo.
  cairo_dwrite_set_cleartype_params(gamma, enhancedContrast, clearTypeLevel,
                                    pixelGeometry, renderingModePref);
}

UniquePtr<gfxFont> gfxDWriteFont::CopyWithAntialiasOption(
    AntialiasOption anAAOption) {
  auto entry = static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
  RefPtr<UnscaledFontDWrite> unscaledFont =
      static_cast<UnscaledFontDWrite*>(mUnscaledFont.get());
  return MakeUnique<gfxDWriteFont>(unscaledFont, entry, &mStyle, mFontFace,
                                   anAAOption);
}

const gfxFont::Metrics& gfxDWriteFont::GetHorizontalMetrics() {
  return *mMetrics;
}

bool gfxDWriteFont::GetFakeMetricsForArialBlack(
    DWRITE_FONT_METRICS* aFontMetrics) {
  gfxFontStyle style(mStyle);
  style.weight = FontWeight(700);

  gfxFontEntry* fe = gfxPlatformFontList::PlatformFontList()->FindFontForFamily(
      nullptr, "Arial"_ns, &style);
  if (!fe || fe == mFontEntry) {
    return false;
  }

  RefPtr<gfxFont> font = fe->FindOrMakeFont(&style);
  gfxDWriteFont* dwFont = static_cast<gfxDWriteFont*>(font.get());
  dwFont->mFontFace->GetMetrics(aFontMetrics);

  return true;
}

void gfxDWriteFont::ComputeMetrics(AntialiasOption anAAOption) {
  DWRITE_FONT_METRICS fontMetrics;
  if (!(mFontEntry->Weight().Min() == FontWeight(900) &&
        mFontEntry->Weight().Max() == FontWeight(900) &&
        !mFontEntry->IsUserFont() &&
        mFontEntry->Name().EqualsLiteral("Arial Black") &&
        GetFakeMetricsForArialBlack(&fontMetrics))) {
    mFontFace->GetMetrics(&fontMetrics);
  }

  if (GetAdjustedSize() > 0.0 && mStyle.sizeAdjust >= 0.0 &&
      FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) !=
          FontSizeAdjust::Tag::None) {
    // For accurate measurement during the font-size-adjust computations;
    // these may be reset later according to the adjusted size.
    mUseSubpixelPositions = true;
    mFUnitsConvFactor = float(mAdjustedSize / fontMetrics.designUnitsPerEm);
    gfxFloat aspect;
    switch (FontSizeAdjust::Tag(mStyle.sizeAdjustBasis)) {
      default:
        MOZ_ASSERT_UNREACHABLE("unhandled sizeAdjustBasis?");
        aspect = 0.0;
        break;
      case FontSizeAdjust::Tag::ExHeight:
        aspect = (gfxFloat)fontMetrics.xHeight / fontMetrics.designUnitsPerEm;
        break;
      case FontSizeAdjust::Tag::CapHeight:
        aspect = (gfxFloat)fontMetrics.capHeight / fontMetrics.designUnitsPerEm;
        break;
      case FontSizeAdjust::Tag::ChWidth: {
        gfxFloat advance = GetCharAdvance('0');
        aspect = advance > 0.0 ? advance / mAdjustedSize : 0.5;
        break;
      }
      case FontSizeAdjust::Tag::IcWidth:
      case FontSizeAdjust::Tag::IcHeight: {
        bool vertical = FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) ==
                        FontSizeAdjust::Tag::IcHeight;
        gfxFloat advance = GetCharAdvance(0x6C34, vertical);
        aspect = advance > 0.0 ? advance / mAdjustedSize : 1.0;
        break;
      }
    }
    if (aspect > 0.0) {
      // If we created a shaper above (to measure glyphs), discard it so we
      // get a new one for the adjusted scaling.
      mHarfBuzzShaper = nullptr;
      mAdjustedSize = mStyle.GetAdjustedSize(aspect);
    }
  }

  // Update now that we've adjusted the size if necessary.
  mFUnitsConvFactor = float(mAdjustedSize / fontMetrics.designUnitsPerEm);

  // Note that GetMeasuringMode depends on mAdjustedSize
  if ((anAAOption == gfxFont::kAntialiasDefault && UsingClearType() &&
       GetMeasuringMode() == DWRITE_MEASURING_MODE_NATURAL) ||
      anAAOption == gfxFont::kAntialiasSubpixel) {
    mUseSubpixelPositions = true;
    // note that this may be reset to FALSE if we determine that a bitmap
    // strike is going to be used
  } else {
    mUseSubpixelPositions = false;
  }

  gfxDWriteFontEntry* fe = static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
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

  mMetrics->xHeight = fontMetrics.xHeight * mFUnitsConvFactor;
  mMetrics->capHeight = fontMetrics.capHeight * mFUnitsConvFactor;

  mMetrics->maxAscent = round(fontMetrics.ascent * mFUnitsConvFactor);
  mMetrics->maxDescent = round(fontMetrics.descent * mFUnitsConvFactor);
  mMetrics->maxHeight = mMetrics->maxAscent + mMetrics->maxDescent;

  mMetrics->emHeight = mAdjustedSize;
  mMetrics->emAscent =
      mMetrics->emHeight * mMetrics->maxAscent / mMetrics->maxHeight;
  mMetrics->emDescent = mMetrics->emHeight - mMetrics->emAscent;

  mMetrics->maxAdvance = mAdjustedSize;

  // try to get the true maxAdvance value from 'hhea'
  gfxFontEntry::AutoTable hheaTable(GetFontEntry(),
                                    TRUETYPE_TAG('h', 'h', 'e', 'a'));
  if (hheaTable) {
    uint32_t len;
    const MetricsHeader* hhea = reinterpret_cast<const MetricsHeader*>(
        hb_blob_get_data(hheaTable, &len));
    if (len >= sizeof(MetricsHeader)) {
      mMetrics->maxAdvance =
          uint16_t(hhea->advanceWidthMax) * mFUnitsConvFactor;
    }
  }

  mMetrics->internalLeading =
      std::max(mMetrics->maxHeight - mMetrics->emHeight, 0.0);
  mMetrics->externalLeading = ceil(fontMetrics.lineGap * mFUnitsConvFactor);

  UINT32 ucs = L' ';
  UINT16 glyph;
  if (SUCCEEDED(mFontFace->GetGlyphIndices(&ucs, 1, &glyph)) && glyph != 0) {
    mSpaceGlyph = glyph;
    mMetrics->spaceWidth = MeasureGlyphWidth(glyph);
  } else {
    mMetrics->spaceWidth = 0;
  }

  // try to get aveCharWidth from the OS/2 table, fall back to measuring 'x'
  // if the table is not available or if using hinted/pixel-snapped widths
  if (mUseSubpixelPositions) {
    mMetrics->aveCharWidth = 0;
    gfxFontEntry::AutoTable os2Table(GetFontEntry(),
                                     TRUETYPE_TAG('O', 'S', '/', '2'));
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
    mMetrics->aveCharWidth = GetCharAdvance('x');
    if (mMetrics->aveCharWidth < 1) {
      // Let's just assume the X is square.
      mMetrics->aveCharWidth = fontMetrics.xHeight * mFUnitsConvFactor;
    }
  }

  mMetrics->zeroWidth = GetCharAdvance('0');

  mMetrics->underlineOffset = fontMetrics.underlinePosition * mFUnitsConvFactor;
  mMetrics->underlineSize = fontMetrics.underlineThickness * mFUnitsConvFactor;
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
    printf("    spaceWidth: %f aveCharWidth: %f zeroWidth: %f\n",
           mMetrics->spaceWidth, mMetrics->aveCharWidth, mMetrics->zeroWidth);
    printf("    xHeight: %f capHeight: %f\n", mMetrics->xHeight, mMetrics->capHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f\n",
           mMetrics->underlineOffset, mMetrics->underlineSize, mMetrics->strikeoutOffset, mMetrics->strikeoutSize);
#endif
}

using namespace mozilla;  // for AutoSwap_* types

struct EBLCHeader {
  AutoSwap_PRUint32 version;
  AutoSwap_PRUint32 numSizes;
};

struct SbitLineMetrics {
  int8_t ascender;
  int8_t descender;
  uint8_t widthMax;
  int8_t caretSlopeNumerator;
  int8_t caretSlopeDenominator;
  int8_t caretOffset;
  int8_t minOriginSB;
  int8_t minAdvanceSB;
  int8_t maxBeforeBL;
  int8_t minAfterBL;
  int8_t pad1;
  int8_t pad2;
};

struct BitmapSizeTable {
  AutoSwap_PRUint32 indexSubTableArrayOffset;
  AutoSwap_PRUint32 indexTablesSize;
  AutoSwap_PRUint32 numberOfIndexSubTables;
  AutoSwap_PRUint32 colorRef;
  SbitLineMetrics hori;
  SbitLineMetrics vert;
  AutoSwap_PRUint16 startGlyphIndex;
  AutoSwap_PRUint16 endGlyphIndex;
  uint8_t ppemX;
  uint8_t ppemY;
  uint8_t bitDepth;
  uint8_t flags;
};

typedef EBLCHeader EBSCHeader;

struct BitmapScaleTable {
  SbitLineMetrics hori;
  SbitLineMetrics vert;
  uint8_t ppemX;
  uint8_t ppemY;
  uint8_t substitutePpemX;
  uint8_t substitutePpemY;
};

bool gfxDWriteFont::HasBitmapStrikeForSize(uint32_t aSize) {
  uint8_t* tableData;
  uint32_t len;
  void* tableContext;
  BOOL exists;
  HRESULT hr = mFontFace->TryGetFontTable(
      DWRITE_MAKE_OPENTYPE_TAG('E', 'B', 'L', 'C'), (const void**)&tableData,
      &len, &tableContext, &exists);
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
    const EBLCHeader* hdr = reinterpret_cast<const EBLCHeader*>(tableData);
    if (hdr->version != 0x00020000) {
      break;
    }
    uint32_t numSizes = hdr->numSizes;
    if (numSizes > 0xffff) {  // sanity-check, prevent overflow below
      break;
    }
    if (len < sizeof(EBLCHeader) + numSizes * sizeof(BitmapSizeTable)) {
      break;
    }
    const BitmapSizeTable* sizeTable =
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
                                  (const void**)&tableData, &len, &tableContext,
                                  &exists);
  if (FAILED(hr)) {
    return false;
  }

  while (exists) {
    if (len < sizeof(EBSCHeader)) {
      break;
    }
    const EBSCHeader* hdr = reinterpret_cast<const EBSCHeader*>(tableData);
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
    const BitmapScaleTable* scaleTable =
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

bool gfxDWriteFont::IsValid() const { return mFontFace != nullptr; }

IDWriteFontFace* gfxDWriteFont::GetFontFace() { return mFontFace.get(); }

gfxFont::RunMetrics gfxDWriteFont::Measure(const gfxTextRun* aTextRun,
                                           uint32_t aStart, uint32_t aEnd,
                                           BoundingBoxType aBoundingBoxType,
                                           DrawTarget* aRefDrawTarget,
                                           Spacing* aSpacing,
                                           gfx::ShapedTextFlags aOrientation) {
  gfxFont::RunMetrics metrics =
      gfxFont::Measure(aTextRun, aStart, aEnd, aBoundingBoxType, aRefDrawTarget,
                       aSpacing, aOrientation);

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
    metrics.mBoundingBox.SetWidth(metrics.mBoundingBox.Width() +
                                  aTextRun->GetAppUnitsPerDevUnit() * 3);
  }

  return metrics;
}

bool gfxDWriteFont::ProvidesGlyphWidths() const {
  return !mUseSubpixelPositions ||
         (mFontFace->GetSimulations() & DWRITE_FONT_SIMULATIONS_BOLD) ||
         ((gfxDWriteFontEntry*)(GetFontEntry()))->HasVariations();
}

int32_t gfxDWriteFont::GetGlyphWidth(uint16_t aGID) {
  if (!mGlyphWidths) {
    mGlyphWidths = MakeUnique<nsTHashMap<nsUint32HashKey, int32_t>>(128);
  }

  return mGlyphWidths->LookupOrInsertWith(
      aGID, [&] { return NS_lround(MeasureGlyphWidth(aGID) * 65536.0); });
}

bool gfxDWriteFont::GetForceGDIClassic() const {
  return sForceGDIClassicEnabled &&
         static_cast<gfxDWriteFontEntry*>(mFontEntry.get())
             ->GetForceGDIClassic() &&
         GetAdjustedSize() <= gfxDWriteFontList::PlatformFontList()
                                  ->GetForceGDIClassicMaxFontSize();
}

DWRITE_MEASURING_MODE
gfxDWriteFont::GetMeasuringMode() const {
  return DWriteSettings::Get(GetForceGDIClassic()).MeasuringMode();
}

gfxFloat gfxDWriteFont::MeasureGlyphWidth(uint16_t aGlyph) {
  MOZ_SEH_TRY {
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
            GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_NATURAL, FALSE, 1,
            &aGlyph, &advance);
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
            GetMeasuringMode() == DWRITE_MEASURING_MODE_GDI_NATURAL, &aGlyph, 1,
            &metrics, FALSE);
        if (SUCCEEDED(hr)) {
          return NS_lround(metrics.advanceWidth * mFUnitsConvFactor);
        }
      }
    }
  }
  MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    // Exception (e.g. disk i/o error) occurred when DirectWrite tried to use
    // the font resource; possibly a failing drive or similar hardware issue.
    // Mark the font as invalid, and wipe the fontEntry's charmap so that font
    // selection will skip it; we'll use a fallback font instead.
    mIsValid = false;
    GetFontEntry()->mCharacterMap = new gfxCharacterMap();
    GetFontEntry()->mShmemCharacterMap = nullptr;
    gfxCriticalError() << "Exception occurred measuring glyph width for "
                       << GetFontEntry()->Name().get();
  }
  return 0.0;
}

bool gfxDWriteFont::GetGlyphBounds(uint16_t aGID, gfxRect* aBounds,
                                   bool aTight) {
  DWRITE_GLYPH_METRICS m;
  HRESULT hr = mFontFace->GetDesignGlyphMetrics(&aGID, 1, &m, FALSE);
  if (FAILED(hr)) {
    return false;
  }
  gfxRect bounds(m.leftSideBearing, m.topSideBearing - m.verticalOriginY,
                 m.advanceWidth - m.leftSideBearing - m.rightSideBearing,
                 m.advanceHeight - m.topSideBearing - m.bottomSideBearing);
  bounds.Scale(mFUnitsConvFactor);
  // GetDesignGlyphMetrics returns 'ideal' glyph metrics, we need to pad to
  // account for antialiasing.
  if (!aTight && !aBounds->IsEmpty()) {
    bounds.Inflate(1.0, 0.0);
  }
  *aBounds = bounds;
  return true;
}

void gfxDWriteFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                           FontCacheSizes* aSizes) const {
  gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
  aSizes->mFontInstances += aMallocSizeOf(mMetrics);
  if (mGlyphWidths) {
    aSizes->mFontInstances +=
        mGlyphWidths->ShallowSizeOfIncludingThis(aMallocSizeOf);
  }
}

void gfxDWriteFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                           FontCacheSizes* aSizes) const {
  aSizes->mFontInstances += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}

already_AddRefed<ScaledFont> gfxDWriteFont::GetScaledFont(
    mozilla::gfx::DrawTarget* aTarget) {
  if (mAzureScaledFontUsedClearType != UsingClearType()) {
    mAzureScaledFont = nullptr;
  }
  if (!mAzureScaledFont) {
    gfxDWriteFontEntry* fe = static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
    bool forceGDI = GetForceGDIClassic();
    bool useEmbeddedBitmap =
        (gfxVars::SystemTextRenderingMode() == DWRITE_RENDERING_MODE_DEFAULT ||
         forceGDI) &&
        fe->IsCJKFont() && HasBitmapStrikeForSize(NS_lround(mAdjustedSize));

    const gfxFontStyle* fontStyle = GetStyle();
    mAzureScaledFont = Factory::CreateScaledFontForDWriteFont(
        mFontFace, fontStyle, GetUnscaledFont(), GetAdjustedSize(),
        useEmbeddedBitmap, forceGDI);
    if (!mAzureScaledFont) {
      return nullptr;
    }
    InitializeScaledFont();
    mAzureScaledFontUsedClearType = UsingClearType();
  }

  RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
  return scaledFont.forget();
}

already_AddRefed<ScaledFont> gfxDWriteFont::GetScaledFontNoGDI(
    mozilla::gfx::DrawTarget* aTarget) {
  if (!GetForceGDIClassic()) {
    return GetScaledFont(aTarget);
  }

  if (mAzureScaledFontNoGDIUsedClearType != UsingClearType()) {
    mAzureScaledFontNoGDI = nullptr;
  }
  if (!mAzureScaledFontNoGDI) {
    gfxDWriteFontEntry* fe = static_cast<gfxDWriteFontEntry*>(mFontEntry.get());
    bool useEmbeddedBitmap =
        gfxVars::SystemTextRenderingMode() == DWRITE_RENDERING_MODE_DEFAULT &&
        fe->IsCJKFont() && HasBitmapStrikeForSize(NS_lround(mAdjustedSize));

    const gfxFontStyle* fontStyle = GetStyle();
    mAzureScaledFontNoGDI = Factory::CreateScaledFontForDWriteFont(
        mFontFace, fontStyle, GetUnscaledFont(), GetAdjustedSize(),
        useEmbeddedBitmap, false);
    if (!mAzureScaledFontNoGDI) {
      return nullptr;
    }
    float angle = AngleForSyntheticOblique();
    if (angle != 0.0f) {
      mAzureScaledFontNoGDI->SetSyntheticObliqueAngle(angle);
    }
    mAzureScaledFontNoGDIUsedClearType = UsingClearType();
  }

  RefPtr<ScaledFont> scaledFont(mAzureScaledFontNoGDI);
  return scaledFont.forget();
}

bool gfxDWriteFont::ShouldRoundXOffset(cairo_t* aCairo) const {
  // show_glyphs is implemented on the font and so is used for all Cairo
  // surface types; however, it may pixel-snap depending on the dwrite
  // rendering mode
  return GetMeasuringMode() != DWRITE_MEASURING_MODE_NATURAL;
}
