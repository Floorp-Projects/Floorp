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
 * Portions created by the Initial Developer are Copyright (C) 2002
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

#include "nsSVGTSpanFrame.h"
#include "nsSVGUtils.h"

//----------------------------------------------------------------------
// Implementation

nsresult
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                    nsIFrame* parentFrame, nsIFrame** aNewFrame)
{
  *aNewFrame = nsnull;

  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContainerFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                              (void**)&text_container);
  if (!text_container) {
    NS_ERROR("trying to construct an SVGTSpanFrame for an invalid container");
    return NS_ERROR_FAILURE;
  }
  
  nsCOMPtr<nsIDOMSVGTSpanElement> tspan_elem = do_QueryInterface(aContent);
  if (!tspan_elem) {
    NS_ERROR("Trying to construct an SVGTSpanFrame for a "
             "content element that doesn't support the right interfaces");
    return NS_ERROR_FAILURE;
  }
  
  nsSVGTSpanFrame* it = new (aPresShell) nsSVGTSpanFrame;
  if (nsnull == it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  
  return NS_OK;
}

nsSVGTSpanFrame::nsSVGTSpanFrame()
  : mCharOffset(0), mFragmentTreeDirty(PR_FALSE), mPropagateTransform(PR_TRUE)
{
}

nsSVGTSpanFrame::~nsSVGTSpanFrame()
{
  // clean up our listener refs:
  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetX();
    if (lengthList)
      NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetY();
    if (lengthList)
      NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetDx();
    if (lengthList)
      NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetDy();
    if (lengthList)
      NS_REMOVE_SVGVALUE_OBSERVER(lengthList);
  }
}

nsresult nsSVGTSpanFrame::InitSVG()
{
  // set us up as a listener for various <tspan>-properties: 
  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetX();
    if (lengthList)
      NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetY();
    if (lengthList)
      NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetDx();
    if (lengthList)
      NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  {
    nsCOMPtr<nsIDOMSVGLengthList> lengthList = GetDy();
    if (lengthList)
      NS_ADD_SVGVALUE_OBSERVER(lengthList);
  }

  return NS_OK;
}

nsIAtom *
nsSVGTSpanFrame::GetType() const
{
  return nsLayoutAtoms::svgTSpanFrame;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGTSpanFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
  NS_INTERFACE_MAP_ENTRY(nsISVGContainerFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGChildFrame)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsISVGValueObserver)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods
NS_IMETHODIMP
nsSVGTSpanFrame::Init(nsPresContext*  aPresContext,
                      nsIContent*      aContent,
                      nsIFrame*        aParent,
                      nsStyleContext*  aContext,
                      nsIFrame*        aPrevInFlow)
{
  nsresult rv;
  rv = nsSVGTSpanFrameBase::Init(aPresContext, aContent, aParent,
                                 aContext, aPrevInFlow);

  InitSVG();
  
  return rv;
}

NS_IMETHODIMP
nsSVGTSpanFrame::AppendFrames(nsIAtom*        aListName,
                              nsIFrame*       aFrameList)
{
  // append == insert at end:
  return InsertFrames(aListName, mFrames.LastChild(), aFrameList);  
}

