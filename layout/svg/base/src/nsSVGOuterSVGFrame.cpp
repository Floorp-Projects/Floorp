/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 *    Alex Fritze <alex.fritze@crocodile-clips.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

//#include "nsHTMLContainerFrame.h"
#include "nsContainerFrame.h"
#include "nsCSSRendering.h"
#include "nsIDOMSVGSVGElement.h"
#include "nsIPresContext.h"
#include "nsIDOMSVGRect.h"
#include "nsIDOMSVGAnimatedLength.h"
#include "nsIDOMSVGLength.h"
#include "nsISVGFrame.h"
#include "nsSVGRenderingContext.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWeakReference.h"
#include "nsISVGValue.h"
#include "nsISVGValueObserver.h"
#include "nsHTMLParts.h"
#include "nsReflowPath.h"

//typedef nsHTMLContainerFrame nsSVGOuterSVGFrameBase;
typedef nsContainerFrame nsSVGOuterSVGFrameBase;

class nsSVGOuterSVGFrame : public nsSVGOuterSVGFrameBase,
                           public nsISVGFrame,
                           public nsISVGValueObserver,
                           public nsSupportsWeakReference
{
  friend nsresult
  NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame);
protected:
  nsSVGOuterSVGFrame();
  virtual ~nsSVGOuterSVGFrame();
  nsresult Init();
  
   // nsISupports interface:
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }  
public:
  // nsIFrame:
  NS_IMETHOD Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  
  NS_IMETHOD Reflow(nsIPresContext*          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD  DidReflow(nsIPresContext*   aPresContext,
                        const nsHTMLReflowState*  aReflowState,
                        nsDidReflowStatus aStatus);


  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  InsertFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aPrevFrame,
                           nsIFrame*       aFrameList);
  NS_IMETHOD  RemoveFrame(nsIPresContext* aPresContext,
                          nsIPresShell&   aPresShell,
                          nsIAtom*        aListName,
                          nsIFrame*       aOldFrame);
  NS_IMETHOD  ReplaceFrame(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aOldFrame,
                           nsIFrame*       aNewFrame);

  NS_IMETHOD  AttributeChanged(nsIPresContext* aPresContext,
                               nsIContent*     aChild,
                               PRInt32         aNameSpaceID,
                               nsIAtom*        aAttribute,
                               PRInt32         aModType,
                               PRInt32         aHint);

//  NS_IMETHOD SetView(nsIPresContext* aPresContext, nsIView* aView);

  NS_IMETHOD  GetFrameForPoint(nsIPresContext* aPresContext,
                               const nsPoint& aPoint, 
                               nsFramePaintLayer aWhichLayer,
                               nsIFrame**     aFrame);

  
  NS_IMETHOD  Paint(nsIPresContext* aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect,
                    nsFramePaintLayer aWhichLayer,
                    PRUint32 aFlags = 0);

  // nsISVGValueObserver
  NS_IMETHOD WillModifySVGObservable(nsISVGValue* observable);
  NS_IMETHOD DidModifySVGObservable (nsISVGValue* observable);

  // nsISupportsWeakReference
  // implementation inherited from nsSupportsWeakReference
  
  // nsISVGFrame interface:
  NS_IMETHOD Paint(nsSVGRenderingContext* renderingContext);
  NS_IMETHOD InvalidateRegion(ArtUta* uta, PRBool bRedraw);
  NS_IMETHOD GetFrameForPoint(float x, float y, nsIFrame** hit);  
  NS_IMETHOD NotifyCTMChanged();

  NS_IMETHOD NotifyRedrawSuspended();
  NS_IMETHOD NotifyRedrawUnsuspended();
  NS_IMETHOD IsRedrawSuspended(PRBool* isSuspended);
  
protected:
  // implementation helpers:
  void InitiateReflow();
  
  float GetPxPerTwips();
  float GetTwipsPerPx();

  
