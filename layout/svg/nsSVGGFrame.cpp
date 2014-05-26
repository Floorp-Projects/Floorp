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
void
nsSVGGFrame::Init(nsIContent*       aContent,
                  nsContainerFrame* aParent,
                  nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVG() &&
               static_cast<nsSVGElement*>(aContent)->IsTransformable(),
               "The element doesn't support nsIDOMSVGTransformable");

  nsSVGGFrameBase::Init(aContent, aParent, aPrevInFlow);
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
nsSVGGFrame::GetCanvasTM(uint32_t aFor, nsIFrame* aTransformRoot)
{
  if (!(GetStateBits() & NS_FRAME_IS_NONDISPLAY) && !aTransformRoot) {
    if ((aFor == FOR_PAINTING && NS_SVGDisplayListPaintingEnabled()) ||
        (aFor == FOR_HIT_TESTING && NS_SVGDisplayListHitTestingEnabled())) {
      return nsSVGIntegrationUtils::GetCSSPxToDevPxMatrix(this);
    }
  }
  if (!mCanvasTM) {
    NS_ASSERTION(GetParent(), "null parent");

    nsSVGContainerFrame *parent = static_cast<nsSVGContainerFrame*>(GetParent());
    SVGGraphicsElement *content = static_cast<SVGGraphicsElement*>(mContent);
    gfxMatrix tm = content->PrependLocalTransformsTo(
        this == aTransformRoot ? gfxMatrix() :
                                 parent->GetCanvasTM(aFor, aTransformRoot));

    mCanvasTM = new gfxMatrix(tm);
  }
  return *mCanvasTM;
}

nsresult
nsSVGGFrame::AttributeChanged(int32_t         aNameSpaceID,
                              nsIAtom*        aAttribute,
                              int32_t         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      aAttribute == nsGkAtoms::transform) {
    // We don't invalidate for transform changes (the layers code does that).
    // Also note that SVGTransformableElement::GetAttributeChangeHint will
    // return nsChangeHint_UpdateOverflow for "transform" attribute changes
    // and cause DoApplyRenderingChangeToTree to make the SchedulePaint call.
    NotifySVGChanged(TRANSFORM_CHANGED);
  }
  
  return NS_OK;
}
