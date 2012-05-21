/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTWIN_H_
#define MOZILLA_GFX_SCALEDFONTWIN_H_

#include "ScaledFontBase.h"
#include <windows.h>

namespace mozilla {
namespace gfx {

class ScaledFontWin : public ScaledFontBase
{
public:
  ScaledFontWin(LOGFONT* aFont, Float aSize);

  virtual FontType GetType() const { return FONT_GDI; }
#ifdef USE_SKIA
  virtual SkTypeface* GetSkTypeface();
#endif
private:
#ifdef USE_SKIA
  friend class DrawTargetSkia;
#endif
  LOGFONT mLogFont;
};

}
}

#endif /* MOZILLA_GFX_SCALEDFONTWIN_H_ */