//  nsIView* mView;
  nsIPresShell* mPresShell; // XXX is a non-owning ref ok?
  PRBool mRedrawSuspended;
  PRBool mNeedsReflow;
};

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGOuterSVGFrame(nsIPresShell* aPresShell, nsIContent* aContent, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;
  
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(aContent);
  if (!svgElement) {
#ifdef DEBUG
    printf("warning: trying to construct an SVGOuterSVGFrame for a content element that doesn't support the right interfaces\n");
#endif
    return NS_ERROR_FAILURE;
  }

  nsSVGOuterSVGFrame* it = new (aPresShell) nsSVGOuterSVGFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;

  // XXX is this ok?
  it->mPresShell = aPresShell;
  
  return NS_OK;
}

nsSVGOuterSVGFrame::nsSVGOuterSVGFrame()
    : mRedrawSuspended(PR_FALSE),
      mNeedsReflow(PR_FALSE)
{
}

nsSVGOuterSVGFrame::~nsSVGOuterSVGFrame()
{
#ifdef DEBUG
//  printf("~nsSVGOuterSVGFrame %p\n", this);
#endif

  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
  NS_ASSERTION(svgElement, "wrong content element");  

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetWidth(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "null length object");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "null length object");
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(length);
    if (value)
      value->RemoveObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetHeight(getter_AddRefs(animLength));
    NS_ASSERTION(animLength, "null length object");
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ASSERTION(length, "null length object");
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(length);
    if (value)
      value->RemoveObserver(this);
  }
}

