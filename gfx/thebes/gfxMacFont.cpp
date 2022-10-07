/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxMacFont.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"
#include "mozilla/StaticPrefs_gfx.h"

#include "gfxCoreTextShaper.h"
#include <algorithm>
#include "gfxPlatformMac.h"
#include "gfxContext.h"
#include "gfxFontUtils.h"
#include "gfxHarfBuzzShaper.h"
#include "gfxMacPlatformFontList.h"
#include "gfxFontConstants.h"
#include "gfxTextRun.h"
#include "gfxUtils.h"
#include "nsCocoaFeatures.h"
#include "AppleUtils.h"
#include "cairo-quartz.h"

using namespace mozilla;
using namespace mozilla::gfx;

template <class T>
struct TagEquals {
  bool Equals(const T& aIter, uint32_t aTag) const {
    return aIter.mTag == aTag;
  }
};

gfxMacFont::gfxMacFont(const RefPtr<UnscaledFontMac>& aUnscaledFont,
                       MacOSFontEntry* aFontEntry,
                       const gfxFontStyle* aFontStyle)
    : gfxFont(aUnscaledFont, aFontEntry, aFontStyle),
      mCGFont(nullptr),
      mCTFont(nullptr),
      mFontSmoothingBackgroundColor(aFontStyle->fontSmoothingBackgroundColor),
      mVariationFont(aFontEntry->HasVariations()) {
  mApplySyntheticBold = aFontStyle->NeedsSyntheticBold(aFontEntry);

  if (mVariationFont) {
    CGFontRef baseFont = aUnscaledFont->GetFont();
    if (!baseFont) {
      mIsValid = false;
      return;
    }

    // Get the variation settings needed to instantiate the fontEntry
    // for a particular fontStyle.
    AutoTArray<gfxFontVariation, 4> vars;
    aFontEntry->GetVariationsForStyle(vars, *aFontStyle);

    if (aFontEntry->HasOpticalSize()) {
      // Because of a Core Text bug, we need to ensure that if the font has
      // an 'opsz' axis, it is always explicitly set, and NOT to the font's
      // default value. (See bug 1457417, bug 1478720.)
      // We record the result of searching the font's axes in the font entry,
      // so that this only has to be done by the first instance created for
      // a given font resource.
      const uint32_t kOpszTag = HB_TAG('o', 'p', 's', 'z');
      const float kOpszFudgeAmount = 0.01f;

      // Record the opsz axis details in the font entry, if not already done.
      if (!aFontEntry->mOpszAxis.mTag) {
        AutoTArray<gfxFontVariationAxis, 4> axes;
        aFontEntry->GetVariationAxes(axes);
        auto index =
            axes.IndexOf(kOpszTag, 0, TagEquals<gfxFontVariationAxis>());
        MOZ_ASSERT(index != axes.NoIndex);
        if (index != axes.NoIndex) {
          const auto& axis = axes[index];
          aFontEntry->mOpszAxis = axis;
          // Pick a slightly-adjusted version of the default that we'll
          // use to work around Core Text's habit of ignoring any attempt
          // to explicitly set the default value.
          aFontEntry->mAdjustedDefaultOpsz =
              axis.mDefaultValue == axis.mMinValue
                  ? axis.mDefaultValue + kOpszFudgeAmount
                  : axis.mDefaultValue - kOpszFudgeAmount;
        }
      }

      // Add 'opsz' if not present, or tweak its value if it looks too close
      // to the default (after clamping to the font's available range).
      auto index = vars.IndexOf(kOpszTag, 0, TagEquals<gfxFontVariation>());
      if (index == vars.NoIndex) {
        // No explicit opsz; set to the font's default.
        vars.AppendElement(
            gfxFontVariation{kOpszTag, aFontEntry->mAdjustedDefaultOpsz});
      } else {
        // An 'opsz' value was already present; use it, but adjust if necessary
        // to a "safe" value that Core Text won't ignore.
        auto& value = vars[index].mValue;
        auto& axis = aFontEntry->mOpszAxis;
        value = fmin(fmax(value, axis.mMinValue), axis.mMaxValue);
        if (std::abs(value - axis.mDefaultValue) < kOpszFudgeAmount) {
          value = aFontEntry->mAdjustedDefaultOpsz;
        }
      }
    }

    mCGFont = UnscaledFontMac::CreateCGFontWithVariations(
        baseFont, aUnscaledFont->CGAxesCache(), aUnscaledFont->CTAxesCache(),
        vars.Length(), vars.Elements());
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

  // turn off font anti-aliasing based on user pref setting
  if (mAdjustedSize <=
      (gfxFloat)gfxPlatformMac::GetPlatform()->GetAntiAliasingThreshold()) {
    mAntialiasOption = kAntialiasNone;
  } else if (mStyle.useGrayscaleAntialiasing) {
    mAntialiasOption = kAntialiasGrayscale;
  }
}

gfxMacFont::~gfxMacFont() {
  if (mCGFont) {
    ::CFRelease(mCGFont);
  }
  if (mCTFont) {
    ::CFRelease(mCTFont);
  }
}

bool gfxMacFont::ShapeText(DrawTarget* aDrawTarget, const char16_t* aText,
                           uint32_t aOffset, uint32_t aLength, Script aScript,
                           nsAtom* aLanguage, bool aVertical,
                           RoundingFlags aRounding,
                           gfxShapedText* aShapedText) {
  if (!mIsValid) {
    NS_WARNING("invalid font! expect incorrect text rendering");
    return false;
  }

  // Currently, we don't support vertical shaping via CoreText,
  // so we ignore RequiresAATLayout if vertical is requested.
  auto macFontEntry = static_cast<MacOSFontEntry*>(GetFontEntry());
  if (macFontEntry->RequiresAATLayout() && !aVertical &&
      StaticPrefs::gfx_font_rendering_coretext_enabled()) {
    if (!mCoreTextShaper) {
      mCoreTextShaper = MakeUnique<gfxCoreTextShaper>(this);
    }
    if (mCoreTextShaper->ShapeText(aDrawTarget, aText, aOffset, aLength,
                                   aScript, aLanguage, aVertical, aRounding,
                                   aShapedText)) {
      PostShapingFixup(aDrawTarget, aText, aOffset, aLength, aVertical,
                       aShapedText);
      if (GetFontEntry()->HasTrackingTable()) {
        // Convert font size from device pixels back to CSS px
        // to use in selecting tracking value
        float trackSize = GetAdjustedSize() *
                          aShapedText->GetAppUnitsPerDevUnit() /
                          AppUnitsPerCSSPixel();
        float tracking =
            GetFontEntry()->TrackingForCSSPx(trackSize) * mFUnitsConvFactor;
        // Applying tracking is a lot like the adjustment we do for
        // synthetic bold: we want to apply between clusters, not to
        // non-spacing glyphs within a cluster. So we can reuse that
        // helper here.
        aShapedText->AdjustAdvancesForSyntheticBold(tracking, aOffset, aLength);
      }
      return true;
    }
  }

  return gfxFont::ShapeText(aDrawTarget, aText, aOffset, aLength, aScript,
                            aLanguage, aVertical, aRounding, aShapedText);
}

gfxFont::RunMetrics gfxMacFont::Measure(const gfxTextRun* aTextRun,
                                        uint32_t aStart, uint32_t aEnd,
                                        BoundingBoxType aBoundingBoxType,
                                        DrawTarget* aRefDrawTarget,
                                        Spacing* aSpacing,
                                        gfx::ShapedTextFlags aOrientation) {
  gfxFont::RunMetrics metrics =
      gfxFont::Measure(aTextRun, aStart, aEnd, aBoundingBoxType, aRefDrawTarget,
                       aSpacing, aOrientation);

  // if aBoundingBoxType is not TIGHT_HINTED_OUTLINE_EXTENTS then we need to add
  // a pixel column each side of the bounding box in case of antialiasing
  // "bleed"
  if (aBoundingBoxType != TIGHT_HINTED_OUTLINE_EXTENTS &&
      metrics.mBoundingBox.width > 0) {
    metrics.mBoundingBox.x -= aTextRun->GetAppUnitsPerDevUnit();
    metrics.mBoundingBox.width += aTextRun->GetAppUnitsPerDevUnit() * 2;
  }

  return metrics;
}

void gfxMacFont::InitMetrics() {
  mIsValid = false;
  ::memset(&mMetrics, 0, sizeof(mMetrics));

  uint32_t upem = 0;

  // try to get unitsPerEm from sfnt head table, to avoid calling CGFont
  // if possible (bug 574368) and because CGFontGetUnitsPerEm does not
  // return the true value for OpenType/CFF fonts (it normalizes to 1000,
  // which then leads to metrics errors when we read the 'hmtx' table to
  // get glyph advances for HarfBuzz, see bug 580863)
  AutoCFRelease<CFDataRef> headData =
      ::CGFontCopyTableForTag(mCGFont, TRUETYPE_TAG('h', 'e', 'a', 'd'));
  if (headData) {
    if (size_t(::CFDataGetLength(headData)) >= sizeof(HeadTable)) {
      const HeadTable* head =
          reinterpret_cast<const HeadTable*>(::CFDataGetBytePtr(headData));
      upem = head->unitsPerEm;
    }
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
                   mFontEntry->Name().get());
    NS_WARNING(warnBuf);
#endif
    return;
  }

  // Apply any size-adjust from the font enty to the given size; this may be
  // re-adjusted below if font-size-adjust is in effect.
  mAdjustedSize = GetAdjustedSize();
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

  AutoCFRelease<CFDataRef> cmap =
      ::CGFontCopyTableForTag(mCGFont, TRUETYPE_TAG('c', 'm', 'a', 'p'));

  uint32_t glyphID;
  mMetrics.zeroWidth = GetCharWidth(cmap, '0', &glyphID, cgConvFactor);
  if (glyphID == 0) {
    mMetrics.zeroWidth = -1.0;  // indicates not found
  }

  if (FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) !=
          FontSizeAdjust::Tag::None &&
      mStyle.sizeAdjust >= 0.0 && GetAdjustedSize() > 0.0) {
    // apply font-size-adjust, and recalculate metrics
    gfxFloat aspect;
    switch (FontSizeAdjust::Tag(mStyle.sizeAdjustBasis)) {
      default:
        MOZ_ASSERT_UNREACHABLE("unhandled sizeAdjustBasis?");
        aspect = 0.0;
        break;
      case FontSizeAdjust::Tag::ExHeight:
        aspect = mMetrics.xHeight / mAdjustedSize;
        break;
      case FontSizeAdjust::Tag::CapHeight:
        aspect = mMetrics.capHeight / mAdjustedSize;
        break;
      case FontSizeAdjust::Tag::ChWidth:
        aspect =
            mMetrics.zeroWidth < 0.0 ? 0.5 : mMetrics.zeroWidth / mAdjustedSize;
        break;
      case FontSizeAdjust::Tag::IcWidth:
      case FontSizeAdjust::Tag::IcHeight: {
        bool vertical = FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) ==
                        FontSizeAdjust::Tag::IcHeight;
        gfxFloat advance = GetCharAdvance(kWaterIdeograph, vertical);
        aspect = advance > 0.0 ? advance / mAdjustedSize : 1.0;
        break;
      }
    }
    if (aspect > 0.0) {
      // If we created a shaper above (to measure glyphs), discard it so we
      // get a new one for the adjusted scaling.
      delete mHarfBuzzShaper.exchange(nullptr);
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
      // Update metrics from the re-scaled font.
      if (mMetrics.xHeight == 0.0) {
        mMetrics.xHeight = ::CGFontGetXHeight(mCGFont) * cgConvFactor;
      }
      if (mMetrics.capHeight == 0.0) {
        mMetrics.capHeight = ::CGFontGetCapHeight(mCGFont) * cgConvFactor;
      }
      mMetrics.zeroWidth = GetCharWidth(cmap, '0', &glyphID, cgConvFactor);
      if (glyphID == 0) {
        mMetrics.zeroWidth = -1.0;  // indicates not found
      }
    }
  }

  // Once we reach here, we've got basic metrics and set mIsValid = TRUE;
  // there should be no further points of actual failure in InitMetrics().
  // (If one is introduced, be sure to reset mIsValid to FALSE!)

  mMetrics.emHeight = mAdjustedSize;

  // Measure/calculate additional metrics, independent of whether we used
  // the tables directly or ATS metrics APIs

  if (mMetrics.aveCharWidth <= 0) {
    mMetrics.aveCharWidth = GetCharWidth(cmap, 'x', &glyphID, cgConvFactor);
    if (glyphID == 0) {
      // we didn't find 'x', so use maxAdvance rather than zero
      mMetrics.aveCharWidth = mMetrics.maxAdvance;
    }
  }

  mMetrics.spaceWidth = GetCharWidth(cmap, ' ', &glyphID, cgConvFactor);
  if (glyphID == 0) {
    // no space glyph?!
    mMetrics.spaceWidth = mMetrics.aveCharWidth;
  }
  mSpaceGlyph = glyphID;

  mMetrics.ideographicWidth =
      GetCharWidth(cmap, kWaterIdeograph, &glyphID, cgConvFactor);
  if (glyphID == 0) {
    // Indicate "not found".
    mMetrics.ideographicWidth = -1.0;
  }

  CalculateDerivedMetrics(mMetrics);

  SanitizeMetrics(&mMetrics, mFontEntry->mIsBadUnderlineFont);

  if (ApplySyntheticBold()) {
    auto delta = GetSyntheticBoldOffset();
    mMetrics.spaceWidth += delta;
    mMetrics.aveCharWidth += delta;
    mMetrics.maxAdvance += delta;
    if (mMetrics.zeroWidth > 0) {
      mMetrics.zeroWidth += delta;
    }
    if (mMetrics.ideographicWidth > 0) {
      mMetrics.ideographicWidth += delta;
    }
  }

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

