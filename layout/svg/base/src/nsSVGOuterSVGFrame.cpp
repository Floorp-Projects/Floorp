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
#include "nsISVGRenderer.h"
#include "nsSVGSVGElement.h"
#include "nsIServiceManager.h"
#include "nsIViewManager.h"
#include "nsReflowPath.h"
#include "nsSVGRect.h"
#include "nsDisplayList.h"
#include "nsISVGRendererCanvas.h"

#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
#include "nsIDeviceContext.h"
#include "nsTransform2D.h"
#endif

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsStyleContext* aContext)
{  
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(aContent);
  if (!svgElement) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGOuterSVGFrame for a content element that doesn't support the right interfaces\n");
#endif
    return nsnull;
  }

  return new (aPresShell) nsSVGOuterSVGFrame(aContext);
}

nsSVGOuterSVGFrame::nsSVGOuterSVGFrame(nsStyleContext* aContext)
    : nsSVGOuterSVGFrameBase(aContext),
      mRedrawSuspendCount(0),
      mNeedsReflow(PR_FALSE),
      mViewportInitialized(PR_FALSE)
{
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::InitSVG()
{
  nsresult rv;

  mRenderer = do_CreateInstance(NS_SVG_RENDERER_CAIRO_CONTRACTID, &rv);
  NS_ASSERTION(mRenderer, "could not get SVG renderer");
  if (NS_FAILED(rv))
    return rv;

  // we are an *outer* svg element, so this frame will become the
  // coordinate context for our content element:
  float mmPerPx = GetTwipsPerPx() / TWIPS_PER_POINT_FLOAT / (72.0f * 0.03937f);
  SetCoordCtxMMPerPx(mmPerPx, mmPerPx);
  
  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ASSERTION(SVGElement, "wrong content element");
  SVGElement->SetParentCoordCtxProvider(this);

  // we only care about our content's zoom and pan values if it's the root element
  nsIDocument* doc = mContent->GetCurrentDoc();
  if (doc && doc->GetRootContent() == mContent) {
    SVGElement->GetZoomAndPanEnum(getter_AddRefs(mZoomAndPan));
    SVGElement->GetCurrentTranslate(getter_AddRefs(mCurrentTranslate));
    SVGElement->GetCurrentScaleNumber(getter_AddRefs(mCurrentScale));
  }

  SuspendRedraw();

  AddStateBits(NS_STATE_IS_OUTER_SVG);

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGOuterSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsSVGCoordCtxProvider)
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
#if defined(DEBUG) && defined(SVG_DEBUG_PRINTING)
  {
    printf("nsSVGOuterSVGFrame(%p)::Reflow()[\n",this);
    float twipsPerScPx = aPresContext->ScaledPixelsToTwips();
    float twipsPerPx = aPresContext->PixelsToTwips();
    printf("tw/sc(px)=%f tw/px=%f\n", twipsPerScPx, twipsPerPx);
    printf("]\n");
  }
#endif
  
  if (aReflowState.reason == eReflowReason_Incremental &&
      !aReflowState.path->mReflowCommand) {
    // We're not the target of the incremental reflow, so just bail.
    // This means that something happened to one of our descendants
    // (excluding those inside svg:foreignObject, since
    // nsSVGForeignObjectFrame is a reflow root).

    // XXXldb If this incremental reflow was the result of a style
    // change to something that *contains* a foreignObject, then we're
    // dropping the change completely on the floor!
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }
  
  //  SVG CR 20001102: When the SVG content is embedded inline within
  //  a containing document, and that document is styled using CSS,
  //  then if there are CSS positioning properties specified on the
  //  outermost 'svg' element that are sufficient to establish the
  //  width of the viewport, then these positioning properties
  //  establish the viewport's width; otherwise, the width attribute
  //  on the outermost 'svg' element establishes the viewport's width.
  //  Similarly, if there are CSS positioning properties specified on
  //  the outermost 'svg' element that are sufficient to establish the
  //  height of the viewport, then these positioning properties
  //  establish the viewport's height; otherwise, the height attribute
  //  on the outermost 'svg' element establishes the viewport's
  //  height.
#ifdef DEBUG
  // printf("--- nsSVGOuterSVGFrame(%p)::Reflow(frame:%p,reason:%d) ---\n",this,aReflowState.frame,aReflowState.reason);
#endif
  
  nsCOMPtr<nsISVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(SVGElement, NS_ERROR_FAILURE);

  float pxPerTwips = GetPxPerTwips();
  float twipsPerPx = GetTwipsPerPx();

  // The width/height attribs given on the <svg>-element might be
  // percentage values of the parent viewport. We will set the parent
  // coordinate context dimensions to the available space.

  nsRect maxRect, preferredRect;
  CalculateAvailableSpace(&maxRect, &preferredRect, aPresContext, aReflowState);
  float preferredWidth = preferredRect.width * pxPerTwips;
  float preferredHeight = preferredRect.height * pxPerTwips;

  SuspendRedraw(); 
  
  nsCOMPtr<nsIDOMSVGRect> r;
  NS_NewSVGRect(getter_AddRefs(r), 0, 0, preferredWidth, preferredHeight);
  SetCoordCtxRect(r);
  
#ifdef DEBUG
  // some debug stuff:
//   {
//     nsRect r=aPresContext->GetVisibleArea();
//     printf("******* aw: %d, ah: %d visiw: %d, visih: %d\n",
//            aReflowState.availableWidth,
//            aReflowState.availableHeight,
//            r.width, r.height);
//     printf("******* cw: %d, ch: %d \n    cmaxw: %d, cmaxh: %d\n",
//            aReflowState.mComputedWidth,
//            aReflowState.mComputedHeight,
//            aReflowState.mComputedMaxWidth,
//            aReflowState.mComputedMaxHeight);

//     if (aReflowState.parentReflowState) {
//       printf("******* parent aw: %d, parent ah: %d \n",
//              aReflowState.parentReflowState->availableWidth,
//              aReflowState.parentReflowState->availableHeight);
//       printf("******* parent cw: %d, parent ch: %d \n  parent cmaxw: %d, parent cmaxh: %d\n",
//              aReflowState.parentReflowState->mComputedWidth,
//              aReflowState.parentReflowState->mComputedHeight,
//              aReflowState.parentReflowState->mComputedMaxWidth,
//              aReflowState.parentReflowState->mComputedMaxHeight);
//     }
//   }
#endif

  // now that the parent coord ctx dimensions have been set, the
  // width/height attributes will be valid.
  // Let's work out our desired dimensions.

  nsSVGSVGElement *svg = NS_STATIC_CAST(nsSVGSVGElement*, mContent);
  svg->SetParentCoordCtxProvider(this);
  float width =
    svg->mLengthAttributes[nsSVGSVGElement::WIDTH].GetAnimValue(this);
  float height =
    svg->mLengthAttributes[nsSVGSVGElement::HEIGHT].GetAnimValue(this);

  aDesiredSize.width = (int)(width*twipsPerPx);
  aDesiredSize.height = (int)(height*twipsPerPx);

  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  
  // XXX add in CSS borders ??

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);

  // tell our element that the viewbox to viewport transform needs refreshing,
  // and set us up to draw
  svg->InvalidateViewBoxToViewport();
  NotifyViewportChange();

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
      nsISVGChildFrame* SVGFrame=nsnull;
      kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
      if (SVGFrame) {
        SVGFrame->InitialUpdate(); 
      }
      kid = kid->GetNextSibling();
    }
    
    UnsuspendRedraw();
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
  return NS_STATIC_CAST(nsSVGOuterSVGFrame*, mFrame)->
    GetFrameForPoint(aPt - aBuilder->ToReferenceFrame(mFrame));
}