NS_IMETHODIMP
nsSVGTSpanFrame::InsertFrames(nsIAtom*        aListName,
                              nsIFrame*       aPrevFrame,
                              nsIFrame*       aFrameList)
{
  // memorize last new frame
  nsIFrame* lastNewFrame = nsnull;
  {
    nsFrameList tmpList(aFrameList);
    lastNewFrame = tmpList.LastChild();
  }
  
  // Insert the new frames
  mFrames.InsertFrames(this, aPrevFrame, aFrameList);

  // call InitialUpdate() on all new frames:
  nsIFrame* kid = aFrameList;
  nsIFrame* end = nsnull;
  if (lastNewFrame)
    end = lastNewFrame->GetNextSibling();
  
  while (kid != end) {
    nsISVGChildFrame* SVGFrame=nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate(); 
    }
    kid = kid->GetNextSibling();
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::RemoveFrame(nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  nsCOMPtr<nsISVGRendererRegion> dirty_region;

  nsISVGChildFrame* SVGFrame=nsnull;
  aOldFrame->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);

  if (SVGFrame)
    dirty_region = SVGFrame->GetCoveredRegion();
  
  PRBool result = mFrames.DestroyFrame(GetPresContext(), aOldFrame);

  nsISVGOuterSVGFrame* outerSVGFrame = GetOuterSVGFrame();
  NS_ASSERTION(outerSVGFrame, "no outer svg frame");

  if (SVGFrame && outerSVGFrame) {
    // XXX We need to rebuild the fragment tree starting from the
    // removed frame. Let's just rebuild the whole tree for now
    outerSVGFrame->SuspendRedraw();
    mFragmentTreeDirty = PR_TRUE;
    
    if (dirty_region) {
      outerSVGFrame->InvalidateRegion(dirty_region, PR_FALSE);
    }

    outerSVGFrame->UnsuspendRedraw();
  }
  
  NS_ASSERTION(result, "didn't find frame to delete");
  return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSVGTSpanFrame::ReplaceFrame(nsIAtom*        aListName,
                              nsIFrame*       aOldFrame,
                              nsIFrame*       aNewFrame)
{
  NS_NOTYETIMPLEMENTED("nsSVGTSpanFrame::ReplaceFrame");
  return NS_ERROR_NOT_IMPLEMENTED;
}

//----------------------------------------------------------------------
// nsISVGValueObserver methods:

NS_IMETHODIMP
nsSVGTSpanFrame::WillModifySVGObservable(nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  return NS_OK;
}


NS_IMETHODIMP
nsSVGTSpanFrame::DidModifySVGObservable (nsISVGValue* observable,
                                         nsISVGValue::modificationType aModType)
{
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->NotifyCanvasTMChanged(PR_FALSE); // XXX
    kid = kid->GetNextSibling();
  }  
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGTSpanFrame::PaintSVG(nsISVGRendererCanvas* canvas,
                          const nsRect& dirtyRectTwips,
                          PRBool ignoreFilter)
{
#ifdef DEBUG
//  printf("nsSVGTSpanFrame(%p)::Paint\n", this);
#endif

  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame)
      SVGFrame->PaintSVG(canvas, dirtyRectTwips, PR_FALSE);
    kid = kid->GetNextSibling();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetFrameForPointSVG(float x, float y, nsIFrame** hit)
{
#ifdef DEBUG
//  printf("nsSVGTSpanFrame(%p)::GetFrameForPoint\n", this);
#endif
  *hit = nsnull;
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsIFrame* temp=nsnull;
      nsresult rv = SVGFrame->GetFrameForPointSVG(x, y, &temp);
      if (NS_SUCCEEDED(rv) && temp) {
        *hit = temp;
        // return NS_OK; can't return. we need reverse order but only
        // have a singly linked list...
      }
    }
    kid = kid->GetNextSibling();
  }
  
  return *hit ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(already_AddRefed<nsISVGRendererRegion>)
nsSVGTSpanFrame::GetCoveredRegion()
{
  nsISVGRendererRegion *accu_region=nsnull;
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      nsCOMPtr<nsISVGRendererRegion> dirty_region = SVGFrame->GetCoveredRegion();
      if (accu_region) {
        if (dirty_region) {
          nsCOMPtr<nsISVGRendererRegion> temp = dont_AddRef(accu_region);
          dirty_region->Combine(temp, &accu_region);
        }
      }
      else {
        accu_region = dirty_region;
        NS_IF_ADDREF(accu_region);
      }
    }
    kid = kid->GetNextSibling();
  }
  
  return accu_region;
}

