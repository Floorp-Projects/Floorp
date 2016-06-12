/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_PRINTTARGETCG_H
#define MOZILLA_GFX_PRINTTARGETCG_H

#include <Carbon/Carbon.h>
#include "PrintTarget.h"

namespace mozilla {
namespace gfx {

/**
 * CoreGraphics printing target.
 *
 * Note that a CGContextRef obtained from PMSessionGetCGGraphicsContext is
 * valid only for the current page.  As a consequence instances of this class
 * should only be used to print a single page.
 */
class PrintTargetCG final : public PrintTarget
{
public:
  static already_AddRefed<PrintTargetCG>
  CreateOrNull(const IntSize& aSize, gfxImageFormat aFormat);

  static already_AddRefed<PrintTargetCG>
  CreateOrNull(CGContextRef aContext, const IntSize& aSize);

private:
  PrintTargetCG(cairo_surface_t* aCairoSurface,
                const IntSize& aSize);
};

} // namespace gfx
} // namespace mozilla

#endif /* MOZILLA_GFX_PRINTTARGETCG_H */
