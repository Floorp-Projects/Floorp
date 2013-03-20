/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGeometryFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "gfxSVGGlyphs.h"
#include "nsPresContext.h"
#include "nsSVGEffects.h"
#include "nsSVGPaintServerFrame.h"
#include "nsSVGUtils.h"

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGeometryFrame)

//----------------------------------------------------------------------
// nsIFrame methods

void
nsSVGGeometryFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  AddStateBits(aParent->GetStateBits() &
               (NS_STATE_SVG_NONDISPLAY_CHILD | NS_STATE_SVG_CLIPPATH_CHILD));
  nsSVGGeometryFrameBase::Init(aContent, aParent, aPrevInFlow);
}

//----------------------------------------------------------------------

uint16_t
nsSVGGeometryFrame::GetClipRule()
{
  return StyleSVG()->mClipRule;
}

uint16_t
nsSVGGeometryFrame::GetHitTestFlags()
{
  return nsSVGUtils::GetGeometryHitTestFlags(this);
}
