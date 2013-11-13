/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGPaintServerFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsSVGElement.h"
#include "nsSVGGeometryFrame.h"

NS_IMPL_FRAMEARENA_HELPERS(nsSVGPaintServerFrame)

bool
nsSVGPaintServerFrame::SetupPaintServer(gfxContext *aContext,
                                        nsIFrame *aSource,
                                        nsStyleSVGPaint nsStyleSVG::*aFillOrStroke,
                                        float aOpacity)
{
  nsRefPtr<gfxPattern> pattern =
    GetPaintServerPattern(aSource, aContext->CurrentMatrix(), aFillOrStroke,
                          aOpacity);
  if (!pattern)
    return false;

  if (!aContext->IsCairo()) {
    pattern->CacheColorStops(aContext->GetDrawTarget());
  }

  aContext->SetPattern(pattern);
  return true;
}