NS_IMETHODIMP
nsSVGTSpanFrame::InitialUpdate()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->InitialUpdate();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}  

NS_IMETHODIMP
nsSVGTSpanFrame::NotifyCanvasTMChanged(PRBool suppressInvalidation)
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyCanvasTMChanged(suppressInvalidation);
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::NotifyRedrawSuspended()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawSuspended();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::NotifyRedrawUnsuspended()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGChildFrame* SVGFrame=0;
    kid->QueryInterface(NS_GET_IID(nsISVGChildFrame),(void**)&SVGFrame);
    if (SVGFrame) {
      SVGFrame->NotifyRedrawUnsuspended();
    }
    kid = kid->GetNextSibling();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::SetMatrixPropagation(PRBool aPropagate)
{
  mPropagateTransform = aPropagate;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::SetOverrideCTM(nsIDOMSVGMatrix *aCTM)
{
  mOverrideCTM = aCTM;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetBBox(nsIDOMSVGRect **_retval)
{
  return nsSVGUtils::GetBBox(&mFrames, _retval);
}

//----------------------------------------------------------------------
// nsISVGContainerFrame methods:

nsISVGOuterSVGFrame *
nsSVGTSpanFrame::GetOuterSVGFrame()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetOuterSVGFrame();  
}

already_AddRefed<nsIDOMSVGMatrix>
nsSVGTSpanFrame::GetCanvasTM()
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

  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCanvasTM();  
}

already_AddRefed<nsSVGCoordCtxProvider>
nsSVGTSpanFrame::GetCoordContextProvider()
{
  NS_ASSERTION(mParent, "null parent");
  
  nsISVGContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    NS_ERROR("invalid container");
    return nsnull;
  }

  return containerFrame->GetCoordContextProvider();  
}


//----------------------------------------------------------------------
// nsISVGTextContainerFrame methods:

NS_IMETHODIMP_(nsISVGTextFrame *)
nsSVGTSpanFrame::GetTextFrame()
{
  NS_ASSERTION(mParent, "null parent");
  nsISVGTextContainerFrame *containerFrame;
  mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void**)&containerFrame);
  if (!containerFrame) {
    return nsnull;
  }

  return containerFrame->GetTextFrame();  
}

NS_IMETHODIMP_(PRBool)
nsSVGTSpanFrame::GetAbsolutePositionAdjustmentX(float &x, PRUint32 charNum)
{
  return PR_FALSE;  
}

NS_IMETHODIMP_(PRBool)
nsSVGTSpanFrame::GetAbsolutePositionAdjustmentY(float &y, PRUint32 charNum)
{
  return PR_FALSE;    
}

NS_IMETHODIMP_(PRBool)
nsSVGTSpanFrame::GetRelativePositionAdjustmentX(float &dx, PRUint32 charNum)
{
  return PR_FALSE;    
}

NS_IMETHODIMP_(PRBool)
nsSVGTSpanFrame::GetRelativePositionAdjustmentY(float &dy, PRUint32 charNum)
{
  return PR_FALSE;    
}


//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode methods:

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGTSpanFrame::GetFirstGlyphFragment()
{
  // try children first:
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode),(void**)&node);
    if (node)
      return node->GetFirstGlyphFragment();
    kid = kid->GetNextSibling();
  }

  // nope. try siblings:
  return GetNextGlyphFragment();

}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGTSpanFrame::GetNextGlyphFragment()
{
  nsIFrame* sibling = mNextSibling;
  while (sibling) {
    nsISVGGlyphFragmentNode *node = nsnull;
    sibling->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(mParent, "null parent");
  nsISVGGlyphFragmentNode *node = nsnull;
  mParent->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(PRUint32)
nsSVGTSpanFrame::BuildGlyphFragmentTree(PRUint32 charNum, PRBool lastBranch)
{
  mCharOffset = charNum;

  // init children:
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  nsISVGGlyphFragmentNode* next;
  while (node) {
    next = GetNextGlyphFragmentChildNode(node);
    charNum = node->BuildGlyphFragmentTree(charNum, lastBranch && !next);
    node = next;
  }
  
  return charNum;
}


NS_IMETHODIMP_(void)
nsSVGTSpanFrame::NotifyMetricsSuspended()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      node->NotifyMetricsSuspended();
    kid = kid->GetNextSibling();
  }
}

