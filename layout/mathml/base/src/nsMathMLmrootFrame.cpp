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

#include "nsMathMLmrootFrame.h"

//
// <msqrt> and <mroot> -- form a radical
//

nsresult
NS_NewMathMLmrootFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmrootFrame* it = new nsMathMLmrootFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;  
  return NS_OK;
}

nsMathMLmrootFrame::nsMathMLmrootFrame() :
  mSqrChar(),
  mBarChar()
{
   nsAutoString sqr(PRUnichar(0x221A)),
                bar(PRUnichar(0xF8E5));
   mSqrChar.SetData(sqr);
   mBarChar.SetData(bar);
}

nsMathMLmrootFrame::~nsMathMLmrootFrame()
{
}

NS_IMETHODIMP
nsMathMLmrootFrame::Init(nsIPresContext*  aPresContext,
                        nsIContent*      aContent,
                        nsIFrame*        aParent,
                        nsIStyleContext* aContext,
                        nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  // XXX Attributes?
  return rv;
}

NS_IMETHODIMP
nsMathMLmrootFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer)
{
    /////////////
  // paint the base and the index
  nsresult rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
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
nsMathMLmrootFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  nsReflowStatus childStatus;
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);

  //////////////////
  // Reflow Children

  PRInt32 count = 0;
  nsIFrame* child[2];
  nsRect rect[4];
  aDesiredSize.width = aDesiredSize.height = aDesiredSize.ascent = aDesiredSize.descent = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) 
  {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);
    }
    else if (2 > count)  {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext,
                       childDesiredSize, childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (0 == count) {
        aDesiredSize.descent = childDesiredSize.descent;
        aDesiredSize.ascent = childDesiredSize.ascent;
        aDesiredSize.width = childDesiredSize.width;
      }
      // At this stage, the origin points of the children have no use, so we
      // will use the origins as placeholders to store the child's ascent and
      // descent. Before return, we should set the origins so as to overwrite
      // what we are storing there now.
      child[count] = childFrame;
      rect[count] = nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                           childDesiredSize.width, childDesiredSize.height);
      count++;
    }
//  else { invalid markup... }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  ////////////
  // Prepare the radical symbol and the overline bar

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
  nsStyleFont font;
  mStyleContext->GetStyle(eStyleStruct_Font, font);
  renderingContext.SetFont(font.mFont);

  // grab some metrics to help adjusting the placements
  nsCOMPtr<nsIFontMetrics> fm;
  nscoord fontAscent, fontDescent; 
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(fontAscent);
  fm->GetMaxDescent(fontDescent);

  nsBoundingMetrics bmSqr, bmBar;
  nscoord dx, dy;

  // radical symbol  
  renderingContext.GetBoundingMetrics(mSqrChar.GetUnicode(), PRUint32(1), bmSqr);
  nscoord widthSqr = bmSqr.width; // width of the radical symbol

  // overline bar
  renderingContext.GetBoundingMetrics(mBarChar.GetUnicode(), PRUint32(1), bmBar);
  nscoord heightBar = bmBar.ascent - bmBar.descent; // height of the overline bar

  // Stretch the sqrt symbol to the appropriate height if it is not big enough.
  nsCharMetrics contSize(aDesiredSize);
  nsCharMetrics desSize(fontDescent, fontAscent, widthSqr, fontAscent + fontDescent);
  mSqrChar.Stretch(aPresContext, renderingContext, mStyleContext,
                    NS_STRETCH_DIRECTION_VERTICAL, contSize, desSize);
  widthSqr = desSize.width;
  if (aDesiredSize.ascent < desSize.ascent) {
    aDesiredSize.ascent = desSize.ascent;
  }
  if (aDesiredSize.descent < desSize.descent) {
    aDesiredSize.descent = desSize.descent;
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  dx = 0;
  dy = bmSqr.ascent - fontAscent;
  rect[2] = nsRect(dx, dy, desSize.width, aDesiredSize.height);
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
  rect[3] = nsRect(dx, dy, desSize.width, heightBar);

  //////////////////
  // Place Children now by setting the origin of each child.

  nscoord thickspace = bmBar.ascent-bmBar.descent; // height of the bar
  aDesiredSize.ascent += thickspace;
  rect[0].y = aDesiredSize.ascent - rect[0].y;  // y-origin of base
  dy = thickspace + (bmSqr.ascent-bmSqr.descent)/3 - rect[1].height;
  rect[1].y = dy;
  if (0 > dy) {
    dy = -dy;
    aDesiredSize.ascent += dy;
    rect[0].y += dy;
    rect[1].y = 0;
    rect[2].y += dy; // y-origin of bar
    rect[3].y += dy; // y-origin of sqrt char
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  rect[1].x = 0; // x-origin of index
  rect[0].x = widthSqr;
  aDesiredSize.width = widthSqr + rect[0].width;
  if (0 < rect[1].width) {
    dx = rect[1].width - (bmSqr.rightBearing - bmSqr.leftBearing)/2;
    aDesiredSize.width += dx;    
    rect[0].x += dx; // x-origin of base
    rect[2].x += dx; // x-origin of sqrt char
    rect[3].x += dx; // x-origin of bar
  }
 
  child[0]->SetRect(aPresContext, rect[0]);
  child[1]->SetRect(aPresContext, rect[1]);
  mSqrChar.SetRect(rect[2]);
  mBarChar.SetRect(rect[3]);

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}
