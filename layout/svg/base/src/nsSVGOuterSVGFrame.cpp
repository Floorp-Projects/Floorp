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
#include "nsSVGRect.h"
#include "nsDisplayList.h"
#include "nsStubMutationObserver.h"
#include "gfxContext.h"
#include "nsPresShellIterator.h"

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
nsSVGOuterSVGFrame::InitSVG()
{
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

  return NS_OK;
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

NS_IMETHODIMP
nsSVGOuterSVGFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  if (!aReflowState.ShouldReflowAllKids()) {
    // We're not the target of the incremental reflow, so just bail.
    // This means that something happened to one of our descendants
    // (excluding those inside svg:foreignObject, since
    // nsSVGForeignObjectFrame is a reflow root).
    aDesiredSize.width = mRect.width;
    aDesiredSize.height = mRect.height;
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  // http://www.w3.org/TR/SVG11/coords.html#ViewportSpace specifies our behavior

  SuspendRedraw();

  // Take a note of the current viewport dimensions.
  //
  // XXX note: "viewport" is kinda ambiguous here. We have the viewport into
  // which we're being rendered and we also have the viewport established by
  // our width and height attributes. The mViewportWidth/Height below are the
  // dimensions of the viewport established _by_ our width and height
  // attributes. These can be thought of as our "computed" width/height in CSS
  // terms (and maybe we should call them that).

  nsSVGSVGElement *svgElem = static_cast<nsSVGSVGElement*>(mContent);
  float oldViewportWidth  = svgElem->mViewportWidth;
  float oldViewportHeight = svgElem->mViewportHeight;

  // Tell our element the dimensions of the viewport into which it is rendering
  // so that it can calculate its computed width/height (mViewportWidth/Height)

  nsRect maxRect, preferredRect;
  CalculateAvailableSpace(&maxRect, &preferredRect, aPresContext, aReflowState);
  float preferredWidth = nsPresContext::AppUnitsToFloatCSSPixels(preferredRect.width);
  float preferredHeight = nsPresContext::AppUnitsToFloatCSSPixels(preferredRect.height);

  nsCOMPtr<nsIDOMSVGRect> r;
  NS_NewSVGRect(getter_AddRefs(r), 0, 0, preferredWidth, preferredHeight);
  svgElem->SetCoordCtxRect(r);

  // Now we can get the computed width/height in app units

  aDesiredSize.width =
    nsPresContext::CSSPixelsToAppUnits(svgElem->mViewportWidth);
  aDesiredSize.height =
    nsPresContext::CSSPixelsToAppUnits(svgElem->mViewportHeight);

  // XXX add in CSS borders ??

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  if (svgElem->mViewportWidth != oldViewportWidth ||
      svgElem->mViewportHeight != oldViewportHeight) {
    NotifyViewportChange();
  }

  UnsuspendRedraw();
  
  return NS_OK;
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
  }
  
  return rv;
}

//----------------------------------------------------------------------
// container methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::InsertFrames(nsIAtom*        aListName,
                                 nsIFrame*       aPrevFrame,
                                 nsIFrame*       aFrameList)
{
  SuspendRedraw();
  nsSVGOuterSVGFrameBase::InsertFrames(aListName, aPrevFrame, aFrameList);
  UnsuspendRedraw();
  
  return NS_OK;
}

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

NS_IMETHODIMP
nsSVGOuterSVGFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      !(GetStateBits() & NS_FRAME_FIRST_REFLOW) &&
      (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height)) {
    PresContext()->PresShell()->
      FrameNeedsReflow(this, nsIPresShell::eStyleChange, NS_FRAME_IS_DIRTY);
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
  // XXX Not sure why this nsSVGOuterSVGFrame::Paint doesn't paint its
  // background or respect CSS visiblity
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
  
  nsRect clipRect;
  clipRect.IntersectRect(aDirtyRect, nsRect(aPt, GetSize()));
  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kIntersect);
  aRenderingContext.Translate(aPt.x, aPt.y);
  nsRect dirtyRect = clipRect - aPt;

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
    if (svgElement->mEnumAttributes[nsSVGSVGElement::ZOOMANDPAN].GetAnimValue()
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
nsSVGOuterSVGFrame::CalculateAvailableSpace(nsRect *maxRect,
                                            nsRect *preferredRect,
                                            nsPresContext* aPresContext,
                                            const nsHTMLReflowState& aReflowState)
{
  *preferredRect = aPresContext->GetVisibleArea();
  
  // XXXldb What about margin?
  if (aReflowState.availableWidth != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.availableWidth;
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->ComputedWidth() != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.parentReflowState->ComputedWidth();
  else
    maxRect->width = NS_MAXSIZE;
  
  if (aReflowState.availableHeight != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.availableHeight;    
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->ComputedHeight() != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.parentReflowState->ComputedHeight();
  else
    maxRect->height = NS_MAXSIZE;

  if (preferredRect->width > maxRect->width)
    preferredRect->width = maxRect->width;
  if (preferredRect->height > maxRect->height)
    preferredRect->height = maxRect->height;
}  
