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

#include "nsMathMLAtoms.h"
#include "nsMathMLParts.h"
#include "nsMathMLmfracFrame.h"

//
// <mfrac> -- form a fraction from two subexpressions - implementation
//

nsresult
NS_NewMathMLmfracFrame(nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmfracFrame* it = new nsMathMLmfracFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmfracFrame::nsMathMLmfracFrame()
{
}

nsMathMLmfracFrame::~nsMathMLmfracFrame()
{
}

NS_IMETHODIMP
nsMathMLmfracFrame::Init(nsIPresContext&  aPresContext,
                         nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIStyleContext* aContext,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = NS_OK;

  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nscoord aLineThickness = DEFAULT_FRACTION_LINE_THICKNESS;
  nsAutoString value;
  
  // see if the linethickness attribute is there  
  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_None, 
                   nsMathMLAtoms::linethickness, value))
  {
    if (value == "thin")
      aLineThickness = THIN_FRACTION_LINE_THICKNESS;
    else if (value == "medium")
      aLineThickness = MEDIUM_FRACTION_LINE_THICKNESS;
    else if (value == "thick")
      aLineThickness = THICK_FRACTION_LINE_THICKNESS;
    else {
      PRInt32 aErrorCode;
      PRInt32 aMultiplier = value.ToInteger(&aErrorCode);
      if (NS_SUCCEEDED(aErrorCode))
        aLineThickness = aMultiplier * DEFAULT_FRACTION_LINE_THICKNESS;  	
      else {
        // XXX TODO: try to see if it is a h/v-unit like 1ex, 2px, 1em
        // see also nsUnitConversion.h
      }
    }
  }

//  mLineThickness = (aLineThickness > MAX_FRACTION_LINE_THICKNESS) ? 
//                   MAX_FRACTION_LINE_THICKNESS : aLineThickness;

  mLineThickness = aLineThickness;
  mLineOrigin.x = 0;
  mLineOrigin.y = 0;  

  // TODO: other attributes like displaystyle...
  return rv;
}

NS_IMETHODIMP
nsMathMLmfracFrame::Paint(nsIPresContext&      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer)
{
  nsresult rv = NS_OK;

  if (NS_FRAME_PAINT_LAYER_FOREGROUND != aWhichLayer) {
    return rv;
  }
  
  /////////////
  // paint the numerator and denominator
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);
  ////////////
  // paint the fraction line
  if (NS_SUCCEEDED(rv) && 0 < mLineThickness) {
    float p2t;
    aPresContext.GetScaledPixelsToTwips(&p2t);
    nscoord thickness = NSIntPixelsToTwips(mLineThickness, p2t); 
/*
//  line looking like <hr noshade>
    const nsStyleColor* color;
    nscolor colors[2]; 
    color = nsStyleUtil::FindNonTransparentBackground(mStyleContext); 
    NS_Get3DColors(colors, color->mBackgroundColor); 
    aRenderingContext.SetColor(colors[0]); 
*/
//  solid line with the current text color
    const nsStyleColor* color =
      (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);
    aRenderingContext.SetColor(color->mColor);

//  draw the line  
    aRenderingContext.FillRect(mLineOrigin.x, mLineOrigin.y, mRect.width, thickness);
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmfracFrame::Reflow(nsIPresContext&          aPresContext,
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
   
  nscoord count = 0;
  nsRect rect[2];
  nsIFrame* child[2];
  nsIFrame* childFrame = mFrames.FirstChild(); 
  while (nsnull != childFrame) 
  {
    //////////////
    // WHITESPACE: don't forget that whitespace doesn't count in MathML!
    if (IsOnlyWhitespace(childFrame)) {
      childFrame->SetRect(nsRect(0,0,0,0));
    }
    else if (2 > count)  {

      nsHTMLReflowState childReflowState(aPresContext, aReflowState, childFrame, availSize);
      rv = ReflowChild(childFrame, aPresContext, childDesiredSize, childReflowState, childStatus);
      NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
      if (NS_FAILED(rv)) {
        return rv;
      }

      child[count] = childFrame;     
      rect[count].width = childDesiredSize.width;
      rect[count].height = childDesiredSize.height;
      count++;  
    }
//  else { invalid markup... }

    rv = childFrame->GetNextSibling(&childFrame);
    NS_ASSERTION(NS_SUCCEEDED(rv),"failed to get next child");
  }

  //////////////////
  // Place Children 
  
  // Get the <strike> line and center the fraction bar with the <strike> line. 
  nscoord strikeOffset, strikeThickness;
  nsCOMPtr<nsIFontMetrics> fm;
  const nsStyleFont* aFont =
    (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  aPresContext.GetMetricsFor(aFont->mFont, getter_AddRefs(fm));
  fm->GetStrikeout(strikeOffset, strikeThickness);

  // Take care of mLineThickness
  float p2t;
  nscoord Thickspace, halfThickspace;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  if (mLineThickness <= 1) {
    Thickspace = 0; 		
    halfThickspace = 0;  	
  }
  else {
    Thickspace = NSIntPixelsToTwips(mLineThickness-1, p2t); 		
    halfThickspace = Thickspace/2;  // distance from the middle of the axis
  }

  aDesiredSize.width = PR_MAX(rect[0].width, rect[1].width);
  aDesiredSize.ascent = rect[0].height + strikeOffset + halfThickspace;
  aDesiredSize.descent = rect[1].height - strikeOffset + halfThickspace;
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;

  rect[0].x = (aDesiredSize.width - rect[0].width) / 2; // center w.r.t largest width
  rect[1].x = (aDesiredSize.width - rect[1].width) / 2;
  rect[0].y = 0;
  rect[1].y = aDesiredSize.height - rect[1].height;
  
  child[0]->SetRect(rect[0]);
  child[1]->SetRect(rect[1]); 
  SetLineOrigin(nsPoint(0,rect[0].height)); // position the fraction bar  

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

