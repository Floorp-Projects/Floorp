/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Main header first:
#include "nsSVGMarkerFrame.h"

// Keep others in (case-insensitive) order:
#include "gfxContext.h"
#include "nsSVGEffects.h"
#include "mozilla/dom/SVGMarkerElement.h"
#include "SVGGeometryElement.h"
#include "SVGGeometryFrame.h"

using namespace mozilla::dom;
using namespace mozilla::gfx;
using namespace mozilla::image;

nsContainerFrame*
NS_NewSVGMarkerFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMarkerFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMarkerFrame)

//----------------------------------------------------------------------
// nsIFrame methods:

nsresult
nsSVGMarkerFrame::AttributeChanged(int32_t  aNameSpaceID,
                                   nsIAtom* aAttribute,
                                   int32_t  aModType)
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

  return nsSVGContainerFrame::AttributeChanged(aNameSpaceID,
                                               aAttribute, aModType);
}

#ifdef DEBUG
void
nsSVGMarkerFrame::Init(nsIContent*       aContent,
                       nsContainerFrame* aParent,
                       nsIFrame*         aPrevInFlow)
{
  NS_ASSERTION(aContent->IsSVGElement(nsGkAtoms::marker), "Content is not an SVG marker");

  nsSVGContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGMarkerFrame::GetCanvasTM()
{
  NS_ASSERTION(mMarkedFrame, "null SVGGeometry frame");

  if (mInUse2) {
    // We're going to be bailing drawing the marker, so return an identity.
    return gfxMatrix();
  }

  SVGMarkerElement *content = static_cast<SVGMarkerElement*>(mContent);
  
  mInUse2 = true;
  gfxMatrix markedTM = mMarkedFrame->GetCanvasTM();
  mInUse2 = false;

  Matrix viewBoxTM = content->GetViewBoxTransform();

  return ThebesMatrix(viewBoxTM * mMarkerTM) * markedTM;
}

static nsIFrame*
GetAnonymousChildFrame(nsIFrame* aFrame)
{
  nsIFrame* kid = aFrame->PrincipalChildList().FirstChild();
  MOZ_ASSERT(kid && kid->IsSVGMarkerAnonChildFrame(),
             "expected to find anonymous child of marker frame");
  return kid;
}

void
nsSVGMarkerFrame::PaintMark(gfxContext& aContext,
                            const gfxMatrix& aToMarkedFrameUserSpace,
                            SVGGeometryFrame* aMarkedFrame,
                            const nsSVGMark& aMark, float aStrokeWidth,
                            imgDrawingParams& aImgParams)
{
  // If the flag is set when we get here, it means this marker frame
  // has already been used painting the current mark, and the document
  // has a marker reference loop.
  if (mInUse) {
    return;
  }

  AutoMarkerReferencer markerRef(this, aMarkedFrame);

  SVGMarkerElement *marker = static_cast<SVGMarkerElement*>(mContent);
  if (!marker->HasValidDimensions()) {
    return;
  }

  const nsSVGViewBoxRect viewBox = marker->GetViewBoxRect();

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    // We must disable rendering if the viewBox width or height are zero.
    return;
  }

  Matrix viewBoxTM = marker->GetViewBoxTransform();

  mMarkerTM = marker->GetMarkerTransform(aStrokeWidth, aMark);

  gfxMatrix markTM = ThebesMatrix(viewBoxTM) * ThebesMatrix(mMarkerTM) *
                     aToMarkedFrameUserSpace;

  if (StyleDisplay()->IsScrollableOverflow()) {
    aContext.Save();
    gfxRect clipRect =
      nsSVGUtils::GetClipRectForFrame(this, viewBox.x, viewBox.y,
                                      viewBox.width, viewBox.height);
    nsSVGUtils::SetClipRect(&aContext, markTM, clipRect);
  }


  nsIFrame* kid = GetAnonymousChildFrame(this);
  nsSVGDisplayableFrame* SVGFrame = do_QueryFrame(kid);
  // The CTM of each frame referencing us may be different.
  SVGFrame->NotifySVGChanged(nsSVGDisplayableFrame::TRANSFORM_CHANGED);
  nsSVGUtils::PaintFrameWithEffects(kid, aContext, markTM, aImgParams);

  if (StyleDisplay()->IsScrollableOverflow())
    aContext.Restore();
}

