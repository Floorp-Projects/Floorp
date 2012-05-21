/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ScaledFontWin.h"
#include "ScaledFontBase.h"

#ifdef USE_SKIA
#include "skia/SkTypeface_win.h"
#endif

namespace mozilla {
namespace gfx {

ScaledFontWin::ScaledFontWin(LOGFONT* aFont, Float aSize)
  : ScaledFontBase(aSize)
  , mLogFont(*aFont)
{
}

#ifdef USE_SKIA
SkTypeface* ScaledFontWin::GetSkTypeface()
{
  if (!mTypeface) {
    mTypeface = SkCreateTypefaceFromLOGFONT(mLogFont);
  }
  return mTypeface;
}
#endif


}
}
