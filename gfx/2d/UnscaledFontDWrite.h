/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTDWRITE_H_
#define MOZILLA_GFX_UNSCALEDFONTDWRITE_H_

#include <dwrite.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class ScaledFontDWrite;

class UnscaledFontDWrite final : public UnscaledFont
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontDWrite, override)
  UnscaledFontDWrite(const RefPtr<IDWriteFontFace>& aFontFace,
                     const RefPtr<IDWriteFont>& aFont,
                     DWRITE_FONT_SIMULATIONS aSimulations = DWRITE_FONT_SIMULATIONS_NONE,
                     bool aNeedsCairo = false)
    : mFontFace(aFontFace)
    , mFont(aFont)
    , mSimulations(aSimulations)
    , mNeedsCairo(aNeedsCairo)
  {}

  FontType GetType() const override { return FontType::DWRITE; }

  const RefPtr<IDWriteFontFace>& GetFontFace() const { return mFontFace; }
  const RefPtr<IDWriteFont>& GetFont() const { return mFont; }
  DWRITE_FONT_SIMULATIONS GetSimulations() const { return mSimulations; }

  bool GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton) override;

  already_AddRefed<ScaledFont>
    CreateScaledFont(Float aGlyphSize,
                     const uint8_t* aInstanceData,
                     uint32_t aInstanceDataLength,
                     const FontVariation* aVariations,
                     uint32_t aNumVariations) override;

  already_AddRefed<ScaledFont>
    CreateScaledFontFromWRFont(Float aGlyphSize,
                               const wr::FontInstanceOptions* aOptions,
                               const wr::FontInstancePlatformOptions* aPlatformOptions,
                               const FontVariation* aVariations,
                               uint32_t aNumVariations) override;

  bool GetWRFontDescriptor(WRFontDescriptorOutput aCb, void* aBaton) override;

private:
  RefPtr<IDWriteFontFace> mFontFace;
  RefPtr<IDWriteFont> mFont;
  DWRITE_FONT_SIMULATIONS mSimulations;
  bool mNeedsCairo;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTDWRITE_H_ */

