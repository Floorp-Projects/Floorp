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

#include "nsIDOMSVGTSpanElement.h"
#include "nsSVGTSpanFrame.h"
#include "nsSVGUtils.h"
#include "nsSVGTextFrame.h"
#include "nsSVGOuterSVGFrame.h"
#include "nsSVGMatrix.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsIContent* aContent,
                    nsIFrame* parentFrame, nsStyleContext* aContext)
{
  NS_ASSERTION(parentFrame, "null parent");
  nsISVGTextContentMetrics *metrics;
  CallQueryInterface(parentFrame, &metrics);
  if (!metrics) {
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

nsIAtom *
nsSVGTSpanFrame::GetType() const
{
  return nsGkAtoms::svgTSpanFrame;
}

//----------------------------------------------------------------------
// nsISupports methods

NS_INTERFACE_MAP_BEGIN(nsSVGTSpanFrame)
  NS_INTERFACE_MAP_ENTRY(nsISVGGlyphFragmentNode)
NS_INTERFACE_MAP_END_INHERITING(nsSVGTSpanFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

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
    UpdateGraphic();
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsISVGChildFrame methods

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
// nsISVGGlyphFragmentNode methods:

NS_IMETHODIMP_(PRUint32)
nsSVGTSpanFrame::GetNumberOfChars()
{
  return nsSVGTSpanFrameBase::GetNumberOfChars();
}

NS_IMETHODIMP_(float)
nsSVGTSpanFrame::GetComputedTextLength()
{
  return nsSVGTSpanFrameBase::GetComputedTextLength();
}

NS_IMETHODIMP_(float)
nsSVGTSpanFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars)
{
  return nsSVGTSpanFrameBase::GetSubStringLengthNoValidation(charnum, nchars);
}

NS_IMETHODIMP_(PRInt32)
nsSVGTSpanFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  return nsSVGTSpanFrameBase::GetCharNumAtPosition(point);
}

NS_IMETHODIMP_(nsISVGGlyphFragmentLeaf *)
nsSVGTSpanFrame::GetFirstGlyphFragment()
{
  // try children first:
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = nsnull;
    CallQueryInterface(kid, &node);
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
    CallQueryInterface(sibling, &node);
    if (node)
      return node->GetFirstGlyphFragment();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(mParent, "null parent");
  nsISVGGlyphFragmentNode *node = nsnull;
  CallQueryInterface(mParent, &node);
  return node ? node->GetNextGlyphFragment() : nsnull;
}

NS_IMETHODIMP_(void)
nsSVGTSpanFrame::SetWhitespaceHandling(PRUint8 aWhitespaceHandling)
{
  nsSVGTSpanFrameBase::SetWhitespaceHandling();
}
