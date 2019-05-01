/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WINDOWSDWRITEFONTS_H
#define GFX_WINDOWSDWRITEFONTS_H

#include "mozilla/MemoryReporting.h"
#include "mozilla/UniquePtr.h"
#include <dwrite_1.h>

#include "gfxFont.h"
#include "gfxUserFontSet.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/gfx/UnscaledFontDWrite.h"

struct _cairo_font_face;
typedef _cairo_font_face cairo_font_face_t;

/**
 * \brief Class representing a font face for a font entry.
 */
class gfxDWriteFont : public gfxFont {
 public:
  gfxDWriteFont(const RefPtr<mozilla::gfx::UnscaledFontDWrite>& aUnscaledFont,
                gfxFontEntry* aFontEntry, const gfxFontStyle* aFontStyle,
                RefPtr<IDWriteFontFace> aFontFace = nullptr,
                AntialiasOption = kAntialiasDefault);
  ~gfxDWriteFont();

  static void UpdateSystemTextQuality();
  static void SystemTextQualityChanged();

  mozilla::UniquePtr<gfxFont> CopyWithAntialiasOption(
      AntialiasOption anAAOption) override;

  uint32_t GetSpaceGlyph() override;

  bool SetupCairoFont(DrawTarget* aDrawTarget) override;

  bool AllowSubpixelAA() override { return mAllowManualShowGlyphs; }

  bool IsValid() const;

  IDWriteFontFace* GetFontFace();

  /* override Measure to add padding for antialiasing */
  RunMetrics Measure(const gfxTextRun* aTextRun, uint32_t aStart, uint32_t aEnd,
                     BoundingBoxType aBoundingBoxType,
                     DrawTarget* aDrawTargetForTightBoundingBox,
                     Spacing* aSpacing,
                     mozilla::gfx::ShapedTextFlags aOrientation) override;

  bool ProvidesGlyphWidths() const override;

  int32_t GetGlyphWidth(uint16_t aGID) override;

  void AddSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const override;
  void AddSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf,
                              FontCacheSizes* aSizes) const override;

  FontType GetType() const override { return FONT_TYPE_DWRITE; }

  already_AddRefed<mozilla::gfx::ScaledFont> GetScaledFont(
      mozilla::gfx::DrawTarget* aTarget) override;

 protected:
  cairo_scaled_font_t* InitCairoScaledFont();

  const Metrics& GetHorizontalMetrics() override;

  bool GetFakeMetricsForArialBlack(DWRITE_FONT_METRICS* aFontMetrics);

  void ComputeMetrics(AntialiasOption anAAOption);

  bool HasBitmapStrikeForSize(uint32_t aSize);

  cairo_font_face_t* CairoFontFace();

  gfxFloat MeasureGlyphWidth(uint16_t aGlyph);

  DWRITE_MEASURING_MODE GetMeasuringMode();
  bool GetForceGDIClassic();

  RefPtr<IDWriteFontFace> mFontFace;
  RefPtr<IDWriteFontFace1> mFontFace1;  // may be unavailable on older DWrite

  cairo_font_face_t* mCairoFontFace;

  Metrics* mMetrics;

  // cache of glyph widths in 16.16 fixed-point pixels
  mozilla::UniquePtr<nsDataHashtable<nsUint32HashKey, int32_t>> mGlyphWidths;

  uint32_t mSpaceGlyph;

  bool mUseSubpixelPositions;
  bool mAllowManualShowGlyphs;

  // Used to record the sUseClearType setting at the time mAzureScaledFont
  // was set up, so we can tell if it's stale and needs to be re-created.
  bool mAzureScaledFontUsedClearType;

  bool UsingClearType() {
    return mozilla::gfx::gfxVars::SystemTextQuality() == CLEARTYPE_QUALITY;
  }
};

#endif
