/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_UNSCALEDFONTGDI_H_
#define MOZILLA_GFX_UNSCALEDFONTGDI_H_

#include "2D.h"
#include <windows.h>

namespace mozilla {
namespace gfx {

class UnscaledFontGDI final : public UnscaledFont
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(UnscaledFontGDI, override)
  explicit UnscaledFontGDI(const LOGFONT& aLogFont)
    : mLogFont(aLogFont)
  {}

  FontType GetType() const override { return FontType::GDI; }

  const LOGFONT& GetLogFont() const { return mLogFont; }

  bool GetFontFileData(FontFileDataOutput aDataCallback, void* aBaton) override;

  bool GetFontDescriptor(FontDescriptorOutput aCb, void* aBaton) override;

  static already_AddRefed<UnscaledFont>
    CreateFromFontDescriptor(const uint8_t* aData, uint32_t aDataLength);

  already_AddRefed<ScaledFont>
    CreateScaledFont(Float aGlyphSize,
                     const uint8_t* aInstanceData,
                     uint32_t aInstanceDataLength) override;

private:
  LOGFONT mLogFont;
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_UNSCALEDFONTGDI_H_ */