void
nsDisplaySVG::Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect)
{
  NS_STATIC_CAST(nsSVGOuterSVGFrame*, mFrame)->
    Paint(*aCtx, aDirtyRect, aBuilder->ToReferenceFrame(mFrame));
}

nsIFrame*
nsSVGOuterSVGFrame::GetFrameForPoint(const nsPoint& aPoint)
{
  // XXX This algorithm is really bad. Because we only have a
  // singly-linked list we have to test each and every SVG element for
  // a hit. What we really want is a double-linked list.

  float x = GetPxPerTwips() * aPoint.x;
  float y = GetPxPerTwips() * aPoint.y;

  nsRect thisRect(nsPoint(0,0), GetSize());
  if (!thisRect.Contains(aPoint) || !mRenderer) {
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

  float pxPerTwips = GetPxPerTwips();
  int x0 = (int)(dirtyRect.x*pxPerTwips);
  int y0 = (int)(dirtyRect.y*pxPerTwips);
  int x1 = (int)ceil((dirtyRect.XMost())*pxPerTwips);
  int y1 = (int)ceil((dirtyRect.YMost())*pxPerTwips);
  NS_ASSERTION(x0>=0 && y0>=0, "unexpected negative coordinates");
  NS_ASSERTION(x1-x0>0 && y1-y0>0, "zero sized dirtyRect");
  nsRect dirtyRectPx(x0, y0, x1-x0, y1-y0);

  // If we don't have a renderer due to the component failing
  // to load (gdi+ or cairo not available), indicate to the user
  // what's going on by drawing a red "X" at the appropriate spot.
  if (!mRenderer) {
    aRenderingContext.SetColor(NS_RGB(255,0,0));
    aRenderingContext.DrawLine(0, 0, mRect.width, mRect.height);
    aRenderingContext.DrawLine(mRect.width, 0, 0, mRect.height);
    aRenderingContext.PopState();
    return;
  }

  nsCOMPtr<nsISVGRendererCanvas> canvas;
  mRenderer->CreateCanvas(&aRenderingContext, GetPresContext(), dirtyRectPx,
                          getter_AddRefs(canvas));

  // paint children:
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsSVGUtils::PaintChildWithEffects(canvas, kid);
  }
  
  canvas->Flush();

  canvas = nsnull;

#if defined(DEBUG) && defined(SVG_DEBUG_PAINT_TIMING)
  PRTime end = PR_Now();
  printf("SVG Paint Timing: %f ms\n", (end-start)/1000.0);
#endif
  
  aRenderingContext.PopState();
}

