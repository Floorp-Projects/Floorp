
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
  static void Init();

  static already_AddRefed<gfx::GradientStops> GetOrCreateGradientStops(
      const gfx::DrawTarget* aDT, nsTArray<gfx::GradientStop>& aStops,
      gfx::ExtendMode aExtend);

  static void PurgeAllCaches();
  static void Shutdown();
};

}  // namespace gfx
}  // namespace mozilla

#endif