nsresult nsSVGOuterSVGFrame::Init()
{
  nsCOMPtr<nsIDOMSVGSVGElement> svgElement = do_QueryInterface(mContent);
  NS_ASSERTION(svgElement, "wrong content element");  
 
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetWidth(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(length);
    if (value)
      value->AddObserver(this);
  }

  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    svgElement->GetHeight(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);
    nsCOMPtr<nsISVGValue> value = do_QueryInterface(length);
    if (value)
      value->AddObserver(this);
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGOuterSVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGOuterSVGFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::Init(nsIPresContext*  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGOuterSVGFrameBase::Init(aPresContext, aContent, aParent,
                                    aContext, aPrevInFlow);

  Init();

  return rv;
}


  
//----------------------------------------------------------------------
// reflowing

NS_IMETHODIMP
nsSVGOuterSVGFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  // check whether this reflow request is targeted at us or a child
  // frame (e.g. a foreignObject):
  if (aReflowState.reason == eReflowReason_Incremental) {
    nsReflowPath::iterator iter = aReflowState.path->FirstChild();
    nsReflowPath::iterator end = aReflowState.path->EndChildren();

    for ( ; iter != end; ++iter) {
      // The actual target of this reflow is one of our child
      // frames. Since SVG as such doesn't use reflow, this will
      // probably be the child of a <foreignObject>. Some HTML|XUL
      // content frames target reflow events at themselves when they
      // need to be redrawn in response to e.g. a style change. For
      // correct visual updating, we must make sure the reflow
      // reaches its intended target.
        
      // Since it is an svg frame (probably an nsSVGForeignObjectFrame),
      // we might as well pass in our aDesiredSize and aReflowState
      // objects - they are ignored by svg frames:
      nsSize availSpace(0, 0); // XXXwaterson probably wrong!
      nsHTMLReflowState state(aPresContext, aReflowState, *iter, availSpace);
      (*iter)->Reflow (aPresContext,
                       aDesiredSize,
                       state,
                       aStatus);

      // XXX do we really have to return our metrics although we're
      // not affected by the reflow? Is there a way of telling our
      // parent that we don't want anything changed?
      aDesiredSize.width  = mRect.width;
      aDesiredSize.height = mRect.height;
      aDesiredSize.ascent = aDesiredSize.height;
      aDesiredSize.descent = 0;
    }

    if (! aReflowState.path->mReflowCommand) {
      // We're not the target of the incremental reflow, so just bail.
      aStatus = NS_FRAME_COMPLETE;
      return NS_OK;
    }
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
  
  NS_ENSURE_TRUE(mContent, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMSVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(SVGElement, NS_ERROR_FAILURE);

  float pxPerTwips = GetPxPerTwips();
  float twipsPerPx = GetTwipsPerPx();
  
  // Temporarily set the viewport to the available size in
  // case percentage width/height attribs have been given:
  nsCOMPtr<nsIDOMSVGRect> viewport;
  SVGElement->GetViewport(getter_AddRefs(viewport));
  NS_ENSURE_TRUE(viewport, NS_ERROR_FAILURE);
  NS_ENSURE_TRUE(aReflowState.parentReflowState, NS_ERROR_FAILURE);
  viewport->SetWidth(aReflowState.parentReflowState->mComputedWidth * pxPerTwips);
  if (aReflowState.parentReflowState->mComputedHeight != NS_UNCONSTRAINEDSIZE)
    viewport->SetHeight(aReflowState.parentReflowState->mComputedHeight * pxPerTwips);
  else
    viewport->SetHeight(aReflowState.parentReflowState->mComputedWidth * pxPerTwips);

#ifdef DEBUG
  // some debug stuff:
//   {
//     nsRect r;
//     aPresContext->GetVisibleArea(r);
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
  
  // calculate width:
//  if (aReflowState.mComputedWidth == NS_INTRINSICSIZE)
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    SVGElement->GetWidth(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);
    
    float w;
    length->GetValue(&w);
#ifdef DEBUG
//    printf("  *intrinsic width:%f*\n",(double)w);
#endif
    aDesiredSize.width = (int) (twipsPerPx * w);
  }
//  else {
//    aDesiredSize.width = aReflowState.mComputedWidth;
//  }

  // calculate height:
//  if (aReflowState.mComputedHeight == NS_INTRINSICSIZE)
  {
    nsCOMPtr<nsIDOMSVGAnimatedLength> animLength;
    SVGElement->GetHeight(getter_AddRefs(animLength));
    NS_ENSURE_TRUE(animLength, NS_ERROR_FAILURE);
    nsCOMPtr<nsIDOMSVGLength> length;
    animLength->GetAnimVal(getter_AddRefs(length));
    NS_ENSURE_TRUE(length, NS_ERROR_FAILURE);

    float h;
    length->GetValue(&h);
#ifdef DEBUG
//    printf("  *intrinsic height:%f*\n",(double)h);
#endif
    aDesiredSize.height = (int) (twipsPerPx * h);
  }
//  else {
//    aDesiredSize.height = aReflowState.mComputedHeight;
//  }
  
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
  
  // XXX add in CSS borders ??

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::DidReflow(nsIPresContext*   aPresContext,
                              const nsHTMLReflowState*  aReflowState,
                              nsDidReflowStatus aStatus)
{
  // Set the viewport. We want the x, y coords relative to the
  // document element, so that we can use them (in conjunction with
  // scrollX, scrollY) to transform mouse event coords into the svg
  // element's viewport coord system.

  NS_ENSURE_TRUE(mContent, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMSVGSVGElement> SVGElement = do_QueryInterface(mContent);
  NS_ENSURE_TRUE(SVGElement, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIDOMSVGRect> viewport;
  SVGElement->GetViewport(getter_AddRefs(viewport));
  NS_ENSURE_TRUE(viewport, NS_ERROR_FAILURE);
  
  nsIFrame* frame = this;
  nsPoint origin(0,0);
  do {
    nsPoint tmpOrigin;
    frame->GetOrigin(tmpOrigin);
    origin += tmpOrigin;

    nsFrameState state;
    frame->GetFrameState(&state);
    if(state & NS_FRAME_OUT_OF_FLOW)
      break;

    frame->GetParent(&frame);
  } while(frame);
  
  float pxPerTwips = GetPxPerTwips();
  
  viewport->SetX(origin.x * pxPerTwips);
  viewport->SetY(origin.y * pxPerTwips);
  viewport->SetWidth(mRect.width   * pxPerTwips);
  viewport->SetHeight(mRect.height * pxPerTwips);

#ifdef DEBUG
//  printf("reflowed nsSVGOuterSVGFrame viewport: (%f, %f, %f, %f)\n",
//         origin.x * pxPerTwips,
//         origin.y * pxPerTwips,
//         mRect.width  * pxPerTwips,
//         mRect.height * pxPerTwips);
#endif
  
  return nsSVGOuterSVGFrameBase::DidReflow(aPresContext,aReflowState,aStatus);
}

//----------------------------------------------------------------------
// container methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::AppendFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;
  
  // Insert the new frames
  mFrames.AppendFrames(this, aFrameList);

  // XXX Get all new frames updated. Should really have a separate
  // function for this and not use NotifyCTMChanged(). For now this
  // will do the trick though:

  // get the view manager, so that we can wrap this up in a batch
  // update.
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      
  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  vm->BeginUpdateViewBatch();

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged(); //XXX use different function
    }
    kid->GetNextSibling(&kid);
  }

  vm->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
  
  // Generate a reflow command to reflow the dirty frames
  //nsHTMLReflowCommand* reflowCmd;
  //rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, eReflowType_ReflowDirty);
  //if (NS_SUCCEEDED(rv)) {
  //  reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
  //  aPresShell.AppendReflowCommand(reflowCmd);
  //  NS_RELEASE(reflowCmd);
  //}
  
  return rv;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::InsertFrames(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aPrevFrame,
                      nsIFrame*       aFrameList)
{
  nsresult  rv = NS_OK;

  // Insert the new frames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  // XXX Get all new frames updated. Should really have a separate
  // function for this and not use NotifyCTMChanged(). For now this
  // will do the trick though:

  // get the view manager, so that we can wrap this up in a batch
  // update.
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      
  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  vm->BeginUpdateViewBatch();

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged(); //XXX use different function
    }
    kid->GetNextSibling(&kid);
  }

  vm->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
  
  // Generate a reflow command to reflow the dirty frames
  //nsHTMLReflowCommand* reflowCmd;
  //rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, eReflowType_ReflowDirty);
  //if (NS_SUCCEEDED(rv)) {
  //  reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
  //  aPresShell.AppendReflowCommand(reflowCmd);
  //  NS_RELEASE(reflowCmd);
  //}
  
  return rv;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::RemoveFrame(nsIPresContext* aPresContext,
                     nsIPresShell&   aPresShell,
                     nsIAtom*        aListName,
                     nsIFrame*       aOldFrame)
{
  PRBool rv = mFrames.DestroyFrame(aPresContext, aOldFrame);
  NS_ASSERTION(rv, "didn't find frame to delete");
  if (NS_FAILED(rv)) return rv;
  
  // XXX Get all new frames updated. Should really have a separate
  // function for this and not use NotifyCTMChanged(). For now this
  // will do the trick though:

  // get the view manager, so that we can wrap this up in a batch
  // update.
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      
  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  vm->BeginUpdateViewBatch();

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCTMChanged(); //XXX use different function
    }
    kid->GetNextSibling(&kid);
  }

  vm->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
  
  // Generate a reflow command to reflow the dirty frames
  //nsHTMLReflowCommand* reflowCmd;
  //rv = NS_NewHTMLReflowCommand(&reflowCmd, aDelegatingFrame, eReflowType_ReflowDirty);
  //if (NS_SUCCEEDED(rv)) {
  //  reflowCmd->SetChildListName(nsLayoutAtoms::absoluteList);
  //  aPresShell.AppendReflowCommand(reflowCmd);
  //  NS_RELEASE(reflowCmd);
  //}

  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::ReplaceFrame(nsIPresContext* aPresContext,
                      nsIPresShell&   aPresShell,
                      nsIAtom*        aListName,
                      nsIFrame*       aOldFrame,
                      nsIFrame*       aNewFrame)
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aModType,
                                     PRInt32         aHint)
{
#ifdef DEBUG
//  {
//    printf("** nsSVGOuterSVGFrame::AttributeChanged(");
//    nsAutoString str;
//    aAttribute->ToString(str);
//    nsCAutoString cstr;
//    cstr.AssignWithConversion(str);
//    printf(cstr.get());
//    printf(", hint:%d)\n",aHint);
//  }
#endif
  return NS_OK;
}


