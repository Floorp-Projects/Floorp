/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxGDIFont.h"

#include "mozilla/MemoryReporting.h"
#include "mozilla/Sprintf.h"
#include "mozilla/WindowsVersion.h"

#include <algorithm>
#include "gfxWindowsPlatform.h"
#include "gfxContext.h"
#include "mozilla/Preferences.h"
#include "nsUnicodeProperties.h"
#include "gfxFontConstants.h"
#include "gfxTextRun.h"

#include "cairo-win32.h"

#define ROUND(x) floor((x) + 0.5)

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::unicode;

gfxGDIFont::gfxGDIFont(GDIFontEntry* aFontEntry, const gfxFontStyle* aFontStyle,
                       AntialiasOption anAAOption)
    : gfxFont(nullptr, aFontEntry, aFontStyle, anAAOption),
      mFont(nullptr),
      mMetrics(nullptr),
      mIsBitmap(false),
      mScriptCache(nullptr) {
  mNeedsSyntheticBold = aFontStyle->NeedsSyntheticBold(aFontEntry);

  Initialize();

  if (mFont) {
    mUnscaledFont = aFontEntry->LookupUnscaledFont(mFont);
  }
}

gfxGDIFont::~gfxGDIFont() {
  if (mFont) {
    ::DeleteObject(mFont);
  }
  if (mScriptCache) {
    ScriptFreeCache(&mScriptCache);
  }
  delete mMetrics;
}

UniquePtr<gfxFont> gfxGDIFont::CopyWithAntialiasOption(
    AntialiasOption anAAOption) {
  auto entry = static_cast<GDIFontEntry*>(mFontEntry.get());
  return MakeUnique<gfxGDIFont>(entry, &mStyle, anAAOption);
}

bool gfxGDIFont::ShapeText(DrawTarget* aDrawTarget, const char16_t* aText,
                           uint32_t aOffset, uint32_t aLength, Script aScript,
                           nsAtom* aLanguage, bool aVertical,
                           RoundingFlags aRounding,
                           gfxShapedText* aShapedText) {
  if (!mIsValid) {
    NS_WARNING("invalid font! expect incorrect text rendering");
    return false;
  }

  return gfxFont::ShapeText(aDrawTarget, aText, aOffset, aLength, aScript,
                            aLanguage, aVertical, aRounding, aShapedText);
}

const gfxFont::Metrics& gfxGDIFont::GetHorizontalMetrics() { return *mMetrics; }

already_AddRefed<ScaledFont> gfxGDIFont::GetScaledFont(DrawTarget* aTarget) {
  if (!mAzureScaledFont) {
    LOGFONT lf;
    GetObject(GetHFONT(), sizeof(LOGFONT), &lf);

    mAzureScaledFont = Factory::CreateScaledFontForGDIFont(
        &lf, GetUnscaledFont(), GetAdjustedSize());
    InitializeScaledFont();
  }

  RefPtr<ScaledFont> scaledFont(mAzureScaledFont);
  return scaledFont.forget();
}

gfxFont::RunMetrics gfxGDIFont::Measure(const gfxTextRun* aTextRun,
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
      mAntialiasOption != kAntialiasNone && metrics.mBoundingBox.Width() > 0) {
    metrics.mBoundingBox.MoveByX(-aTextRun->GetAppUnitsPerDevUnit());
    metrics.mBoundingBox.SetWidth(metrics.mBoundingBox.Width() +
                                  aTextRun->GetAppUnitsPerDevUnit() * 3);
  }

  return metrics;
}

