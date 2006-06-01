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
#include "nsDOMError.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                    nsIFrame* parentFrame, nsStyleContext* aContext)
{
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContainerFrame *text_container;
  parentFrame->QueryInterface(NS_GET_IID(nsISVGTextContainerFrame),
                              (void**)&text_container);
  if (!text_container) {
    NS_ERROR("trying to construct an SVGTSpanFrame for an invalid container");
    return nsnull;
  }
  
  nsCOMPtr<nsIDOMSVGTSpanElement> tspan_elem = do_QueryInterface(aContent);
  if (!tspan_elem) {
    NS_ERROR("Trying to construct an SVGTSpanFrame for a "
             "content element that doesn't support the right interfaces");
    return nsnull;
  }

  return new (aPresShell) nsSVGTSpanFrame(aContext);
}

nsSVGTSpanFrame::nsSVGTSpanFrame(nsStyleContext* aContext)
  : nsSVGTSpanFrameBase(aContext),
    mCharOffset(0), mFragmentTreeDirty(PR_FALSE), mPropagateTransform(PR_TRUE)
{
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
  NS_INTERFACE_MAP_ENTRY(nsISVGTextContentMetrics)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanFrameBase)


//----------------------------------------------------------------------
// nsIFrame methods

NS_IMETHODIMP
nsSVGTSpanFrame::RemoveFrame(nsIAtom*        aListName,
                             nsIFrame*       aOldFrame)
{
  nsISVGOuterSVGFrame* outerSVGFrame = nsSVGUtils::GetOuterSVGFrame(this);
  if (outerSVGFrame)
    outerSVGFrame->SuspendRedraw();
  mFragmentTreeDirty = PR_TRUE;

  nsresult rv = nsSVGTSpanFrameBase::RemoveFrame(aListName, aOldFrame);
  
  if (outerSVGFrame)
    outerSVGFrame->UnsuspendRedraw();

  return rv;
}

NS_IMETHODIMP
nsSVGTSpanFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::dx ||
       aAttribute == nsGkAtoms::dy)) {
    nsISVGTextFrame* text_frame = GetTextFrame();
    if (text_frame)
      text_frame->NotifyGlyphMetricsChange(this);
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGTextContentMetrics
NS_IMETHODIMP
nsSVGTSpanFrame::GetNumberOfChars(PRInt32 *_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  if (node)
    return nsSVGUtils::GetNumberOfChars(node, _retval);

  *_retval = 0;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetComputedTextLength(float *_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  if (node)
    return nsSVGUtils::GetComputedTextLength(node, _retval);

  *_retval = 0.0;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars, float *_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  if (node)
    return nsSVGUtils::GetSubStringLength(node, charnum, nchars, _retval);

  *_retval = 0.0;
  return NS_OK;
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetStartPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node) return NS_ERROR_DOM_INDEX_SIZE_ERR;
  return nsSVGUtils::GetStartPositionOfChar(node, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetEndPositionOfChar(PRUint32 charnum, nsIDOMSVGPoint **_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node) return NS_ERROR_DOM_INDEX_SIZE_ERR;
  return nsSVGUtils::GetEndPositionOfChar(node, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetExtentOfChar(PRUint32 charnum, nsIDOMSVGRect **_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node) return NS_ERROR_DOM_INDEX_SIZE_ERR;
  return nsSVGUtils::GetExtentOfChar(node, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetRotationOfChar(PRUint32 charnum, float *_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();
  if (!node) return NS_ERROR_DOM_INDEX_SIZE_ERR;
  return nsSVGUtils::GetRotationOfChar(node, charnum, _retval);
}

NS_IMETHODIMP
nsSVGTSpanFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point, PRInt32 *_retval)
{
  nsISVGGlyphFragmentNode* node = GetFirstGlyphFragmentChildNode();

  if (node)
    return nsSVGUtils::GetCharNumAtPosition(node, point, _retval);

  *_retval = -1;
  return NS_OK;
}


//----------------------------------------------------------------------
// nsISVGChildFrame methods

NS_IMETHODIMP
nsSVGTSpanFrame::PaintSVG(nsISVGRendererCanvas* canvas)
{
  for (nsIFrame* kid = mFrames.FirstChild(); kid;
       kid = kid->GetNextSibling()) {
    nsISVGChildFrame* SVGFrame = nsnull;
    CallQueryInterface(kid, &SVGFrame);
    if (SVGFrame)
      SVGFrame->PaintSVG(canvas);
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

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

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
  nsSVGContainerFrame *containerFrame = NS_STATIC_CAST(nsSVGContainerFrame*,
                                                       mParent);
  return containerFrame->GetCanvasTM();  
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

  if (!mContent->HasAttr(kNameSpaceID_None, nsSVGAtoms::x))
      return nsnull;

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

  if (!mContent->HasAttr(kNameSpaceID_None, nsSVGAtoms::y))
      return nsnull;

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