//----------------------------------------------------------------------
//
// NS_IMETHODIMP
// nsSVGOuterSVGFrame::SetView(nsIPresContext* aPresContext, nsIView* aView)
// {
//   mView = aView;
//   return nsSVGOuterSVGFrameBase::SetView(aPresContext, aView);  
// }

nsresult
nsSVGOuterSVGFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint,
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  //printf("nsSVGOuterSVGFrame::GetFrameForPoint(%d)\n", aWhichLayer);
  *aFrame = nsnull;
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND) return NS_ERROR_FAILURE;
  
  float x = GetPxPerTwips() * ( aPoint.x - mRect.x);
  float y = GetPxPerTwips() * ( aPoint.y - mRect.y);
  
  PRBool inThisFrame = mRect.Contains(aPoint);
  
  if (!inThisFrame) {
    return NS_ERROR_FAILURE;
  }

  *aFrame = this;
  nsIFrame* kid = mFrames.FirstChild();
  nsIFrame* hit = nsnull;
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsresult rv = SVGFrame->GetFrameForPoint(x, y, &hit);
      if (NS_SUCCEEDED(rv) && hit) {
        *aFrame = hit;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
    kid->GetNextSibling(&kid);
  }
    
  return NS_OK;
}



//----------------------------------------------------------------------
// painting

