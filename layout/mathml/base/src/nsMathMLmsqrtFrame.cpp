/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Vilya Harvey <vilya@nag.co.uk>
 */


#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsIHTMLContent.h"
#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsHTMLAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsStyleUtil.h"

#include "nsMathMLmsqrtFrame.h"

//
// <msqrt> and <mroot> -- form a radical - implementation
//

nsresult
NS_NewMathMLmsqrtFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsqrtFrame* it = new nsMathMLmsqrtFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsqrtFrame::nsMathMLmsqrtFrame() :
  mSqrChar(),
  mBarChar()
{
   nsAutoString sqr(PRUnichar(0x221A)),
                bar(PRUnichar(0xF8E5));
   mSqrChar.SetData(sqr);
   mBarChar.SetData(bar);
}

nsMathMLmsqrtFrame::~nsMathMLmsqrtFrame()
{
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Init(nsIPresContext*  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;
  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent,
                                    aContext, aPrevInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // TODO: other attributes...
  return rv;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;
  
  /////////////
  // paint the content we are square-rooting
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);

  if (NS_SUCCEEDED(rv) && NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    // paint the sqrt symbol
    rv = mSqrChar.Paint(aPresContext, aRenderingContext, mStyleContext);
    // paint the overline bar
    rv = mBarChar.Paint(aPresContext, aRenderingContext, mStyleContext);
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  ///////////////
  // Let the base class format our content like an inferred mrow
  rv = nsMathMLContainerFrame::Reflow(aPresContext, aDesiredSize,
                                      aReflowState, aStatus);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  if (NS_FAILED(rv)) {
    return rv;
  }
  nscoord oldAscent = aDesiredSize.ascent;

  ////////////
  // Prepare the radical symbol and the overline bar
  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  renderingContext.SetFont(font.mFont);

  // grab some metrics that will help to adjust the placements
  nsCOMPtr<nsIFontMetrics> fm;
  nscoord fontAscent, fontDescent; 
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(fontAscent);
  fm->GetMaxDescent(fontDescent);

  nsBoundingMetrics bmSqr, bmBar;
  nscoord dx, dy;

  // radical symbol  
  renderingContext.GetBoundingMetrics(mSqrChar.GetUnicode(), PRUint32(1), bmSqr);
  nscoord charWidth = bmSqr.width; // width of the radical symbol

  // overline bar
  renderingContext.GetBoundingMetrics(mBarChar.GetUnicode(), PRUint32(1), bmBar);
  nscoord thickspace = bmBar.ascent - bmBar.descent; // height of the overline bar

  // Stretch the sqrt symbol to the appropriate height if it is not big enough.
  nsCharMetrics contSize(aDesiredSize);
  nsCharMetrics desSize(fontDescent, fontAscent, charWidth, fontAscent + fontDescent);
  mSqrChar.Stretch(aPresContext, renderingContext, mStyleContext,
                    NS_STRETCH_DIRECTION_VERTICAL, contSize, desSize);
  charWidth = desSize.width;
  if (aDesiredSize.ascent < desSize.ascent) {
    aDesiredSize.ascent = desSize.ascent;
  }
  if (aDesiredSize.descent < desSize.descent) {
    aDesiredSize.descent = desSize.descent;
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  dx = 0;
  dy = bmSqr.ascent - fontAscent;
  mSqrChar.SetRect(nsRect(dx, dy, desSize.width, aDesiredSize.height));
  dx = bmSqr.rightBearing;

  // Stretch the overline bar to the appropriate width if it is not big enough.
  contSize = nsCharMetrics(aDesiredSize);
  desSize = nsCharMetrics(fontDescent, fontAscent, bmBar.rightBearing-bmBar.leftBearing, fontAscent + fontDescent);
  nsCharMetrics oldSize = desSize;
  mBarChar.Stretch(aPresContext, renderingContext, mStyleContext,
                   NS_STRETCH_DIRECTION_HORIZONTAL, contSize, desSize);

  dy = bmBar.ascent - fontAscent;
  if (oldSize == desSize) { // hasn't changed size! Char will be painted as a normal char
    dx -= bmBar.leftBearing; // adjust so that it coincides with the sqrt char
  } 
  dy = bmBar.ascent - fontAscent;
  mBarChar.SetRect(nsRect(dx, dy, desSize.width, thickspace));

  // Update the size of the container
  aDesiredSize.width += charWidth;
  aDesiredSize.ascent += thickspace;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  //////////////////
  // Place Children now by setting the origin of each child.

  nsRect rect;
  dx = charWidth;
  dy = aDesiredSize.ascent - oldAscent;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      childFrame->MoveTo(aPresContext, dx, rect.y + dy);
      dx += rect.width;
    }
    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

