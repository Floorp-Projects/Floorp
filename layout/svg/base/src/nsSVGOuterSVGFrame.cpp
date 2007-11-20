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

#include "nsSVGOuterSVGFrame.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsSVGSVGElement.h"
#include "nsSVGTextFrame.h"
#include "nsSVGForeignObjectFrame.h"
#include "nsSVGRect.h"
#include "nsDisplayList.h"
#include "nsStubMutationObserver.h"
#include "gfxContext.h"
#include "nsPresShellIterator.h"
#include "nsIDOMSVGAnimatedRect.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindowInternal.h"
#include "nsPIDOMWindow.h"
#include "nsIObjectLoadingContent.h"
#include "nsIInterfaceRequestorUtils.h"

#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#endif

class nsSVGMutationObserver : public nsStubMutationObserver
{
public:
  // nsIMutationObserver interface
  NS_DECL_NSIMUTATIONOBSERVER_ATTRIBUTECHANGED

  // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return 1; }
  NS_IMETHOD_(nsrefcnt) Release() { return 1; }

  static void UpdateTextFragmentTrees(nsIFrame *aFrame);
};

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGMutationObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMutationObserver)
NS_INTERFACE_MAP_END

static nsSVGMutationObserver sSVGMutationObserver;

//----------------------------------------------------------------------
// nsIMutationObserver methods

