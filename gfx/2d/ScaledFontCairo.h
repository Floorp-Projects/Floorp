/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_SCALEDFONTCAIRO_H_
#define MOZILLA_GFX_SCALEDFONTCAIRO_H_

#include "ScaledFontBase.h"

#include "cairo.h"

namespace mozilla {
namespace gfx {

class ScaledFontCairo : public ScaledFontBase
{
public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(ScaledFontCairo)

  ScaledFontCairo(cairo_scaled_font_t* aScaledFont, Float aSize);

#if defined(USE_SKIA) && defined(MOZ_ENABLE_FREETYPE)
  virtual SkTypeface* GetSkTypeface();
#endif
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_SCALEDFONTCAIRO_H_ */
