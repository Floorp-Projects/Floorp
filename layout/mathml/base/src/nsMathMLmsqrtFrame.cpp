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
  mSqrtChar(),
  mSqrtBar()
{
   mSqrtChar.SetData(nsAutoString(PRUnichar(0x221A)));
   mSqrtBar.SetData(nsAutoString(PRUnichar(0xF8E5)));
}

nsMathMLmsqrtFrame::~nsMathMLmsqrtFrame()
{
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Init(nsIPresContext&  aPresContext,
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
nsMathMLmsqrtFrame::Paint(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;
  
  /////////////
  // paint the content we are square-rooting
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);

  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return rv;
  }

  if (NS_SUCCEEDED(rv)) {
    // paint the sqrt symbol
    rv = mSqrtChar.Paint(aPresContext, aRenderingContext, mStyleContext);
    // paint the overline bar
    rv = mSqrtBar.Paint(aPresContext, aRenderingContext, mStyleContext);
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Reflow(nsIPresContext&          aPresContext,
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

  nsRect rect;
  aDesiredSize.width = 0;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) 
  {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      ReflowEmptyChild(aPresContext, childFrame);
    }
    else {
      nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                         childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext,
                       childDesiredSize, childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }

      // At this stage, the origin points of the children have no use, so we
      // will use the origins as placeholders to store the child's ascent and
      // descent. Before return, we should set the origins so as to overwrite
      // what we are storing there now.
      childFrame->SetRect(&aPresContext,
                          nsRect(childDesiredSize.descent, childDesiredSize.ascent,
                                 childDesiredSize.width, childDesiredSize.height));

      aDesiredSize.width += childDesiredSize.width;
      if (aDesiredSize.ascent < childDesiredSize.ascent) {
        aDesiredSize.ascent = childDesiredSize.ascent;
      }
      if (aDesiredSize.descent < childDesiredSize.descent) {
        aDesiredSize.descent = childDesiredSize.descent;
      }
    }
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
  nscoord ascent, descent; 
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));
  fm->GetMaxAscent(ascent);
  fm->GetMaxDescent(descent);

  nsAutoString aData;
  nsBoundingMetrics bm, bmdata[2];

  // radical symbol  
  mSqrtChar.GetData(aData);
  renderingContext.GetBoundingMetrics(aData.GetUnicode(), PRUint32(1), bm);
  bmdata[0] = bm;
  nscoord charWidth = bm.width; // width of the radical symbol
  nscoord charBearing = bm.rightBearing;

  // overline bar
  mSqrtBar.GetData(aData);
  renderingContext.GetBoundingMetrics(aData.GetUnicode(), PRUint32(1), bm);
  bmdata[1] = bm;
  nscoord thickspace = bm.ascent - bm.descent; // height of the overline bar

  // Stretch the sqrt symbol to the appropriate height if it is not big enough.
  nsCharMetrics contSize(aDesiredSize);
  nsCharMetrics desSize(descent, ascent, charWidth, ascent + descent);
  mSqrtChar.Stretch(aPresContext, renderingContext, mStyleContext,
                    NS_STRETCH_DIRECTION_VERTICAL, contSize, desSize);
  charWidth = desSize.width;
  if (aDesiredSize.ascent < desSize.ascent) {
    aDesiredSize.ascent = desSize.ascent;
  }
  if (aDesiredSize.descent < desSize.descent) {
    aDesiredSize.descent = desSize.descent;
  }
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  mSqrtChar.SetRect(nsRect(0, thickspace, desSize.width, aDesiredSize.height));

  // Stretch the overline bar to the appropriate width if it is not big enough.
  contSize = nsCharMetrics(aDesiredSize);
  desSize = nsCharMetrics(descent, ascent, bmdata[1].rightBearing-bmdata[1].leftBearing, ascent + descent);
  mSqrtBar.Stretch(aPresContext, renderingContext, mStyleContext,
                   NS_STRETCH_DIRECTION_HORIZONTAL, contSize, desSize);
  nscoord dy = bmdata[1].ascent - ascent;
  mSqrtBar.SetRect(nsRect(charBearing, dy, desSize.width, thickspace));

  // Update the size of the container
  aDesiredSize.width += charWidth;
  aDesiredSize.ascent += thickspace;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  //////////////////
  // Place Children now by setting the origin of each child.
  nscoord chX = charWidth;
  nscoord chY; // = thickspace;
  childFrame = mFrames.FirstChild();
  while (nsnull != childFrame) {
    if (!IsOnlyWhitespace(childFrame)) {
      childFrame->GetRect(rect);
      // rect.y was storing the child's ascent, from earlier.
      chY = aDesiredSize.ascent - rect.y;
      childFrame->MoveTo(&aPresContext, chX, chY);
      chX += rect.width;
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