nsIAtom *
nsSVGOuterSVGFrame::GetType() const
{
  return nsLayoutAtoms::svgOuterSVGFrame;
}

//----------------------------------------------------------------------
// nsSVGOuterSVGFrame methods:

NS_IMETHODIMP
nsSVGOuterSVGFrame::InvalidateRect(nsRect aRect)
{
  // just ignore invalidates if painting is suppressed by the shell
  PRBool suppressed = PR_FALSE;
  GetPresContext()->PresShell()->IsPaintingSuppressed(&suppressed);
  if (suppressed)
    return NS_OK;
  
  nsIView* view = GetClosestView();
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);

  nsIViewManager* vm = view->GetViewManager();

  aRect.ScaleRoundOut(GetTwipsPerPx());
  vm->UpdateView(view, aRect, NS_VMREFRESH_NO_SYNC);

  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  *isSuspended = (mRedrawSuspendCount>0) || !mViewportInitialized;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::GetRenderer(nsISVGRenderer**renderer)
{
  *renderer = mRenderer;
  NS_IF_ADDREF(*renderer);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGSVGFrame methods:


NS_IMETHODIMP
nsSVGOuterSVGFrame::SuspendRedraw()
{
  if (!mRenderer)
    return NS_OK;

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
  if (!mRenderer)
    return NS_OK;

#ifdef DEBUG
//  printf("unsuspend redraw (count=%d)\n", mRedrawSuspendCount);
#endif
  if (--mRedrawSuspendCount > 0)
    return NS_OK;
  
  NS_ASSERTION(mRedrawSuspendCount >=0, "unbalanced suspend count!");
  
  // If we need to reflow, do so before we update any of our
  // children. Reflows are likely to affect the display of children:
  if (mNeedsReflow)
    InitiateReflow();
  
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
  if (!mRenderer)
    return NS_OK;

  // no point in doing anything when were not init'ed yet:
  if (!mViewportInitialized) return NS_OK;

  // make sure canvas transform matrix gets (lazily) recalculated:
  mCanvasTM = nsnull;
  
  // inform children
  SuspendRedraw();
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
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
    nsSVGSVGElement *svgElement = NS_STATIC_CAST(nsSVGSVGElement*, mContent);
    svgElement->GetViewboxToViewportTransform(getter_AddRefs(mCanvasTM));

    if (mZoomAndPan) {
      // our content is the document element so we must premultiply the values
      // of it's currentScale and currentTranslate properties
      PRUint16 val;
      mZoomAndPan->GetIntegerValue(val);
      if (val == nsIDOMSVGZoomAndPan::SVG_ZOOMANDPAN_MAGNIFY) {
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
  }
  nsIDOMSVGMatrix* retval = mCanvasTM.get();
  NS_IF_ADDREF(retval);
  return retval;
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGOuterSVGFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mContent, "null parent");

  // Our <svg> content element is the CoordContextProvider for our children:
  nsSVGCoordCtxProvider *provider;
  CallQueryInterface(mContent, &provider);

  return provider;  
}

//----------------------------------------------------------------------
// Implementation helpers

float nsSVGOuterSVGFrame::GetPxPerTwips()
{
  float val = GetTwipsPerPx();
  
  NS_ASSERTION(val!=0.0f, "invalid px/twips");  
  if (val == 0.0) val = 1e-20f;
  
  return 1.0f/val;
}

float nsSVGOuterSVGFrame::GetTwipsPerPx()
{
  return GetPresContext()->ScaledPixelsToTwips();
}

void nsSVGOuterSVGFrame::InitiateReflow()
{
  mNeedsReflow = PR_FALSE;
  
  // Generate a reflow command to reflow ourselves
  nsIPresShell* presShell = GetPresContext()->PresShell();
  presShell->AppendReflowCommand(this, eReflowType_ReflowDirty, nsnull);
  // XXXbz why is this synchronously flushing reflows, exactly?  If it
  // needs to, why is it not using the presshell's reflow batching
  // instead of hacking its own?
  presShell->FlushPendingNotifications(Flush_OnlyReflow);  
}


void
nsSVGOuterSVGFrame::CalculateAvailableSpace(nsRect *maxRect,
                                            nsRect *preferredRect,
                                            nsPresContext* aPresContext,
                                            const nsHTMLReflowState& aReflowState)
{
  *preferredRect = aPresContext->GetVisibleArea();
  
  if (aReflowState.availableWidth != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.availableWidth;
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->mComputedWidth != NS_INTRINSICSIZE)
    maxRect->width = aReflowState.parentReflowState->mComputedWidth;
  else
    maxRect->width = NS_MAXSIZE;
  
  if (aReflowState.availableHeight != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.availableHeight;    
  else if (aReflowState.parentReflowState &&
           aReflowState.parentReflowState->mComputedHeight != NS_INTRINSICSIZE)
    maxRect->height = aReflowState.parentReflowState->mComputedHeight;
  else
    maxRect->height = NS_MAXSIZE;

  if (preferredRect->width > maxRect->width)
    preferredRect->width = maxRect->width;
  if (preferredRect->height > maxRect->height)
    preferredRect->height = maxRect->height;
}  