void
nsSVGMutationObserver::AttributeChanged(nsIDocument *aDocument,
                                        nsIContent *aContent,
                                        PRInt32 aNameSpaceID,
                                        nsIAtom *aAttribute,
                                        PRInt32 aModType,
                                        PRUint32 aStateMask)
{
  if (aNameSpaceID != kNameSpaceID_XML || aAttribute != nsGkAtoms::space) {
    return;
  }

  nsPresShellIterator iter(aDocument);
  nsCOMPtr<nsIPresShell> shell;
  while ((shell = iter.GetNextShell())) {
    nsIFrame *frame = shell->GetPrimaryFrameFor(aContent);
    if (!frame) {
      continue;
    }

    // is the content a child of a text element
    nsISVGTextContentMetrics* metrics;
    CallQueryInterface(frame, &metrics);
    if (metrics) {
      nsSVGTextContainerFrame *containerFrame =
        static_cast<nsSVGTextContainerFrame *>(frame);
      containerFrame->UpdateGraphic();
      continue;
    }
    // if not, are there text elements amongst its descendents
    UpdateTextFragmentTrees(frame);
  }
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGMutationObserver::UpdateTextFragmentTrees(nsIFrame *aFrame)
{
  nsIFrame* kid = aFrame->GetFirstChild(nsnull);
  while (kid) {
    if (kid->GetType() == nsGkAtoms::svgTextFrame) {
      nsSVGTextFrame* textFrame = static_cast<nsSVGTextFrame*>(kid);
      textFrame->NotifyGlyphMetricsChange();
    } else {
      UpdateTextFragmentTrees(kid);
    }
    kid = kid->GetNextSibling();
  }
}

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{  
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(aContent);
  if (!svgElement) {
    NS_ERROR("Can't create frame! Content is not an SVG 'svg' element!");
    return nsnull;
  }

  return new (aPresShell) nsSVGOuterSVGFrame(aContext);
}

nsSVGOuterSVGFrame::nsSVGOuterSVGFrame(nsStyleContext* aContext)
    : nsSVGOuterSVGFrameBase(aContext),
      mRedrawSuspendCount(0),
      mViewportInitialized(PR_FALSE)
{
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::Init(nsIContent* aContent,
                         nsIFrame* aParent,
                         nsIFrame* aPrevInFlow)
{
  nsresult rv = nsSVGOuterSVGFrameBase::Init(aContent, aParent, aPrevInFlow);

  nsIDocument* doc = mContent->GetCurrentDoc();
  if (doc) {
    // we only care about our content's zoom and pan values if it's the root element
    if (doc->GetRootContent() == mContent) {
      nsSVGSVGElement *SVGElement = static_cast<nsSVGSVGElement*>(mContent);
      SVGElement->GetCurrentTranslate(getter_AddRefs(mCurrentTranslate));
      SVGElement->GetCurrentScaleNumber(getter_AddRefs(mCurrentScale));
    }
    // AddMutationObserver checks that the observer is not already added.
    // sSVGMutationObserver has the same lifetime as the document so does
    // not need to be removed
    doc->AddMutationObserver(&sSVGMutationObserver);
  }

  SuspendRedraw();  // UnsuspendRedraw is in DidReflow

  AddStateBits(NS_STATE_IS_OUTER_SVG);

  return rv;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGOuterSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
NS_INTERFACE_MAP_END_INHERITING(nsSVGOuterSVGFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods
  
//----------------------------------------------------------------------
// reflowing

/* virtual */ nscoord
nsSVGOuterSVGFrame::GetMinWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_MIN_WIDTH(this, result);

  result = nscoord(0);

  return result;
}

/* virtual */ nscoord
nsSVGOuterSVGFrame::GetPrefWidth(nsIRenderingContext *aRenderingContext)
{
  nscoord result;
  DISPLAY_PREF_WIDTH(this, result);

  nsSVGSVGElement *svg = static_cast<nsSVGSVGElement*>(mContent);
  nsSVGLength2 &width = svg->mLengthAttributes[nsSVGSVGElement::WIDTH];

  if (width.GetSpecifiedUnitType() == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    result = nscoord(0);
  } else {
    result = nsPresContext::CSSPixelsToAppUnits(width.GetAnimValue(svg));
  }

  return result;
}

/* virtual */ nsIFrame::IntrinsicSize
nsSVGOuterSVGFrame::GetIntrinsicSize()
{
  // XXXjwatt Note that here we want to return the CSS width/height if they're
  // specified and we're embedded inside an nsIObjectLoadingContent.

  IntrinsicSize intrinsicSize;

  nsSVGSVGElement *content = static_cast<nsSVGSVGElement*>(mContent);
  nsSVGLength2 &width  = content->mLengthAttributes[nsSVGSVGElement::WIDTH];
  nsSVGLength2 &height = content->mLengthAttributes[nsSVGSVGElement::HEIGHT];

  if (width.GetSpecifiedUnitType() == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    float val = width.GetAnimValInSpecifiedUnits() / 100.0f;
    if (val < 0.0f) val = 0.0f;
    intrinsicSize.width.SetPercentValue(val);
  } else {
    nscoord val = nsPresContext::CSSPixelsToAppUnits(width.GetAnimValue(content));
    if (val < 0) val = 0;
    intrinsicSize.width.SetCoordValue(val);
  }

  if (height.GetSpecifiedUnitType() == nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    float val = height.GetAnimValInSpecifiedUnits() / 100.0f;
    if (val < 0.0f) val = 0.0f;
    intrinsicSize.height.SetPercentValue(val);
  } else {
    nscoord val = nsPresContext::CSSPixelsToAppUnits(height.GetAnimValue(content));
    if (val < 0) val = 0;
    intrinsicSize.height.SetCoordValue(val);
  }

  return intrinsicSize;
}

/* virtual */ nsSize
nsSVGOuterSVGFrame::GetIntrinsicRatio()
{
  // We only have an intrinsic size/ratio if our width and height attributes
  // are both specified and set to non-percentage values, or we have a viewBox
  // rect: http://www.w3.org/TR/SVGMobile12/coords.html#IntrinsicSizing

  nsSVGSVGElement *content = static_cast<nsSVGSVGElement*>(mContent);
  nsSVGLength2 &width  = content->mLengthAttributes[nsSVGSVGElement::WIDTH];
  nsSVGLength2 &height = content->mLengthAttributes[nsSVGSVGElement::HEIGHT];

  if (width.GetSpecifiedUnitType()  != nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE &&
      height.GetSpecifiedUnitType() != nsIDOMSVGLength::SVG_LENGTHTYPE_PERCENTAGE) {
    return nsSize(width.GetAnimValue(content), height.GetAnimValue(content));
  }

  if (content->HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    // XXXjwatt we need to fix our viewBox code so that we can tell whether the
    // viewBox attribute specifies a valid rect or not.
    float viewBoxWidth, viewBoxHeight;
    nsCOMPtr<nsIDOMSVGRect> viewBox;
    content->mViewBox->GetAnimVal(getter_AddRefs(viewBox));
    viewBox->GetWidth(&viewBoxWidth);
    viewBox->GetHeight(&viewBoxHeight);
    return nsSize(viewBoxWidth, viewBoxHeight);
  }

  return nsSVGOuterSVGFrameBase::GetIntrinsicRatio();
}

/* virtual */ nsSize
nsSVGOuterSVGFrame::ComputeSize(nsIRenderingContext *aRenderingContext,
                                nsSize aCBSize, nscoord aAvailableWidth,
                                nsSize aMargin, nsSize aBorder, nsSize aPadding,
                                PRBool aShrinkWrap)
{
  if (EmbeddedByReference()) {
    // The embedding element has done the replaced element sizing, using our
    // intrinsic dimensions as necessary. We just need to fill the viewport.
    return aCBSize;
  }

  return nsLayoutUtils::ComputeSizeWithIntrinsicDimensions(
                            aRenderingContext, this,
                            GetIntrinsicSize(), GetIntrinsicRatio(), aCBSize,
                            aMargin, aBorder, aPadding);
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::Reflow(nsPresContext*           aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsSVGOuterSVGFrame");
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aDesiredSize, aStatus);
  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("enter nsSVGOuterSVGFrame::Reflow: availSize=%d,%d",
                  aReflowState.availableWidth, aReflowState.availableHeight));

  NS_PRECONDITION(mState & NS_FRAME_IN_REFLOW, "frame is not in reflow");

  aStatus = NS_FRAME_COMPLETE;

  aDesiredSize.width  = aReflowState.ComputedWidth() +
                          aReflowState.mComputedBorderPadding.LeftRight();
  aDesiredSize.height = aReflowState.ComputedHeight() +
                          aReflowState.mComputedBorderPadding.TopBottom();

  NS_ASSERTION(!GetPrevInFlow(), "SVG can't currently be broken across pages.");

  // Make sure we scroll if we're too big:
  // XXX Use the bounding box of our descendants? (See bug 353460 comment 14.)
  aDesiredSize.mOverflowArea.SetRect(0, 0, aDesiredSize.width, aDesiredSize.height);
  FinishAndStoreOverflow(&aDesiredSize);

  // If our SVG viewport has changed, update our content and notify.
  // http://www.w3.org/TR/SVG11/coords.html#ViewportSpace

  svgFloatSize newViewportSize(
    nsPresContext::AppUnitsToFloatCSSPixels(aReflowState.ComputedWidth()),
    nsPresContext::AppUnitsToFloatCSSPixels(aReflowState.ComputedHeight()));

  nsSVGSVGElement *svgElem = static_cast<nsSVGSVGElement*>(mContent);

  if (newViewportSize != svgElem->GetViewportSize()) {
    svgElem->SetViewportSize(newViewportSize);
    NotifyViewportChange();
  }

  NS_FRAME_TRACE(NS_FRAME_TRACE_CALLS,
                  ("exit nsSVGOuterSVGFrame::Reflow: size=%d,%d",
                  aDesiredSize.width, aDesiredSize.height));
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

PR_STATIC_CALLBACK(PLDHashOperator)
ReflowForeignObject(nsVoidPtrHashKey *aEntry, void* aUserArg)
{
  static_cast<nsSVGForeignObjectFrame*>
    (const_cast<void*>(aEntry->GetKey()))->MaybeReflowFromOuterSVGFrame();
  return PL_DHASH_NEXT;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::DidReflow(nsPresContext*   aPresContext,
                              const nsHTMLReflowState*  aReflowState,
                              nsDidReflowStatus aStatus)
{
  nsresult rv = nsSVGOuterSVGFrameBase::DidReflow(aPresContext,aReflowState,aStatus);

  if (!mViewportInitialized) {
    // it is now
    mViewportInitialized = PR_TRUE;

    // call InitialUpdate() on all frames:
    nsIFrame* kid = mFrames.FirstChild();
    while (kid) {
      nsISVGChildFrame* SVGFrame = nsnull;
      CallQueryInterface(kid, &SVGFrame);
      if (SVGFrame) {
        SVGFrame->InitialUpdate(); 
      }
      kid = kid->GetNextSibling();
    }
    
    UnsuspendRedraw(); // For the SuspendRedraw in InitSVG
  } else {
    // Now that all viewport establishing descendants have their correct size,
    // tell our foreignObject descendants to reflow their children.
    if (mForeignObjectHash.IsInitialized()) {
      PRUint32 count = mForeignObjectHash.EnumerateEntries(ReflowForeignObject, nsnull);
      NS_ASSERTION(count == mForeignObjectHash.Count(),
                   "We didn't reflow all our nsSVGForeignObjectFrames!");
    }
  }
  
  return rv;
}

//----------------------------------------------------------------------
// container methods

class nsDisplaySVG : public nsDisplayItem {
public:
  nsDisplaySVG(nsSVGOuterSVGFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplaySVG);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVG() {
    MOZ_COUNT_DTOR(nsDisplaySVG);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("SVGEventReceiver")
};

nsIFrame*
nsDisplaySVG::HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt)
{
  return static_cast<nsSVGOuterSVGFrame*>(mFrame)->
    GetFrameForPoint(aPt - aBuilder->ToReferenceFrame(mFrame));
}

void
nsDisplaySVG::Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect)
{
  static_cast<nsSVGOuterSVGFrame*>(mFrame)->
    Paint(*aCtx, aDirtyRect, aBuilder->ToReferenceFrame(mFrame));
}

// helper
static inline PRBool
DependsOnIntrinsicSize(const nsIFrame* aEmbeddingFrame)
{
  const nsStylePosition *pos = aEmbeddingFrame->GetStylePosition();
  nsStyleUnit widthUnit  = pos->mWidth.GetUnit();
  nsStyleUnit heightUnit = pos->mHeight.GetUnit();

  // XXX it would be nice to know if the size of aEmbeddingFrame's containing
  // block depends on aEmbeddingFrame, then we'd know if we can return false
  // for eStyleUnit_Percent too.
  return (widthUnit != eStyleUnit_Coord) || (heightUnit != eStyleUnit_Coord);
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::AttributeChanged(PRInt32  aNameSpaceID,
                                     nsIAtom* aAttribute,
                                     PRInt32  aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      !(GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    nsIFrame* embeddingFrame;
    EmbeddedByReference(&embeddingFrame);
    if (embeddingFrame) {
      if (DependsOnIntrinsicSize(embeddingFrame)) {
        // Tell embeddingFrame's presShell it needs to be reflowed (which takes
        // care of reflowing us too).
        embeddingFrame->PresContext()->PresShell()->
          FrameNeedsReflow(embeddingFrame, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
      }
      // else our width and height is overridden - don't reflow anything
    } else {
      // We are not embedded by reference, so our 'width' and 'height'
      // attributes are not overridden - we need to reflow.
      PresContext()->PresShell()->
        FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
    }
  }

  return NS_OK;
}

nsIFrame*
nsSVGOuterSVGFrame::GetFrameForPoint(const nsPoint& aPoint)
{
  // XXX This algorithm is really bad. Because we only have a
  // singly-linked list we have to test each and every SVG element for
  // a hit. What we really want is a double-linked list.

  float x = PresContext()->AppUnitsToDevPixels(aPoint.x);
  float y = PresContext()->AppUnitsToDevPixels(aPoint.y);

  nsRect thisRect(nsPoint(0,0), GetSize());
  if (!thisRect.Contains(aPoint)) {
    return nsnull;
  }

  nsIFrame* hit;
  nsSVGUtils::HitTestChildren(this, x, y, &hit);

  return hit;
}

//----------------------------------------------------------------------
// painting

NS_IMETHODIMP
nsSVGOuterSVGFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  nsresult rv = DisplayBorderBackgroundOutline(aBuilder, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  return aLists.Content()->AppendNewToTop(new (aBuilder) nsDisplaySVG(this));
}

void
nsSVGOuterSVGFrame::Paint(nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect, nsPoint aPt)
{
#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
  {
    nsCOMPtr<nsIDeviceContext>  dx;
    aRenderingContext.GetDeviceContext(*getter_AddRefs(dx));
    float zoom,tzoom,scale;
    dx->GetZoom(zoom);
    dx->GetTextZoom(tzoom);
    dx->GetCanonicalPixelScale(scale);
    printf("nsSVGOuterSVGFrame(%p)::Paint()[ z=%f tz=%f ps=%f\n",this,zoom,tzoom,scale);
    printf("dirtyrect= %d, %d, %d, %d\n", aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
    nsTransform2D* xform;
    aRenderingContext.GetCurrentTransform(xform);
    printf("translation=(%f,%f)\n", xform->GetXTranslation(), xform->GetYTranslation());
    float sx=1.0f,sy=1.0f;
    xform->TransformNoXLate(&sx,&sy);
    printf("scale=(%f,%f)\n", sx, sy);
    float twipsPerScPx = aPresContext->ScaledPixelsToTwips();
    float twipsPerPx = aPresContext->PixelsToTwips();
    printf("tw/sc(px)=%f tw/px=%f\n", twipsPerScPx, twipsPerPx);
    int fontsc;
    GetPresContext()->GetFontScaler(&fontsc);
    printf("font scale=%d\n",fontsc);
    printf("]\n");
  }
#endif
  
  // initialize Mozilla rendering context
  aRenderingContext.PushState();

  nsMargin bp = GetUsedBorderAndPadding();
  ApplySkipSides(bp);

  nsRect viewportRect = GetContentRect();
  nsPoint viewportOffset = aPt + nsPoint(bp.left, bp.top);
  viewportRect.MoveTo(viewportOffset);

  nsRect clipRect;
  clipRect.IntersectRect(aDirtyRect, viewportRect);
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
  aRenderingContext.Translate(viewportRect.x, viewportRect.y);
  nsRect dirtyRect = clipRect - viewportOffset;

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime start = PR_Now();
#endif

  dirtyRect.ScaleRoundOut(1.0f / PresContext()->AppUnitsPerDevPixel());

  nsSVGRenderState ctx(&aRenderingContext);

  // nquartz fallback paths, which svg tends to trigger, need
  // a non-window context target
#ifdef XP_MACOSX
  ctx.GetGfxContext()->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
#endif

  // paint children:
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(&ctx, &dirtyRect, kid);
  }

// show the surface we pushed earlier for nquartz fallbacks
#ifdef XP_MACOSX
  ctx.GetGfxContext()->PopGroupToSource();
  ctx.GetGfxContext()->Paint();
#endif

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime end = PR_Now();
  printf("SVG Paint Timing: %f ms\n", (end-start)/1000.0);
#endif
  
  aRenderingContext.PopState();
}

nsIAtom *
nsSVGOuterSVGFrame::GetType() const
{
  return nsGkAtoms::svgOuterSVGFrame;
}

//----------------------------------------------------------------------
// nsSVGOuterSVGFrame methods:

nsresult
nsSVGOuterSVGFrame::InvalidateRect(nsRect aRect)
{
  aRect.ScaleRoundOut(PresContext()->AppUnitsPerDevPixel());
  Invalidate(aRect);

  return NS_OK;
}

PRBool
nsSVGOuterSVGFrame::IsRedrawSuspended()
{
  return (mRedrawSuspendCount>0) || !mViewportInitialized;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:


NS_IMETHODIMP
nsSVGOuterSVGFrame::SuspendRedraw()
{
#ifdef DEBUG
  //printf("suspend redraw (count=%d)\n", mRedrawSuspendCount);
#endif
  if (++mRedrawSuspendCount != 1)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::UnsuspendRedraw()
{
#ifdef DEBUG
//  printf("unsuspend redraw (count=%d)\n", mRedrawSuspendCount);
#endif

  NS_ASSERTION(mRedrawSuspendCount >=0, "unbalanced suspend count!");

  if (--mRedrawSuspendCount > 0)
    return NS_OK;

  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame=nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::NotifyViewportChange()
{
  // no point in doing anything when were not init'ed yet:
  if (!mViewportInitialized) return NS_OK;

/* XXX this caused reftest failures
  // viewport changes only affect our transform if we have a viewBox attribute
  nsSVGSVGElement *svgElem = static_cast<nsSVGSVGElement*>(mContent);
  if (!svgElem->HasAttr(kNameSpaceID_None, nsGkAtoms::viewBox)) {
    return NS_OK;
  }
*/

  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame = nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(PR_FALSE); 
    kid = kid->GetNextSibling();
  }
  UnsuspendRedraw();
  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

already_AddRefed<nsIDOMSVGMatrix>
nsSVGOuterSVGFrame::GetCanvasTM()
{
  if (!mCanvasTM) {
    nsSVGSVGElement *svgElement = static_cast<nsSVGSVGElement*>(mContent);
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(mCanvasTM));

    // our content is the document element so we must premultiply the values
    // of its currentScale and currentTranslate properties
    if (mCurrentScale &&
        mCurrentTranslate &&
        svgElement->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].GetAnimValue()
        == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
      nsCOMPtr<nsIDOMSVGMatrix> zoomPanMatrix;
      nsCOMPtr<nsIDOMSVGMatrix> temp;
      float scale, x, y;
      mCurrentScale->GetValue(&scale);
      mCurrentTranslate->GetX(&x);
      mCurrentTranslate->GetY(&y);
      svgElement->CreateSVGMatrix(getter_AddRefs(zoomPanMatrix));
      zoomPanMatrix->Translate(x, y, getter_AddRefs(temp));
      temp->Scale(scale, getter_AddRefs(zoomPanMatrix));
      zoomPanMatrix->Multiply(mCanvasTM, getter_AddRefs(temp));
      temp.swap(mCanvasTM);
    }
  }
  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

//----------------------------------------------------------------------
// Implementation helpers

void
nsSVGOuterSVGFrame::RegisterForeignObject(nsSVGForeignObjectFrame* aFrame)
{
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");

  if (!mForeignObjectHash.IsInitialized()) {
    if (!mForeignObjectHash.Init()) {
      NS_ERROR("Failed to initialize foreignObject hash.");
      return;
    }
  }

  NS_ASSERTION(!mForeignObjectHash.GetEntry(aFrame),
               "nsSVGForeignObjectFrame already registered!");

  mForeignObjectHash.PutEntry(aFrame);

  NS_ASSERTION(mForeignObjectHash.GetEntry(aFrame),
               "Failed to register nsSVGForeignObjectFrame!");
}

void
nsSVGOuterSVGFrame::UnregisterForeignObject(nsSVGForeignObjectFrame* aFrame) {
  NS_ASSERTION(aFrame, "Who on earth is calling us?!");
  NS_ASSERTION(mForeignObjectHash.GetEntry(aFrame),
               "nsSVGForeignObjectFrame not in registry!");
  return mForeignObjectHash.RemoveEntry(aFrame);
}

PRBool
nsSVGOuterSVGFrame::EmbeddedByReference(nsIFrame **aEmbeddingFrame)
{
  if (mContent->GetParent() == nsnull) {
    // Our content is the document element
    nsCOMPtr<nsISupports> container = PresContext()->GetContainer();
    nsCOMPtr<nsIDOMWindowInternal> window = do_GetInterface(container);
    if (window) {
      nsCOMPtr<nsIDOMElement> frameElement;
      window->GetFrameElement(getter_AddRefs(frameElement));
      nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(frameElement);
      if (olc) {
        // Our document is inside an HTML 'object', 'embed' or 'applet' element
        if (aEmbeddingFrame) {
          nsCOMPtr<nsIContent> element = do_QueryInterface(frameElement);
          *aEmbeddingFrame =
            static_cast<nsGenericElement*>(element.get())->GetPrimaryFrame();
          NS_ASSERTION(*aEmbeddingFrame, "Yikes, no embedding frame!");
        }
        return PR_TRUE;
      }
    }
  }
  if (aEmbeddingFrame) {
    *aEmbeddingFrame = nsnull;
  }
  return PR_FALSE;
}