void gfxGDIFont::Initialize() {
  NS_ASSERTION(!mMetrics, "re-creating metrics? this will leak");

  LOGFONTW logFont;

  if (mAdjustedSize == 0.0) {
    mAdjustedSize = GetAdjustedSize();
    if (FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) !=
        FontSizeAdjust::Tag::None) {
      if (mStyle.sizeAdjust > 0.0 && mAdjustedSize > 0.0) {
        // to implement font-size-adjust, we first create the "unadjusted" font
        FillLogFont(logFont, mAdjustedSize);
        mFont = ::CreateFontIndirectW(&logFont);

        // initialize its metrics so we can calculate size adjustment
        Initialize();

        // Unless the font was so small that GDI metrics rounded to zero,
        // calculate the properly adjusted size, and then proceed
        // to recreate mFont and recalculate metrics
        if (mMetrics->emHeight > 0.0) {
          gfxFloat aspect;
          switch (FontSizeAdjust::Tag(mStyle.sizeAdjustBasis)) {
            default:
              MOZ_ASSERT_UNREACHABLE("unhandled sizeAdjustBasis?");
              aspect = 0.0;
              break;
            case FontSizeAdjust::Tag::ExHeight:
              aspect = mMetrics->xHeight / mMetrics->emHeight;
              break;
            case FontSizeAdjust::Tag::CapHeight:
              aspect = mMetrics->capHeight / mMetrics->emHeight;
              break;
            case FontSizeAdjust::Tag::ChWidth: {
              gfxFloat advance = GetCharAdvance('0');
              aspect = advance > 0.0 ? advance / mMetrics->emHeight : 0.5;
              break;
            }
            case FontSizeAdjust::Tag::IcWidth:
            case FontSizeAdjust::Tag::IcHeight: {
              bool vertical = FontSizeAdjust::Tag(mStyle.sizeAdjustBasis) ==
                              FontSizeAdjust::Tag::IcHeight;
              gfxFloat advance = GetCharAdvance(0x6C34, vertical);
              aspect = advance > 0.0 ? advance / mMetrics->emHeight : 1.0;
              break;
            }
          }
          if (aspect > 0.0) {
            // If we created a shaper above (to measure glyphs), discard it so
            // we get a new one for the adjusted scaling.
            mHarfBuzzShaper = nullptr;
            mAdjustedSize = mStyle.GetAdjustedSize(aspect);
          }
        }

        // delete the temporary font and metrics
        ::DeleteObject(mFont);
        mFont = nullptr;
        delete mMetrics;
        mMetrics = nullptr;
      } else {
        mAdjustedSize = 0.0;
      }
    }
  }

  // (bug 724231) for local user fonts, we don't use GDI's synthetic bold,
  // as it could lead to a different, incompatible face being used
  // but instead do our own multi-striking
  if (mNeedsSyntheticBold && GetFontEntry()->IsLocalUserFont()) {
    mApplySyntheticBold = true;
  }

  // this may end up being zero
  mAdjustedSize = ROUND(mAdjustedSize);
  FillLogFont(logFont, mAdjustedSize);
  mFont = ::CreateFontIndirectW(&logFont);

  mMetrics = new gfxFont::Metrics;
  ::memset(mMetrics, 0, sizeof(*mMetrics));

  if (!mFont) {
    NS_WARNING("Failed creating GDI font");
    mIsValid = false;
    return;
  }

  AutoDC dc;
  SetGraphicsMode(dc.GetDC(), GM_ADVANCED);
  AutoSelectFont selectFont(dc.GetDC(), mFont);

  // Get font metrics if size > 0
  if (mAdjustedSize > 0.0) {
    OUTLINETEXTMETRIC oMetrics;
    TEXTMETRIC& metrics = oMetrics.otmTextMetrics;

    if (0 < GetOutlineTextMetrics(dc.GetDC(), sizeof(oMetrics), &oMetrics)) {
      mMetrics->strikeoutSize = (double)oMetrics.otmsStrikeoutSize;
      mMetrics->strikeoutOffset = (double)oMetrics.otmsStrikeoutPosition;
      mMetrics->underlineSize = (double)oMetrics.otmsUnderscoreSize;
      mMetrics->underlineOffset = (double)oMetrics.otmsUnderscorePosition;

      const MAT2 kIdentityMatrix = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
      GLYPHMETRICS gm;
      DWORD len = GetGlyphOutlineW(dc.GetDC(), char16_t('x'), GGO_METRICS, &gm,
                                   0, nullptr, &kIdentityMatrix);
      if (len == GDI_ERROR || gm.gmptGlyphOrigin.y <= 0) {
        // 56% of ascent, best guess for true type
        mMetrics->xHeight =
            ROUND((double)metrics.tmAscent * DEFAULT_XHEIGHT_FACTOR);
      } else {
        mMetrics->xHeight = gm.gmptGlyphOrigin.y;
      }
      len = GetGlyphOutlineW(dc.GetDC(), char16_t('H'), GGO_METRICS, &gm, 0,
                             nullptr, &kIdentityMatrix);
      if (len == GDI_ERROR || gm.gmptGlyphOrigin.y <= 0) {
        mMetrics->capHeight = metrics.tmAscent - metrics.tmInternalLeading;
      } else {
        mMetrics->capHeight = gm.gmptGlyphOrigin.y;
      }
      mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
      gfxFloat typEmHeight =
          (double)oMetrics.otmAscent - (double)oMetrics.otmDescent;
      mMetrics->emAscent =
          ROUND(mMetrics->emHeight * (double)oMetrics.otmAscent / typEmHeight);
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
      mMetrics->strikeoutSize = 1;
      mMetrics->strikeoutOffset =
          ROUND(mMetrics->xHeight * 0.5f);  // 50% of xHeight
      mMetrics->underlineSize = 1;
      mMetrics->underlineOffset =
          -ROUND((float)metrics.tmDescent * 0.30f);  // 30% of descent
      mMetrics->emHeight = metrics.tmHeight - metrics.tmInternalLeading;
      mMetrics->emAscent = metrics.tmAscent - metrics.tmInternalLeading;
      mMetrics->emDescent = metrics.tmDescent;
      mMetrics->capHeight = mMetrics->emAscent;
    }

    mMetrics->internalLeading = metrics.tmInternalLeading;
    mMetrics->externalLeading = metrics.tmExternalLeading;
    mMetrics->maxHeight = metrics.tmHeight;
    mMetrics->maxAscent = metrics.tmAscent;
    mMetrics->maxDescent = metrics.tmDescent;
    mMetrics->maxAdvance = metrics.tmMaxCharWidth;
    mMetrics->aveCharWidth = std::max<gfxFloat>(1, metrics.tmAveCharWidth);
    // The font is monospace when TMPF_FIXED_PITCH is *not* set!
    // See http://msdn2.microsoft.com/en-us/library/ms534202(VS.85).aspx
    if (!(metrics.tmPitchAndFamily & TMPF_FIXED_PITCH)) {
      mMetrics->maxAdvance = mMetrics->aveCharWidth;
    }

    mIsBitmap = !(metrics.tmPitchAndFamily & TMPF_VECTOR);

    // For fonts with USE_TYPO_METRICS set in the fsSelection field,
    // let the OS/2 sTypo* metrics override the previous values.
    // (see http://www.microsoft.com/typography/otspec/os2.htm#fss)
    // Using the equivalent values from oMetrics provides inconsistent
    // results with CFF fonts, so we instead rely on OS2Table.
    gfxFontEntry::AutoTable os2Table(mFontEntry,
                                     TRUETYPE_TAG('O', 'S', '/', '2'));
    if (os2Table) {
      uint32_t len;
      const OS2Table* os2 =
          reinterpret_cast<const OS2Table*>(hb_blob_get_data(os2Table, &len));
      if (len >= offsetof(OS2Table, sTypoLineGap) + sizeof(int16_t)) {
        const uint16_t kUseTypoMetricsMask = 1 << 7;
        if ((uint16_t(os2->fsSelection) & kUseTypoMetricsMask)) {
          double ascent = int16_t(os2->sTypoAscender);
          double descent = int16_t(os2->sTypoDescender);
          double lineGap = int16_t(os2->sTypoLineGap);
          mMetrics->maxAscent = ROUND(ascent * mFUnitsConvFactor);
          mMetrics->maxDescent = -ROUND(descent * mFUnitsConvFactor);
          mMetrics->maxHeight = mMetrics->maxAscent + mMetrics->maxDescent;
          mMetrics->internalLeading = mMetrics->maxHeight - mMetrics->emHeight;
          gfxFloat lineHeight =
              ROUND((ascent - descent + lineGap) * mFUnitsConvFactor);
          lineHeight = std::max(lineHeight, mMetrics->maxHeight);
          mMetrics->externalLeading = lineHeight - mMetrics->maxHeight;
        }
      }
      // although sxHeight and sCapHeight are signed fields, we consider
      // negative values to be erroneous and just ignore them
      if (uint16_t(os2->version) >= 2) {
        // version 2 and later includes the x-height and cap-height fields
        if (len >= offsetof(OS2Table, sxHeight) + sizeof(int16_t) &&
            int16_t(os2->sxHeight) > 0) {
          mMetrics->xHeight = ROUND(int16_t(os2->sxHeight) * mFUnitsConvFactor);
        }
        if (len >= offsetof(OS2Table, sCapHeight) + sizeof(int16_t) &&
            int16_t(os2->sCapHeight) > 0) {
          mMetrics->capHeight =
              ROUND(int16_t(os2->sCapHeight) * mFUnitsConvFactor);
        }
      }
    }

    WORD glyph;
    SIZE size;
    DWORD ret = GetGlyphIndicesW(dc.GetDC(), L" ", 1, &glyph,
                                 GGI_MARK_NONEXISTING_GLYPHS);
    if (ret != GDI_ERROR && glyph != 0xFFFF) {
      mSpaceGlyph = glyph;
      // Cache the width of a single space.
      GetTextExtentPoint32W(dc.GetDC(), L" ", 1, &size);
      mMetrics->spaceWidth = ROUND(size.cx);
    } else {
      mMetrics->spaceWidth = mMetrics->aveCharWidth;
    }

    // Cache the width of digit zero, if available.
    ret = GetGlyphIndicesW(dc.GetDC(), L"0", 1, &glyph,
                           GGI_MARK_NONEXISTING_GLYPHS);
    if (ret != GDI_ERROR && glyph != 0xFFFF) {
      GetTextExtentPoint32W(dc.GetDC(), L"0", 1, &size);
      mMetrics->zeroWidth = ROUND(size.cx);
    } else {
      mMetrics->zeroWidth = -1.0;  // indicates not found
    }

    SanitizeMetrics(mMetrics, GetFontEntry()->mIsBadUnderlineFont);
  } else {
    mFUnitsConvFactor = 0.0;  // zero-sized font: all values scale to zero
  }

  if (IsSyntheticBold()) {
    mMetrics->aveCharWidth += GetSyntheticBoldOffset();
    mMetrics->maxAdvance += GetSyntheticBoldOffset();
  }