gfxFloat gfxMacFont::GetCharWidth(CFDataRef aCmap, char16_t aUniChar,
                                  uint32_t* aGlyphID, gfxFloat aConvFactor) {
  CGGlyph glyph = 0;

  if (aCmap) {
    glyph = gfxFontUtils::MapCharToGlyph(::CFDataGetBytePtr(aCmap),
                                         ::CFDataGetLength(aCmap), aUniChar);
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
CTFontRef gfxMacFont::CreateCTFontFromCGFontWithVariations(
    CGFontRef aCGFont, CGFloat aSize, bool aInstalledFont,
    CTFontDescriptorRef aFontDesc) {
  // Avoid calling potentially buggy variation APIs on pre-Sierra macOS
  // versions (see bug 1331683).
  //
  // And on HighSierra, CTFontCreateWithGraphicsFont properly carries over
  // variation settings from the CGFont to CTFont, so we don't need to do
  // the extra work here -- and this seems to avoid Core Text crashiness
  // seen in bug 1454094.
  //
  // However, for installed fonts it seems we DO need to copy the variations
  // explicitly even on 10.13, otherwise fonts fail to render (as in bug
  // 1455494) when non-default values are used. Fortunately, the crash
  // mentioned above occurs with data fonts, not (AFAICT) with system-
  // installed fonts.
  //
  // So we only need to do this "the hard way" on Sierra, and on HighSierra
  // for system-installed fonts; in other cases just let the standard CTFont
  // function do its thing.
  //
  // NOTE in case this ever needs further adjustment: there is similar logic
  // in four places in the tree (sadly):
  //    CreateCTFontFromCGFontWithVariations in gfxMacFont.cpp
  //    CreateCTFontFromCGFontWithVariations in ScaledFontMac.cpp
  //    CreateCTFontFromCGFontWithVariations in cairo-quartz-font.c
  //    ctfont_create_exact_copy in SkFontHost_mac.cpp

  CTFontRef ctFont;
  if (nsCocoaFeatures::OnSierraExactly() ||
      (aInstalledFont && nsCocoaFeatures::OnHighSierraOrLater())) {
    AutoCFRelease<CFDictionaryRef> variations = ::CGFontCopyVariations(aCGFont);
    if (variations) {
      AutoCFRelease<CFDictionaryRef> varAttr = ::CFDictionaryCreate(
          nullptr, (const void**)&kCTFontVariationAttribute,
          (const void**)&variations, 1, &kCFTypeDictionaryKeyCallBacks,
          &kCFTypeDictionaryValueCallBacks);

      AutoCFRelease<CTFontDescriptorRef> varDesc =
          aFontDesc
              ? ::CTFontDescriptorCreateCopyWithAttributes(aFontDesc, varAttr)
              : ::CTFontDescriptorCreateWithAttributes(varAttr);

      ctFont = ::CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr, varDesc);
    } else {
      ctFont =
          ::CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr, aFontDesc);
    }
  } else {
    ctFont = ::CTFontCreateWithGraphicsFont(aCGFont, aSize, nullptr, aFontDesc);
  }

  return ctFont;
}

