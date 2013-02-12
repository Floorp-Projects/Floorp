/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGGFrame.h"

// Keep others in (case-insensitive) order:
#include "nsGkAtoms.h"
#include "SVGTransformableElement.h"
#include "nsIFrame.h"
#include "SVGGraphicsElement.h"
#include "nsSVGIntegrationUtils.h"
#include "nsSVGUtils.h"

using namespace mozilla::dom;

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGGFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{  
  return new (aPresShell) nsSVGGFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGGFrame)

#ifdef DEBUG
NS_IMETHODIMP
nsSVGGFrame::Init(nsIContent* aContent,
                  nsIFrame* aParent,
                  nsIFrame* aPrevInFlow)
{
  nsCOMPtr<SVGTransformableElement> transformable = do_QueryInterface(aContent);
  NS_ASSERTION(transformable,
               "The element doesn't support nsIDOMSVGTransformable\n");

  return nsSVGGFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGGFrame::GetType() const
{
  return nsGkAtoms::svgGFrame;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

void
nsSVGGFrame::NotifySVGChanged(uint32_t aFlags)
{
  NS_ABORT_IF_FALSE(aFlags & (TRANSFORM_CHANGED | COORD_CONTEXT_CHANGED),
                    "Invalidation logic may need adjusting");

  if (aFlags & TRANSFORM_CHANGED) {
    // make sure our cached transform matrix gets (lazily) updated
    mCanvasTM = nullptr;
  }

  nsSVGGFrameBase::NotifySVGChanged(aFlags);
}

gfxMatrix
nsSVGGFrame::GetCanvasTM(uint32_t aFor)
{
  if (!(GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }
  if (!mCanvasTM) {
    NS_ASSERTION(mParent, "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(mParent);
    SVGGraphicsElement *content = static_cast<SVGGraphicsElement*>(mContent);

    gfxMatrix tm = content->PrependLocalTransformsTo(parent->GetCanvasTM(aFor));

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

NS_IMETHODIMP
nsSVGGFrame::AttributeChanged(int32_t         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::transform) {
    nsSVGUtils::InvalidateBounds(this, false);
    nsSVGUtils::ScheduleReflowSVG(this);
    NotifySVGChanged(TRANSFORM_CHANGED);
  }
  
  return NS_OK;
}