#if 0
    printf("Font: %p (%s) size: %f adjusted size: %f valid: %s\n", this,
           GetName().get(), mStyle.size, mAdjustedSize, (mIsValid ? "yes" : "no"));
    printf("    emHeight: %f emAscent: %f emDescent: %f\n", mMetrics->emHeight, mMetrics->emAscent, mMetrics->emDescent);
    printf("    maxAscent: %f maxDescent: %f maxAdvance: %f\n", mMetrics->maxAscent, mMetrics->maxDescent, mMetrics->maxAdvance);
    printf("    internalLeading: %f externalLeading: %f\n", mMetrics->internalLeading, mMetrics->externalLeading);
    printf("    spaceWidth: %f aveCharWidth: %f\n", mMetrics->spaceWidth, mMetrics->aveCharWidth);
    printf("    xHeight: %f capHeight: %f\n", mMetrics->xHeight, mMetrics->capHeight);
    printf("    uOff: %f uSize: %f stOff: %f stSize: %f\n",
           mMetrics->underlineOffset, mMetrics->underlineSize, mMetrics->strikeoutOffset, mMetrics->strikeoutSize);
#endif
}

void gfxGDIFont::FillLogFont(LOGFONTW& aLogFont, gfxFloat aSize) {
  GDIFontEntry* fe = static_cast<GDIFontEntry*>(GetFontEntry());

  // Figure out the lfWeight value to use for GDI font selection,
  // or zero to use the entry's current LOGFONT value.
  LONG weight;
  if (fe->IsUserFont()) {
    if (fe->IsLocalUserFont()) {
      // for local user fonts, don't change the original weight
      // in the entry's logfont, because that could alter the
      // choice of actual face used (bug 724231)
      weight = 0;
    } else {
      // avoid GDI synthetic bold which occurs when weight
      // specified is >= font data weight + 200
      weight = mNeedsSyntheticBold ? 700 : 200;
    }
  } else {
    // GDI doesn't support variation fonts, so for system fonts we know
    // that the entry has only a single weight, not a range.
    MOZ_ASSERT(fe->Weight().IsSingle());
    weight = mNeedsSyntheticBold ? 700 : fe->Weight().Min().ToIntRounded();
  }

  fe->FillLogFont(&aLogFont, weight, aSize);
}

