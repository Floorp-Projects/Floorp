/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTDWRITE_H_
#define MOZILLA_GFX_UNSCALEDFONTDWRITE_H_

#include <dwrite.h>

#include "2D.h"

namespace mozilla {
namespace gfx {

class UnscaledFontDWrite final : public UnscaledFont
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontDWrite, override)
  explicit UnscaledFontDWrite(const RefPtr<IDWriteFontFace>& aFontFace,
                              DWRITE_FONT_SIMULATIONS aSimulations =
                                DWRITE_FONT_SIMULATIONS_NONE,
                              bool aNeedsCairo = false)
    : mFontFace(aFontFace)
    , mSimulations(aSimulations)
    , mNeedsCairo(aNeedsCairo)
  {}

  FontType GetType() const override { return FontType::DWRITE; }

  const RefPtr<IDWriteFontFace> GetFontFace() const { return mFontFace; }
  DWRITE_FONT_SIMULATIONS GetSimulations() const { return mSimulations; }

  bool GetFontFileData(FontFileDataOutput aDataCallback, void *aBaton) override;

  already_AddRefed<ScaledFont>
    CreateScaledFont(Float aGlyphSize,
                     const uint8_t* aInstanceData,
                     uint32_t aInstanceDataLength) override;

private:
  RefPtr<IDWriteFontFace> mFontFace;
  DWRITE_FONT_SIMULATIONS mSimulations;
  bool mNeedsCairo;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTDWRITE_H_ */