int32_t gfxMacFont::GetGlyphWidth(uint16_t aGID) {
  if (mVariationFont) {
    // Avoid a potential Core Text crash (bug 1450209) by using
    // CoreGraphics glyph advance API. This is inferior for 'sbix'
    // fonts, but those won't have variations, so it's OK.
    int cgAdvance;
    if (::CGFontGetGlyphAdvances(mCGFont, &aGID, 1, &cgAdvance)) {
      return cgAdvance * mFUnitsConvFactor * 0x10000;
    }
  }

  if (!mCTFont) {
    bool isInstalledFont =
        !mFontEntry->IsUserFont() || mFontEntry->IsLocalUserFont();
    mCTFont = CreateCTFontFromCGFontWithVariations(mCGFont, mAdjustedSize,
                                                   isInstalledFont);
    if (!mCTFont) {  // shouldn't happen, but let's be safe
      NS_WARNING("failed to create CTFontRef to measure glyph width");
      return 0;
    }
  }

  CGSize advance;
  ::CTFontGetAdvancesForGlyphs(mCTFont, kCTFontOrientationDefault, &aGID,
                               &advance, 1);
  return advance.width * 0x10000;
}

bool gfxMacFont::GetGlyphBounds(uint16_t aGID, gfxRect* aBounds,
                                bool aTight) const {
  CGRect bb;
  if (!::CGFontGetGlyphBBoxes(mCGFont, &aGID, 1, &bb)) {
    return false;
  }

  // broken fonts can return incorrect bounds for some null characters,
  // see https://bugzilla.mozilla.org/show_bug.cgi?id=534260
  if (bb.origin.x == -32767 && bb.origin.y == -32767 &&
      bb.size.width == 65534 && bb.size.height == 65534) {
    *aBounds = gfxRect(0, 0, 0, 0);
    return true;
  }

  gfxRect bounds(bb.origin.x, -(bb.origin.y + bb.size.height), bb.size.width,
                 bb.size.height);
  bounds.Scale(mFUnitsConvFactor);
  *aBounds = bounds;
  return true;
}

