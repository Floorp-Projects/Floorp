/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTWIN_H_
#define MOZILLA_GFX_SCALEDFONTWIN_H_

#include "ScaledFontBase.h"
#include <windows.h>

#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace gfx {

class ScaledFontWin : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontWin)
  ScaledFontWin(LOGFONT* aFont, Float aSize);

  ScaledFontWin(uint8_t* aFontData, uint32_t aFontDataLength, uint32_t aIndex,
                Float aGlyphSize);

  virtual FontType GetType() const { return FontType::GDI; }
#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface();
#endif

protected:
#ifdef USE_CAIRO_SCALED_FONT
  cairo_font_face_t* GetCairoFontFace() override;
#endif

private:
#ifdef USE_SKIA
  friend class DrawTargetSkia;
#endif
  LOGFONT mLogFont;

  struct MemoryFontRemover
  {
    HANDLE memFontHandle;
    MemoryFontRemover(HANDLE aMemFontHandle) : memFontHandle(aMemFontHandle) {}
    ~MemoryFontRemover() { ::RemoveFontMemResourceEx(memFontHandle); }
  };

  UniquePtr<MemoryFontRemover> mMemoryFontRemover;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTWIN_H_ */
