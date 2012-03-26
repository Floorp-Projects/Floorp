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

// Main header first:
#include "nsSVGTSpanFrame.h"

// Keep others in (case-insensitive) order:
#include "nsIDOMSVGTSpanElement.h"
#include "nsIDOMSVGAltGlyphElement.h"
#include "nsSVGUtils.h"

//----------------------------------------------------------------------
// Implementation

nsIFrame*
NS_NewSVGTSpanFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsSVGTSpanFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsSVGTSpanFrame)

nsIAtom *
nsSVGTSpanFrame::GetType() const
{
  return nsGkAtoms::svgTSpanFrame;
}

//----------------------------------------------------------------------
// nsQueryFrame methods

NS_QUERYFRAME_HEAD(nsSVGTSpanFrame)
  NS_QUERYFRAME_ENTRY(nsISVGGlyphFragmentNode)
NS_QUERYFRAME_TAIL_INHERITING(nsSVGTSpanFrameBase)

//----------------------------------------------------------------------
// nsIFrame methods

#ifdef DEBUG
NS_IMETHODIMP
nsSVGTSpanFrame::Init(nsIContent* aContent,
                      nsIFrame* aParent,
                      nsIFrame* aPrevInFlow)
{
  NS_ASSERTION(aParent, "null parent");

  // Some of our subclasses have an aContent that's not a <svg:tspan> or are
  // allowed to be constructed even when there is no nsISVGTextContentMetrics
  // ancestor.  For example, nsSVGAFrame inherits from us but may have nothing
  // to do with text.
  if (GetType() == nsGkAtoms::svgTSpanFrame) {
    nsIFrame* ancestorFrame = nsSVGUtils::GetFirstNonAAncestorFrame(aParent);
    NS_ASSERTION(ancestorFrame, "Must have ancestor");

    nsSVGTextContainerFrame *metrics = do_QueryFrame(ancestorFrame);
    NS_ASSERTION(metrics,
                 "trying to construct an SVGTSpanFrame for an invalid "
                 "container");

    nsCOMPtr<nsIDOMSVGTSpanElement> tspan = do_QueryInterface(aContent);
    nsCOMPtr<nsIDOMSVGAltGlyphElement> altGlyph = do_QueryInterface(aContent);
    NS_ASSERTION(tspan || altGlyph, "Content is not an SVG tspan or altGlyph");
  }

  return nsSVGTSpanFrameBase::Init(aContent, aParent, aPrevInFlow);
}
#endif /* DEBUG */

NS_IMETHODIMP
nsSVGTSpanFrame::AttributeChanged(PRInt32         aNameSpaceID,
                                  nsIAtom*        aAttribute,
                                  PRInt32         aModType)
{
  if (aNameSpaceID == kNameSpaceID_None &&
      (aAttribute == nsGkAtoms::x ||
       aAttribute == nsGkAtoms::y ||
       aAttribute == nsGkAtoms::dx ||
       aAttribute == nsGkAtoms::dy ||
       aAttribute == nsGkAtoms::rotate)) {
    NotifyGlyphMetricsChange();
  }

  return NS_OK;
}

//----------------------------------------------------------------------
// nsSVGContainerFrame methods:

gfxMatrix
nsSVGTSpanFrame::GetCanvasTM()
{
  NS_ASSERTION(mParent, "null parent");
  return static_cast<nsSVGContainerFrame*>(mParent)->GetCanvasTM();  
}

//----------------------------------------------------------------------
// nsISVGGlyphFragmentNode methods:

PRUint32
nsSVGTSpanFrame::GetNumberOfChars()
{
  return nsSVGTSpanFrameBase::GetNumberOfChars();
}

float
nsSVGTSpanFrame::GetComputedTextLength()
{
  return nsSVGTSpanFrameBase::GetComputedTextLength();
}

float
nsSVGTSpanFrame::GetSubStringLength(PRUint32 charnum, PRUint32 nchars)
{
  return nsSVGTSpanFrameBase::GetSubStringLength(charnum, nchars);
}

PRInt32
nsSVGTSpanFrame::GetCharNumAtPosition(nsIDOMSVGPoint *point)
{
  return nsSVGTSpanFrameBase::GetCharNumAtPosition(point);
}

NS_IMETHODIMP_(nsSVGGlyphFrame *)
nsSVGTSpanFrame::GetFirstGlyphFrame()
{
  // try children first:
  nsIFrame* kid = mFrames.FirstChild();
  while (kid) {
    nsISVGGlyphFragmentNode *node = do_QueryFrame(kid);
    if (node)
      return node->GetFirstGlyphFrame();
    kid = kid->GetNextSibling();
  }

  // nope. try siblings:
  return GetNextGlyphFrame();

}

NS_IMETHODIMP_(nsSVGGlyphFrame *)
nsSVGTSpanFrame::GetNextGlyphFrame()
{
  nsIFrame* sibling = GetNextSibling();
  while (sibling) {
    nsISVGGlyphFragmentNode *node = do_QueryFrame(sibling);
    if (node)
      return node->GetFirstGlyphFrame();
    sibling = sibling->GetNextSibling();
  }

  // no more siblings. go back up the tree.
  
  NS_ASSERTION(GetParent(), "null parent");
  nsISVGGlyphFragmentNode *node = do_QueryFrame(GetParent());
  return node ? node->GetNextGlyphFrame() : nsnull;
}

NS_IMETHODIMP_(void)
nsSVGTSpanFrame::SetWhitespaceCompression(bool)
{
  nsSVGTSpanFrameBase::SetWhitespaceCompression();
}