NS_IMETHODIMP
nsSVGOuterSVGFrame::Paint(nsIPresContext* aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect,
                          nsFramePaintLayer aWhichLayer,
                          PRUint32 aFlags)
{
  
//    if (NS_FRAME_PAINT_LAYER_BACKGROUND == aWhichLayer) {
//      const nsStyleDisplay* disp = (const nsStyleDisplay*)
//        mStyleContext->GetStyleData(eStyleStruct_Display);
//      if (disp->IsVisible() && mRect.width && mRect.height) {
//        // Paint our background and border
//        const nsStyleBorder* border = (const nsStyleBorder*)
//          mStyleContext->GetStyleData(eStyleStruct_Border);
//        const nsStylePadding* padding = (const nsStylePadding*)
//          mStyleContext->GetStyleData(eStyleStruct_Padding);
//        const nsStyleOutline* outline = (const nsStyleOutline*)
//          mStyleContext->GetStyleData(eStyleStruct_Outline);

//        nsRect  rect(0, 0, mRect.width, mRect.height);
// //       nsCSSRendering::PaintBackground(aPresContext, aRenderingContext, this,
// //                                       aDirtyRect, rect, *border, *padding, 0, 0);
//        nsCSSRendering::PaintBorder(aPresContext, aRenderingContext, this,
//                                    aDirtyRect, rect, *border, mStyleContext, 0);
//        nsCSSRendering::PaintOutline(aPresContext, aRenderingContext, this,
//                                    aDirtyRect, rect, *border, *outline, mStyleContext, 0);
      
//      }
//    }

  
  if (aWhichLayer != NS_FRAME_PAINT_LAYER_FOREGROUND) return NS_OK;
  if (aDirtyRect.width<=0 || aDirtyRect.height<=0) return NS_OK;

  nsSVGRenderingContext SVGCtx(aPresContext, &aRenderingContext, aDirtyRect);
  Paint(&SVGCtx);

  // paint children:
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->Paint(&SVGCtx);
    kid->GetNextSibling(&kid);
  }
  
  SVGCtx.Render();
  
  return NS_OK;
  // see if we have to draw a selection frame around this container
  //return nsFrame::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGOuterSVGFrame::WillModifySVGObservable(nsISVGValue* observable)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGOuterSVGFrame::DidModifySVGObservable(nsISVGValue* observable)
{
  mNeedsReflow = PR_TRUE;
  if (!mRedrawSuspended) {
    InitiateReflow();
  }
  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGFrame methods

NS_IMETHODIMP
nsSVGOuterSVGFrame::Paint(nsSVGRenderingContext* renderingContext)
{
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::InvalidateRegion(ArtUta* uta, PRBool bRedraw)
{
//  NS_ASSERTION(mView, "need a view!");
//  if (!mView) return NS_ERROR_FAILURE;

  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
    
  
  if (!uta && !bRedraw) return NS_OK;

  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  vm->BeginUpdateViewBatch();
  if (uta) {
    int twipsPerPx = (int)(GetTwipsPerPx()+0.5f);
    
    int nRects=0;
    ArtIRect* rectList = art_rect_list_from_uta(uta, 200, 200, &nRects);
    for (int i=0; i<nRects; ++i) {
      nsRect rect(rectList[i].x0 * twipsPerPx, rectList[i].y0 * twipsPerPx,
                  (rectList[i].x1 - rectList[i].x0) * twipsPerPx,
                  (rectList[i].y1 - rectList[i].y0) * twipsPerPx);
      vm->UpdateView(view, rect, NS_VMREFRESH_NO_SYNC);
    }
        
    art_free(rectList);
    art_uta_free(uta);    
  }
  vm->EndUpdateViewBatch(bRedraw ? NS_VMREFRESH_IMMEDIATE : NS_VMREFRESH_NO_SYNC);
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::GetFrameForPoint(float x, float y, nsIFrame** hit)
{
  NS_ASSERTION(PR_FALSE, "shouldn't be called!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::NotifyCTMChanged()
{
  NS_NOTYETIMPLEMENTED("write me!");
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::NotifyRedrawSuspended()
{
  mRedrawSuspended = PR_TRUE;

 // get the view manager, so that we can wrap this up in a batch
  // update.
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      
  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  vm->BeginUpdateViewBatch();
 
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
    kid->GetNextSibling(&kid);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::NotifyRedrawUnsuspended()
{
  // If we need to reflow, do so before we update any of our
  // children. Reflows are likely to affect the display of children:
  if (mNeedsReflow)
    InitiateReflow();
  
  mRedrawSuspended = PR_FALSE;

  // get the view manager, so that we can wrap this up in a batch
  // update.
  NS_ENSURE_TRUE(mPresShell, NS_ERROR_FAILURE);
  nsCOMPtr<nsIPresContext> presCtx;
  mPresShell->GetPresContext(getter_AddRefs(presCtx));
  NS_ENSURE_TRUE(presCtx, NS_ERROR_FAILURE);
  nsIView* view = nsnull;
  GetView(presCtx, &view);
  if (!view) {
    nsIFrame* frame;
    GetParentWithView(presCtx, &frame);
    if (frame)
      frame->GetView(presCtx, &view);
  }
  NS_ENSURE_TRUE(view, NS_ERROR_FAILURE);
      
  nsCOMPtr<nsIViewManager> vm;
  view->GetViewManager(*getter_AddRefs(vm));

  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
    kid->GetNextSibling(&kid);
  }

  vm->EndUpdateViewBatch(NS_VMREFRESH_IMMEDIATE);
  return NS_OK;
}

NS_IMETHODIMP
nsSVGOuterSVGFrame::IsRedrawSuspended(PRBool* isSuspended)
{
  *isSuspended = mRedrawSuspended;
  return NS_OK;
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
  float twipsPerPx=16.0f;
  NS_ASSERTION(mPresShell, "need presshell");
  if (mPresShell) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    presContext->GetScaledPixelsToTwips(&twipsPerPx);
  }
  return twipsPerPx;
}

void nsSVGOuterSVGFrame::InitiateReflow()
{
  mNeedsReflow = PR_FALSE;
  
  // Generate a reflow command to reflow ourselves
  nsHTMLReflowCommand *reflowCmd;
  NS_NewHTMLReflowCommand(&reflowCmd, this, eReflowType_ReflowDirty);
  if (!reflowCmd) {
    NS_ERROR("error creating reflow command object");
    return;
  }
  
  mPresShell->AppendReflowCommand(reflowCmd);
  mPresShell->FlushPendingNotifications(PR_FALSE);  
}