// Try to initialize font metrics via platform APIs (CG/CT),
// and set mIsValid = TRUE on success.
// We ONLY call this for local (platform) fonts that are not sfnt format;
// for sfnts, including ALL downloadable fonts, we prefer to use
// InitMetricsFromSfntTables and avoid platform APIs.
void gfxMacFont::InitMetricsFromPlatform() {
  AutoCFRelease<CTFontRef> ctFont =
      ::CTFontCreateWithGraphicsFont(mCGFont, mAdjustedSize, nullptr, nullptr);
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

  mIsValid = true;
}

already_AddRefed<ScaledFont> gfxMacFont::GetScaledFont(
    const TextRunDrawParams& aRunParams) {
  if (ScaledFont* scaledFont = mAzureScaledFont) {
    return do_AddRef(scaledFont);
  }

  gfxFontEntry* fe = GetFontEntry();
  bool hasColorGlyphs = fe->HasColorBitmapTable() || fe->TryGetColorGlyphs();
  RefPtr<ScaledFont> newScaledFont = Factory::CreateScaledFontForMacFont(
      GetCGFontRef(), GetUnscaledFont(), GetAdjustedSize(),
      ToDeviceColor(mFontSmoothingBackgroundColor),
      !mStyle.useGrayscaleAntialiasing, ApplySyntheticBold(), hasColorGlyphs);
  if (!newScaledFont) {
    return nullptr;
  }

  InitializeScaledFont(newScaledFont);

  if (mAzureScaledFont.compareExchange(nullptr, newScaledFont.get())) {
    Unused << newScaledFont.forget();
  }
  ScaledFont* scaledFont = mAzureScaledFont;
  return do_AddRef(scaledFont);
}

bool gfxMacFont::ShouldRoundXOffset(cairo_t* aCairo) const {
  // Quartz surfaces implement show_glyphs for Quartz fonts
  return aCairo && cairo_surface_get_type(cairo_get_target(aCairo)) !=
                       CAIRO_SURFACE_TYPE_QUARTZ;
}

bool gfxMacFont::UseNativeColrFontSupport() const {
  if (nsCocoaFeatures::OnHighSierraOrLater()) {
    auto* colr = GetFontEntry()->GetCOLR();
    if (colr && COLRFonts::GetColrTableVersion(colr) == 0) {
      return true;
    }
  }
  return false;
}

void gfxMacFont::AddSizeOfExcludingThis(MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const {
  gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
  // mCGFont is shared with the font entry, so not counted here;
}

void gfxMacFont::AddSizeOfIncludingThis(MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const {
  aSizes->mFontInstances += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}