NS_IMETHODIMP_(void)
nsSVGTSpanFrame::NotifyMetricsUnsuspended()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      node->NotifyMetricsUnsuspended();
    kid = kid->GetNextSibling();
  }
}

NS_IMETHODIMP_(void)
nsSVGTSpanFrame::NotifyGlyphFragmentTreeSuspended()
{
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      node->NotifyGlyphFragmentTreeSuspended();
    kid = kid->GetNextSibling();
  }
}

NS_IMETHODIMP_(void)
nsSVGTSpanFrame::NotifyGlyphFragmentTreeUnsuspended()
{
  if (mFragmentTreeDirty) {
    nsISVGTextFrame* text_frame = GetTextFrame();
    NS_ASSERTION(text_frame, "null text frame");
    if (text_frame)
      text_frame->NotifyGlyphFragmentTreeChange(this);
    mFragmentTreeDirty = PR_FALSE;
  }    
  
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    kid->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode), (void**)&node);
    if (node)
      node->NotifyGlyphFragmentTreeUnsuspended();
    kid = kid->GetNextSibling();
  }
}


//----------------------------------------------------------------------
//

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTSpanFrame::GetX()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  if (!tpElement)
    return nsnull;

  if (!mContent->HasAttr(kNameSpaceID_None, nsSVGAtoms::x)) {
    nsISVGTextContainerFrame *parent;
    mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void **)&parent);
    if (parent)
      return parent->GetX();
    else
      return nsnull;
  }
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetX(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTSpanFrame::GetY()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  if (!tpElement)
    return nsnull;

  if (!mContent->HasAttr(kNameSpaceID_None, nsSVGAtoms::y)) {
    nsISVGTextContainerFrame *parent;
    mParent->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame), (void **)&parent);
    if (parent)
      return parent->GetY();
    else
      return nsnull;
  }
  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetY(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTSpanFrame::GetDx()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  if (!tpElement)
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDx(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

NS_IMETHODIMP_(already_AddRefed<nsIDOMSVGLengthList>)
nsSVGTSpanFrame::GetDy()
{
  nsCOMPtr<nsIDOMSVGTextPositioningElement> tpElement = do_QueryInterface(mContent);
  if (!tpElement)
    return nsnull;

  nsCOMPtr<nsIDOMSVGAnimatedLengthList> animLengthList;
  tpElement->GetDy(getter_AddRefs(animLengthList));
  nsIDOMSVGLengthList *retval;
  animLengthList->GetAnimVal(&retval);
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTSpanFrame::GetFirstGlyphFragmentChildNode()
{
  nsISVGGlyphFragmentNode* retval = nsnull;
  nsIFrame* frame = mFrames.FirstChild();
  while (frame) {
    frame->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode),(void**)&retval);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}

nsISVGGlyphFragmentNode *
nsSVGTSpanFrame::GetNextGlyphFragmentChildNode(nsISVGGlyphFragmentNode*node)
{
  nsISVGGlyphFragmentNode* retval = nsnull;
  nsIFrame* frame = nsnull;
  node->QueryInterface(NS_GET_IID(nsIFrame), (void**)&frame);
  NS_ASSERTION(frame, "interface not implemented");
  frame = frame->GetNextSibling();
  while (frame) {
    frame->QueryInterface(NS_GET_IID(nsISVGGlyphFragmentNode),(void**)&retval);
    if (retval) break;
    frame = frame->GetNextSibling();
  }
  return retval;
}
