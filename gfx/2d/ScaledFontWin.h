/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTWIN_H_
#define MOZILLA_GFX_SCALEDFONTWIN_H_

#include "ScaledFontBase.h"
#include <windows.h>

namespace mozilla {
namespace gfx {

class ScaledFontWin : public ScaledFontBase {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontWin, override)
  ScaledFontWin(const LOGFONT* aFont, const RefPtr<UnscaledFont>& aUnscaledFont,
                Float aSize);

  FontType GetType() const override { return FontType::GDI; }

  bool GetFontInstanceData(FontInstanceDataOutput aCb, void* aBaton) override;

  AntialiasMode GetDefaultAAMode() override;

  SkTypeface* CreateSkTypeface() override;

  bool MayUseBitmaps() override { return true; }

  bool UseSubpixelPosition() const override { return false; }

 protected:
  cairo_font_face_t* CreateCairoFontFace(
      cairo_font_options_t* aFontOptions) override;

 private:
  friend class DrawTargetSkia;
  LOGFONT mLogFont;
};

}  // namespace gfx
}  // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTWIN_H_ */