uint32_t gfxGDIFont::GetGlyph(uint32_t aUnicode, uint32_t aVarSelector) {
  // Callback used only for fonts that lack a 'cmap' table.

  // We don't support variation selector sequences or non-BMP characters
  // in the legacy bitmap, vector or postscript fonts that might use
  // this code path.
  if (aUnicode > 0xffff || aVarSelector) {
    return 0;
  }

  if (!mGlyphIDs) {
    mGlyphIDs = MakeUnique<nsTHashMap<nsUint32HashKey, uint32_t>>(64);
  }

  uint32_t gid;
  if (mGlyphIDs->Get(aUnicode, &gid)) {
    return gid;
  }

  wchar_t ch = aUnicode;
  WORD glyph;
  DWORD ret = ScriptGetCMap(nullptr, &mScriptCache, &ch, 1, 0, &glyph);
  if (ret != S_OK) {
    AutoDC dc;
    AutoSelectFont fs(dc.GetDC(), GetHFONT());
    if (ret == E_PENDING) {
      // Try ScriptGetCMap again now that we've set up the font.
      ret = ScriptGetCMap(dc.GetDC(), &mScriptCache, &ch, 1, 0, &glyph);
    }
    if (ret != S_OK) {
      // If ScriptGetCMap still failed, fall back to GetGlyphIndicesW
      // (see bug 1105807).
      ret = GetGlyphIndicesW(dc.GetDC(), &ch, 1, &glyph,
                             GGI_MARK_NONEXISTING_GLYPHS);
      if (ret == GDI_ERROR || glyph == 0xFFFF) {
        glyph = 0;
      }
    }
  }

  mGlyphIDs->InsertOrUpdate(aUnicode, glyph);
  return glyph;
}

