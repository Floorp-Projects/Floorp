/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGMarkerFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsRenderingContext.h"
#include "nsSVGEffects.h"
#include "nsSVGMarkerElement.h"
#include "nsSVGPathGeometryElement.h"
#include "nsSVGPathGeometryFrame.h"

nsIFrame*
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMarkerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMarkerFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

NS_IMETHODIMP
nsSVGMarkerFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   PRInt32  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::markerUnits ||
       aAttribute == nsGkAtoms::refX ||
       aAttribute == nsGkAtoms::refY ||
       aAttribute == nsGkAtoms::markerWidth ||
       aAttribute == nsGkAtoms::markerHeight ||
       aAttribute == nsGkAtoms::orient ||
       aAttribute == nsGkAtoms::preserveAspectRatio ||
       aAttribute == nsGkAtoms::viewBox)) {
    nsSVGEffects::InvalidateDirectRenderingObservers(this);
  }

  return nsSVGMarkerFrameBase::AttributeChanged(aNameSpaceID,
                                                aAttribute, aModType);
}

#ifdef DEBUG
NS_IMETHODIMP
nsSVGMarkerFrame::Init(nsIContent* aContent,
                       nsIFrame* aParent,
                       nsIFrame* aPrevInFlow)
{
  nsCOMPtr<nsIDOMSVGMarkerElement> marker = do_QueryInterface(aContent);
  NS_ASSERTION(marker, "Content is not an SVG marker");

  return nsSVGMarkerFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

nsIAtom *
nsSVGMarkerFrame::GetType() const
{
  return nsGkAtoms::svgMarkerFrame;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGMarkerFrame::GetCanvasTM()
{
  NS_ASSERTION(mMarkedFrame, "null nsSVGPathGeometry frame");

  if (mInUse2) {
    // We're going to be bailing drawing the marker, so return an identity.
    return gfxMatrix();
  }

  nsSVGMarkerElement *content = static_cast<nsSVGMarkerElement*>(mContent);
  
  mInUse2 = true;
  gfxMatrix markedTM = mMarkedFrame->GetCanvasTM();
  mInUse2 = false;

  gfxMatrix markerTM = content->GetMarkerTransform(mStrokeWidth, mX, mY, mAutoAngle);
  gfxMatrix viewBoxTM = content->GetViewBoxTransform();

  return viewBoxTM * markerTM * markedTM;
}


nsresult
nsSVGMarkerFrame::PaintMark(nsRenderingContext *aContext,
                            nsSVGPathGeometryFrame *aMarkedFrame,
                            nsSVGMark *aMark, float aStrokeWidth)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used painting the current mark, and the document
  // has a marker reference loop.
  if (mInUse)
    return NS_OK;

  AutoMarkerReferencer markerRef(this, aMarkedFrame);

  nsSVGMarkerElement *marker = static_cast<nsSVGMarkerElement*>(mContent);

  const nsSVGViewBoxRect viewBox = marker->GetViewBoxRect();

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    // We must disable rendering if the viewBox width or height are zero.
    return NS_OK;
  }

  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAutoAngle = aMark->angle;

  gfxContext *gfx = aContext->ThebesContext();

  if (GetStyleDisplay()->IsScrollableOverflow()) {
    gfx->Save();
    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, viewBox.x, viewBox.y,
                                      viewBox.width, viewBox.height);
    nsSVGUtils::SetClipRect(gfx, GetCanvasTM(), clipRect);
  }

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = do_QueryFrame(kid);
    if (SVGFrame) {
      // The CTM of each frame referencing us may be different.
      SVGFrame->NotifySVGChanged(
                          nsISVGChildFrame::DO_NOT_NOTIFY_RENDERING_OBSERVERS |
                          nsISVGChildFrame::TRANSFORM_CHANGED);
      nsSVGUtils::PaintFrameWithEffects(aContext, nsnull, kid);
    }
  }

  if (GetStyleDisplay()->IsScrollableOverflow())
    gfx->Restore();

  return NS_OK;
}

SVGBBox
nsSVGMarkerFrame::GetMarkBBoxContribution(const gfxMatrix &aToBBoxUserspace,
                                          PRUint32 aFlags,
                                          nsSVGPathGeometryFrame *aMarkedFrame,
                                          const nsSVGMark *aMark,
                                          float aStrokeWidth)
{
  SVGBBox bbox;

  // If the flag is set when we get here, it means this marker frame
  // has already been used in calculating the current mark bbox, and
  // the document has a marker reference loop.
  if (mInUse)
    return bbox;

  AutoMarkerReferencer markerRef(this, aMarkedFrame);

  nsSVGMarkerElement *content = static_cast<nsSVGMarkerElement*>(mContent);

  const nsSVGViewBoxRect viewBox = content->GetViewBoxRect();

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    return bbox;
  }

  mStrokeWidth = aStrokeWidth;
  mX = aMark->x;
  mY = aMark->y;
  mAutoAngle = aMark->angle;

  gfxMatrix markerTM =
    content->GetMarkerTransform(mStrokeWidth, mX, mY, mAutoAngle);
  gfxMatrix viewBoxTM = content->GetViewBoxTransform();

  gfxMatrix tm = viewBoxTM * markerTM * aToBBoxUserspace;

  for (nsIFrame* kid = mFrames.FirstChild();
       kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* child = do_QueryFrame(kid);
    if (child) {
      // When we're being called to obtain the invalidation area, we need to
      // pass down all the flags so that stroke is included. However, once DOM
      // getBBox() accepts flags, maybe we should strip some of those here?

      // We need to include zero width/height vertical/horizontal lines, so we have
      // to use UnionEdges.
      bbox.UnionEdges(child->GetBBoxContribution(tm, aFlags));
    }
  }

  return bbox;
}

void
nsSVGMarkerFrame::SetParentCoordCtxProvider(nsSVGSVGElement *aContext)
{
  nsSVGMarkerElement *marker = static_cast<nsSVGMarkerElement*>(mContent);
  marker->SetParentCoordCtxProvider(aContext);
}

//----------------------------------------------------------------------
// helper class

nsSVGMarkerFrame::AutoMarkerReferencer::AutoMarkerReferencer(
    nsSVGMarkerFrame *aFrame,
    nsSVGPathGeometryFrame *aMarkedFrame)
      : mFrame(aFrame)
{
  mFrame->mInUse = true;
  mFrame->mMarkedFrame = aMarkedFrame;

  nsSVGSVGElement *ctx =
    static_cast<nsSVGElement*>(aMarkedFrame->GetContent())->GetCtx();
  mFrame->SetParentCoordCtxProvider(ctx);
}

nsSVGMarkerFrame::AutoMarkerReferencer::~AutoMarkerReferencer()
{
  mFrame->SetParentCoordCtxProvider(nsnull);

  mFrame->mMarkedFrame = nsnull;
  mFrame->mInUse = false;
}