SVGBBox
nsSVGMarkerFrame::GetMarkBBoxContribution(const Matrix& aToBBoxUserspace,
                                          uint32_t aFlags,
                                          SVGGeometryFrame* aMarkedFrame,
                                          const nsSVGMark& aMark,
                                          float aStrokeWidth)
{
  SVGBBox bbox;

  // If the flag is set when we get here, it means this marker frame
  // has already been used in calculating the current mark bbox, and
  // the document has a marker reference loop.
  if (mInUse)
    return bbox;

  AutoMarkerReferencer markerRef(this, aMarkedFrame);

  SVGMarkerElement *content = static_cast<SVGMarkerElement*>(mContent);
  if (!content->HasValidDimensions()) {
    return bbox;
  }

  const nsSVGViewBoxRect viewBox = content->GetViewBoxRect();

  if (viewBox.width <= 0.0f || viewBox.height <= 0.0f) {
    return bbox;
  }

  mMarkerTM = content->GetMarkerTransform(aStrokeWidth, aMark);
  Matrix viewBoxTM = content->GetViewBoxTransform();

  Matrix tm = viewBoxTM * mMarkerTM * aToBBoxUserspace;

  nsSVGDisplayableFrame* child = do_QueryFrame(GetAnonymousChildFrame(this));
  // When we're being called to obtain the invalidation area, we need to
  // pass down all the flags so that stroke is included. However, once DOM
  // getBBox() accepts flags, maybe we should strip some of those here?

  // We need to include zero width/height vertical/horizontal lines, so we have
  // to use UnionEdges.
  bbox.UnionEdges(child->GetBBoxContribution(tm, aFlags));

  return bbox;
}

void
nsSVGMarkerFrame::SetParentCoordCtxProvider(SVGSVGElement *aContext)
{
  SVGMarkerElement *marker = static_cast<SVGMarkerElement*>(mContent);
  marker->SetParentCoordCtxProvider(aContext);
}

void
nsSVGMarkerFrame::AppendDirectlyOwnedAnonBoxes(nsTArray<OwnedAnonBox>& aResult)
{
  aResult.AppendElement(OwnedAnonBox(GetAnonymousChildFrame(this)));
}

//----------------------------------------------------------------------
// helper class

nsSVGMarkerFrame::AutoMarkerReferencer::AutoMarkerReferencer(
    nsSVGMarkerFrame *aFrame,
    SVGGeometryFrame *aMarkedFrame
    MOZ_GUARD_OBJECT_NOTIFIER_PARAM_IN_IMPL)
      : mFrame(aFrame)
{
  MOZ_GUARD_OBJECT_NOTIFIER_INIT;
  mFrame->mInUse = true;
  mFrame->mMarkedFrame = aMarkedFrame;

  SVGSVGElement *ctx =
    static_cast<nsSVGElement*>(aMarkedFrame->GetContent())->GetCtx();
  mFrame->SetParentCoordCtxProvider(ctx);
}

nsSVGMarkerFrame::AutoMarkerReferencer::~AutoMarkerReferencer()
{
  mFrame->SetParentCoordCtxProvider(nullptr);

  mFrame->mMarkedFrame = nullptr;
  mFrame->mInUse = false;
}

//----------------------------------------------------------------------
// Implementation of nsSVGMarkerAnonChildFrame

nsContainerFrame*
NS_NewSVGMarkerAnonChildFrame(nsIPresShell* aPresShell,
                              nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGMarkerAnonChildFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGMarkerAnonChildFrame)

#ifdef DEBUG
void
nsSVGMarkerAnonChildFrame::Init(nsIContent*       aContent,
                                nsContainerFrame* aParent,
                                nsIFrame*         aPrevInFlow)
{
  MOZ_ASSERT(aParent->IsSVGMarkerFrame(), "Unexpected parent");
  nsSVGDisplayContainerFrame::Init(aContent, aParent, aPrevInFlow);
}
#endif