int32_t gfxGDIFont::GetGlyphWidth(uint16_t aGID) {
  if (!mGlyphWidths) {
    mGlyphWidths = MakeUnique<nsTHashMap<nsUint32HashKey, int32_t>>(128);
  }

  return mGlyphWidths->WithEntryHandle(aGID, [&](auto&& entry) {
    if (!entry) {
      DCForMetrics dc;
      AutoSelectFont fs(dc, GetHFONT());

      int devWidth;
      if (!GetCharWidthI(dc, aGID, 1, nullptr, &devWidth)) {
        return -1;
      }
      // clamp value to range [0..0x7fff], and convert to 16.16 fixed-point
      devWidth = std::min(std::max(0, devWidth), 0x7fff);
      entry.Insert(devWidth << 16);
    }
    return *entry;
  });
}

bool gfxGDIFont::GetGlyphBounds(uint16_t aGID, gfxRect* aBounds, bool aTight) {
  DCForMetrics dc;
  AutoSelectFont fs(dc, GetHFONT());

  if (mIsBitmap) {
    int devWidth;
    if (!GetCharWidthI(dc, aGID, 1, nullptr, &devWidth)) {
      return false;
    }
    devWidth = std::min(std::max(0, devWidth), 0x7fff);

    *aBounds = gfxRect(0, -mMetrics->maxAscent, devWidth,
                       mMetrics->maxAscent + mMetrics->maxDescent);
    return true;
  }

  const MAT2 kIdentityMatrix = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
  GLYPHMETRICS gm;
  if (GetGlyphOutlineW(dc, aGID, GGO_METRICS | GGO_GLYPH_INDEX, &gm, 0, nullptr,
                       &kIdentityMatrix) == GDI_ERROR) {
    return false;
  }

  if (gm.gmBlackBoxX == 1 && gm.gmBlackBoxY == 1 &&
      !GetGlyphOutlineW(dc, aGID, GGO_NATIVE | GGO_GLYPH_INDEX, &gm, 0, nullptr,
                        &kIdentityMatrix)) {
    // Workaround for GetGlyphOutline returning 1x1 bounding box
    // for <space> glyph that is in fact empty.
    gm.gmBlackBoxX = 0;
    gm.gmBlackBoxY = 0;
  } else if (gm.gmBlackBoxX > 0 && !aTight) {
    // The bounding box reported by Windows supposedly contains the glyph's
    // "black" area; however, antialiasing (especially with ClearType) means
    // that the actual image that needs to be rendered may "bleed" into the
    // adjacent pixels, mainly on the right side.
    gm.gmptGlyphOrigin.x -= 1;
    gm.gmBlackBoxX += 3;
  }

  *aBounds = gfxRect(gm.gmptGlyphOrigin.x, -gm.gmptGlyphOrigin.y,
                     gm.gmBlackBoxX, gm.gmBlackBoxY);
  return true;
}

void gfxGDIFont::AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const {
  gfxFont::AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
  aSizes->mFontInstances += aMallocSizeOf(mMetrics);
  if (mGlyphWidths) {
    aSizes->mFontInstances +=
        mGlyphWidths->ShallowSizeOfIncludingThis(aMallocSizeOf);
  }
}

void gfxGDIFont::AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                                        FontCacheSizes* aSizes) const {
  aSizes->mFontInstances += aMallocSizeOf(this);
  AddSizeOfExcludingThis(aMallocSizeOf, aSizes);
}
