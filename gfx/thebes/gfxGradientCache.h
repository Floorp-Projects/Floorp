
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GRADIENT_CACHE_H
#define GFX_GRADIENT_CACHE_H

#include "nsTArray.h"
#include "gfxPattern.h"
#include "mozilla/gfx/2D.h"

namespace mozilla {
namespace gfx {

class gfxGradientCache {
public:
  static gfx::GradientStops *GetGradientStops(gfx::DrawTarget *aDT,
					      nsTArray<gfx::GradientStop>& aStops,
					      gfx::ExtendMode aExtend);
  static gfx::GradientStops *GetOrCreateGradientStops(gfx::DrawTarget *aDT,
						      nsTArray<gfx::GradientStop>& aStops,
						      gfx::ExtendMode aExtend);

  static void Shutdown();
};

}
}

#endif
