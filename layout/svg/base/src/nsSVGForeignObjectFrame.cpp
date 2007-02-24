/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SVG project.
 *
 * The Initial Developer of the Original Code is
 * Crocodile Clips Ltd..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsSVGForeignObjectFrame.h"

#include "nsISVGValue.h"
#include "nsIDOMSVGGElement.h"
#include "nsIDOMSVGForeignObjectElem.h"
#include "nsIDOMSVGMatrix.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIDOMSVGPoint.h"
#include "nsSpaceManager.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsISVGValueUtils.h"
#include "nsRegion.h"
#include "nsGkAtoms.h"
#include "nsLayoutUtils.h"
#include "nsSVGUtils.h"
#include "nsIURI.h"
#include "nsSVGPoint.h"
#include "nsSVGRect.h"
#include "nsSVGMatrix.h"
#include "nsINameSpaceManager.h"
#include "nsSVGForeignObjectElement.h"
#include "nsSVGContainerFrame.h"
#include "gfxContext.h"
#include "gfxMatrix.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGForeignObjectFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{
  nsCOMPtr<nsIDOMSVGForeignObjectElement> foreignObject = do_QueryInterface(aContent);
  if (!foreignObject) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGForeignObjectFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGForeignObjectFrame(aContext);
}

nsSVGForeignObjectFrame::nsSVGForeignObjectFrame(nsStyleContext* aContext)
  : nsSVGForeignObjectFrameBase(aContext),
    mPropagateTransform(PR_TRUE), mInReflow(PR_FALSE)
{
  AddStateBits(NS_FRAME_REFLOW_ROOT);
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGForeignObjectFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGForeignObjectFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

void nsSVGForeignObjectFrame::Destroy()
{
  nsSVGUtils::StyleEffects(this);
  nsSVGForeignObjectFrameBase::Destroy();
}

nsIAtom *
nsSVGForeignObjectFrame::GetType() const
{
  return nsGkAtoms::svgForeignObjectFrame;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                          nsIAtom*        aAttribute,
                                          PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::width ||
        aAttribute == nsGkAtoms::height) {
      PostChildDirty();
      UpdateGraphic();
    } else if (aAttribute == nsGkAtoms::x ||
               aAttribute == nsGkAtoms::y) {
      UpdateGraphic();
    } else if (aAttribute == nsGkAtoms::transform) {
      // make sure our cached transform matrix gets (lazily) updated
      mCanvasTM = nsnull;
      UpdateGraphic();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::DidSetStyleContext()
{
  nsSVGUtils::StyleEffects(this);
  return NS_OK;
}

/* virtual */ void
nsSVGForeignObjectFrame::MarkIntrinsicWidthsDirty()
{
  if (GetStateBits() & NS_FRAME_FIRST_REFLOW)
    // If we haven't had an InitialUpdate yet, nothing to do.
    return;

  // Use the fact that we get a MarkIntrinsicWidthsDirty whenever
  // there's a style change that requires reflow to actually cause that
  // reflow, since the SVG outer frame doesn't know to reflow us.
  nsIFrame* kid = GetFirstChild(nsnull);
  if (!kid)
    return;
  // Since we don't know whether this is because of a style change on an
  // ancestor or descendant, mark the kid dirty.  If it's a descendant,
  // all we need is the NS_FRAME_IS_DIRTY_CHILDREN that our caller is
  // going to set, though.
  kid->AddStateBits(NS_FRAME_IS_DIRTY);
  // This is really a style change, except we're already being called
  // from MarkIntrinsicWidthsDirty, so say it's a resize to avoid doing
  // the same work over again.
  GetPresContext()->PresShell()->FrameNeedsReflow(kid,
                                                  nsIPresShell::eResize);
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::Reflow(nsPresContext*           aPresContext,
                                nsHTMLReflowMetrics&     aDesiredSize,
                                const nsHTMLReflowState& aReflowState,
                                nsReflowStatus&          aStatus)
{
  NS_ASSERTION(!aReflowState.parentReflowState,
               "should only get reflow from being reflow root");
  NS_ASSERTION(aReflowState.ComputedWidth() == GetSize().width &&
               aReflowState.mComputedHeight == GetSize().height,
               "reflow roots should be reflown at existing size and "
               "svg.css should ensure we have no padding/border/margin");

  DoReflow();

  aDesiredSize.width = aReflowState.ComputedWidth();
  aDesiredSize.height = aReflowState.mComputedHeight;
  aDesiredSize.mOverflowArea =
    nsRect(nsPoint(0, 0), nsSize(aDesiredSize.width, aDesiredSize.height));
  aStatus = NS_FRAME_COMPLETE;

  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

/**
 * Transform a rectangle with a given matrix. Since the image of the
 * rectangle may not be a rectangle, the output rectangle is the
 * bounding box of the true image.
 */
static void
TransformRect(float* aX, float *aY, float* aWidth, float *aHeight,
              nsIDOMSVGMatrix* aMatrix)
{
  float x[4], y[4];
  x[0] = *aX;
  y[0] = *aY;
  x[1] = x[0] + *aWidth;
  y[1] = y[0];
  x[2] = x[0] + *aWidth;
  y[2] = y[0] + *aHeight;
  x[3] = x[0];
  y[3] = y[0] + *aHeight;
 
  int i;
  for (i = 0; i < 4; i++) {
    nsSVGUtils::TransformPoint(aMatrix, &x[i], &y[i]);
  }

  float xmin, xmax, ymin, ymax;
  xmin = xmax = x[0];
  ymin = ymax = y[0];
  for (i=1; i<4; i++) {
    if (x[i] < xmin)
      xmin = x[i];
    if (y[i] < ymin)
      ymin = y[i];
    if (x[i] > xmax)
      xmax = x[i];
    if (y[i] > ymax)
      ymax = y[i];
  }
 
  *aX = xmin;
  *aY = ymin;
  *aWidth = xmax - xmin;
  *aHeight = ymax - ymin;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::PaintSVG(nsSVGRenderState *aContext,
                                  nsRect *aDirtyRect)
{
  nsIFrame* kid = GetFirstChild(nsnull);
  if (!kid)
    return NS_OK;

  nsCOMPtr<nsIDOMSVGMatrix> tm = GetTMIncludingOffset();

  cairo_matrix_t matrix = nsSVGUtils::ConvertSVGMatrixToCairo(tm);

  nsIRenderingContext *ctx = aContext->GetRenderingContext();

  if (!ctx || nsSVGUtils::IsSingular(&matrix)) {
    NS_WARNING("Can't render foreignObject element!");
    return NS_ERROR_FAILURE;
  }

  gfxContext *gfx = aContext->GetGfxContext();

  gfx->Save();
  gfx->Multiply(gfxMatrix(*reinterpret_cast<gfxMatrix*>(&matrix)));

  nsresult rv = nsLayoutUtils::PaintFrame(ctx, kid, nsRegion(kid->GetRect()),
                                          NS_RGBA(0,0,0,0));

  gfx->Restore();

  return rv;
}

nsresult
nsSVGForeignObjectFrame::TransformPointFromOuterPx(float aX, float aY, nsPoint* aOut)
{
  if (mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMSVGMatrix> tm = GetTMIncludingOffset();
  nsCOMPtr<nsIDOMSVGMatrix> inverse;
  nsresult rv = tm->Inverse(getter_AddRefs(inverse));
  if (NS_FAILED(rv))
    return rv;
   
  nsSVGUtils::TransformPoint(inverse, &aX, &aY);
  *aOut = nsPoint(nsPresContext::CSSPixelsToAppUnits(aX),
                  nsPresContext::CSSPixelsToAppUnits(aY));
  return NS_OK;
}
 
NS_IMETHODIMP
nsSVGForeignObjectFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
  nsIFrame* kid = GetFirstChild(nsnull);
  if (!kid) {
    *hit = nsnull;
    return NS_OK;
  }
  nsPoint pt;
  nsresult rv = TransformPointFromOuterPx(x, y, &pt);
  if (NS_FAILED(rv))
    return rv;
  *hit = nsLayoutUtils::GetFrameForPoint(kid, pt);
  return NS_OK;
}

nsPoint
nsSVGForeignObjectFrame::TransformPointFromOuter(nsPoint aPt)
{
  nsPoint pt(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  TransformPointFromOuterPx(nsPresContext::AppUnitsToFloatCSSPixels(aPt.x),
                            nsPresContext::AppUnitsToFloatCSSPixels(aPt.y),
                            &pt);
  return pt;
}

NS_IMETHODIMP_(nsRect)
nsSVGForeignObjectFrame::GetCoveredRegion()
{
  return mRect;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::UpdateCoveredRegion()
{
  if (mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return NS_ERROR_FAILURE;

  float x, y, w, h;
  GetBBoxInternal(&x, &y, &w, &h);

  mRect = nsSVGUtils::ToBoundingPixelRect(x, y, x + w, y + h);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::InitialUpdate()
{
  UpdateCoveredRegion();
  DoReflow();

  NS_ASSERTION(!(mState & NS_FRAME_IN_REFLOW),
               "We don't actually participate in reflow");
  
  // Do unset the various reflow bits, though.
  mState &= ~(NS_FRAME_FIRST_REFLOW | NS_FRAME_IS_DIRTY |
              NS_FRAME_HAS_DIRTY_CHILDREN);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  mCanvasTM = nsnull;
  UpdateGraphic();
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawSuspended()
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::NotifyRedrawUnsuspended()
{
  if (!(mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)) {
    if (GetStateBits() & NS_STATE_SVG_DIRTY) {
      UpdateGraphic();
    } else {
      FlushDirtyRegion();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGForeignObjectFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

void
nsSVGForeignObjectFrame::GetBBoxInternal(float* aX, float *aY, float* aWidth,
                                         float *aHeight)
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm = GetCanvasTM();
  if (!ctm)
    return;
  
  nsSVGForeignObjectElement *fO = NS_STATIC_CAST(nsSVGForeignObjectElement*,
                                                 mContent);
  fO->GetAnimatedLengthValues(aX, aY, aWidth, aHeight, nsnull);
  
  TransformRect(aX, aY, aWidth, aHeight, ctm);
}
  
NS_IMETHODIMP
nsSVGForeignObjectFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  if (mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD) {
    *_retval = nsnull;
    return NS_ERROR_FAILURE;
  }

  float x, y, w, h;
  GetBBoxInternal(&x, &y, &w, &h);
  return NS_NewSVGRect(_retval, x, y, w, h);
}

//----------------------------------------------------------------------

already_AddRefed<nsIDOMSVGMatrix>
nsSVGForeignObjectFrame::GetTMIncludingOffset()
{
  nsCOMPtr<nsIDOMSVGMatrix> ctm = GetCanvasTM();
  if (!ctm)
    return nsnull;

  nsSVGForeignObjectElement *fO =
    NS_STATIC_CAST(nsSVGForeignObjectElement*, mContent);
  float x, y;
  fO->GetAnimatedLengthValues(&x, &y, nsnull);
  nsIDOMSVGMatrix* matrix;
  ctm->Translate(x, y, &matrix);
  return matrix;
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGForeignObjectFrame::GetCanvasTM()
{
  if (!mPropagateTransform) {
    nsIDOMSVGMatrix *retval;
    if (mOverrideCTM) {
      retval = mOverrideCTM;
      NS_ADDREF(retval);
    } else {
      NS_NewSVGMatrix(&retval);
    }
    return retval;
  }

  if (!mCanvasTM) {
    // get our parent's tm and append local transforms (if any):
    NS_ASSERTION(mParent, "null parent");
    nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                         mParent);
    nsCOMPtr<nsIDOMSVGMatrix> parentTM = containerFrame->GetCanvasTM();
    NS_ASSERTION(parentTM, "null TM");

    // got the parent tm, now check for local tm:
    nsSVGGraphicElement *element =
      NS_STATIC_CAST(nsSVGGraphicElement*, mContent);
    nsCOMPtr<nsIDOMSVGMatrix> localTM = element->GetLocalTransformMatrix();
    
    if (localTM)
      parentTM->Multiply(localTM, getter_AddRefs(mCanvasTM));
    else
      mCanvasTM = parentTM;
  }

  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

//----------------------------------------------------------------------
// Implementation helpers

void nsSVGForeignObjectFrame::PostChildDirty()
{
  nsIFrame* kid = GetFirstChild(nsnull);
  if (!kid)
    return;
  GetPresContext()->PresShell()->
    FrameNeedsReflow(kid, nsIPresShell::eStyleChange);
}

void nsSVGForeignObjectFrame::UpdateGraphic()
{
  if (GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }
  
  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (suspended) {
    AddStateBits(NS_STATE_SVG_DIRTY);
  } else {
    RemoveStateBits(NS_STATE_SVG_DIRTY);

    outerSVGFrame->InvalidateRect(mRect);

    UpdateCoveredRegion();

    nsRect filterRect;
    filterRect = nsSVGUtils::FindFilterInvalidation(this);
    if (!filterRect.IsEmpty()) {
      outerSVGFrame->InvalidateRect(filterRect);
    } else {
      outerSVGFrame->InvalidateRect(mRect);
    }
  }

  // Clear any layout dirty region since we invalidated our whole area.
  mDirtyRegion.SetEmpty();
}

void
nsSVGForeignObjectFrame::DoReflow()
{
#ifdef DEBUG
  printf("**nsSVGForeignObjectFrame::DoReflow()\n");
#endif

  if (mParent->GetStateBits() & NS_STATE_SVG_NONDISPLAY_CHILD)
    return;

  nsPresContext *presContext = GetPresContext();
  nsIFrame* kid = GetFirstChild(nsnull);
  if (!kid)
    return;

  // initiate a synchronous reflow here and now:  
  nsSize availableSpace(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
  nsCOMPtr<nsIRenderingContext> renderingContext;
  nsIPresShell* presShell = presContext->PresShell();
  NS_ASSERTION(presShell, "null presShell");
  presShell->CreateRenderingContext(this,getter_AddRefs(renderingContext));
  if (!renderingContext)
    return;

  nsSVGForeignObjectElement *fO = NS_STATIC_CAST(nsSVGForeignObjectElement*,
                                                 mContent);

  float width =
    fO->mLengthAttributes[nsSVGForeignObjectElement::WIDTH].GetAnimValue(fO);
  float height =
    fO->mLengthAttributes[nsSVGForeignObjectElement::HEIGHT].GetAnimValue(fO);

  nsSize size(nsPresContext::CSSPixelsToAppUnits(width),
              nsPresContext::CSSPixelsToAppUnits(height));

  mInReflow = PR_TRUE;

  nsHTMLReflowState reflowState(presContext, kid,
                                renderingContext,
                                nsSize(size.width, NS_UNCONSTRAINEDSIZE));
  nsHTMLReflowMetrics desiredSize;
  nsReflowStatus status;

  // We don't use size.height above because that tells the child to do
  // page/column breaking at that height.
  NS_ASSERTION(reflowState.mComputedBorderPadding == nsMargin(0, 0, 0, 0) &&
               reflowState.mComputedMargin == nsMargin(0, 0, 0, 0),
               "style system should ensure that :-moz-svg-foreign content "
               "does not get styled");
  NS_ASSERTION(reflowState.ComputedWidth() == size.width,
               "reflow state made child wrong size");
  reflowState.mComputedHeight = size.height;
  
  ReflowChild(kid, presContext, desiredSize, reflowState, 0, 0,
              NS_FRAME_NO_MOVE_FRAME, status);
  NS_ASSERTION(size.width == desiredSize.width &&
               size.height == desiredSize.height, "unexpected size");
  FinishReflowChild(kid, presContext, &reflowState, desiredSize, 0, 0,
                    NS_FRAME_NO_MOVE_FRAME);
  
  mInReflow = PR_FALSE;
  FlushDirtyRegion();
}

void
nsSVGForeignObjectFrame::FlushDirtyRegion() {
  if (mDirtyRegion.IsEmpty() || mInReflow)
    return;

  nsSVGOuterSVGFrame *outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (!outerSVGFrame) {
    NS_ERROR("null outerSVGFrame");
    return;
  }

  PRBool suspended;
  outerSVGFrame->IsRedrawSuspended(&suspended);
  if (suspended)
    return;

  nsRect rect = nsSVGUtils::FindFilterInvalidation(this);
  if (!rect.IsEmpty()) {
    outerSVGFrame->InvalidateRect(rect);
    mDirtyRegion.SetEmpty();
    return;
  }
  
  nsCOMPtr<nsIDOMSVGMatrix> tm = GetTMIncludingOffset();
  nsRect r = mDirtyRegion.GetBounds();
  r.ScaleRoundOut(1.0f / nsPresContext::AppUnitsPerCSSPixel());
  float x = r.x, y = r.y, w = r.width, h = r.height;
  TransformRect(&x, &y, &w, &h, tm);
  r = nsSVGUtils::ToBoundingPixelRect(x, y, x+w, y+h);
  outerSVGFrame->InvalidateRect(r);

  mDirtyRegion.SetEmpty();
}

void
nsSVGForeignObjectFrame::InvalidateInternal(const nsRect& aDamageRect,
                                            nscoord aX, nscoord aY, nsIFrame* aForChild,
                                            PRBool aImmediate)
{
  mDirtyRegion.Or(mDirtyRegion, aDamageRect + nsPoint(aX, aY));
  FlushDirtyRegion();
}
