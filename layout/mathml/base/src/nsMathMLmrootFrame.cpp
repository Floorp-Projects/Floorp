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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
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
// <msqrt> and <mroot> -- form a radical - implementation
//

//NOTE:
//  The code assumes that TeX fonts are picked.
//  There is not fall-back to draw the branches of the sqrt explicitly
//  in the case where TeX fonts are not there. In general, there is not
//  fall-back(s) in MathML when some (freely-downloadable) fonts are missing.
//  Otherwise, this will add much work and unnecessary complexity to the core
//  MathML  engine. Assuming that authors have the free fonts is part of the
//  deal. We are not responsible for cases of misconfigurations out there.

// additional style context to be used by our MathMLChar.
#define NS_SQR_CHAR_STYLE_CONTEXT_INDEX   0

static const PRUnichar kSqrChar = PRUnichar(0x221A);

nsresult
NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmrootFrame* it = new (aPresShell) nsMathMLmrootFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmrootFrame::nsMathMLmrootFrame() :
  mSqrChar(),
  mBarRect()
{
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
  nsresult rv = NS_OK;
  rv = nsMathMLContainerFrame::Init(aPresContext, aContent, aParent,
                                    aContext, aPrevInFlow);

  mEmbellishData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  nsAutoString sqrChar; sqrChar.Assign(kSqrChar);
  mSqrChar.SetData(aPresContext, sqrChar);
  ResolveMathMLCharStyle(aPresContext, mContent, mStyleContext, &mSqrChar);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
  mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif
  return rv;
}

NS_IMETHODIMP
nsMathMLmrootFrame::Paint(nsIPresContext*      aPresContext,
                          nsIRenderingContext& aRenderingContext,
                          const nsRect&        aDirtyRect,
                          nsFramePaintLayer    aWhichLayer,
                          PRUint32             aFlags)
{
  nsresult rv = NS_OK;

  /////////////
  // paint the content we are square-rooting
  rv = nsMathMLContainerFrame::Paint(aPresContext, aRenderingContext, 
                                     aDirtyRect, aWhichLayer);
  /////////////
  // paint the sqrt symbol
  if (!NS_MATHML_HAS_ERROR(mPresentationData.flags))
  {
    mSqrChar.Paint(aPresContext, aRenderingContext,
                   aDirtyRect, aWhichLayer, this);

    if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer)
    {
      // paint the overline bar
      const nsStyleColor *color = NS_STATIC_CAST(const nsStyleColor*,
        mStyleContext->GetStyleData(eStyleStruct_Color));
      aRenderingContext.SetColor(color->mColor);
      aRenderingContext.FillRect(mBarRect);
    }

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
    // for visual debug
    if (NS_MATHML_PAINT_BOUNDING_METRICS(mPresentationData.flags))
    {
      nsRect rect;
      mSqrChar.GetRect(rect);

      nsBoundingMetrics bm;
      mSqrChar.GetBoundingMetrics(bm);

      aRenderingContext.SetColor(NS_RGB(255,0,0));
      nscoord x = rect.x + bm.leftBearing;
      nscoord y = rect.y;
      nscoord w = bm.rightBearing - bm.leftBearing;
      nscoord h = bm.ascent + bm.descent;
      aRenderingContext.DrawRect(x,y,w,h);
    }
#endif
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
  // ask our children to compute their bounding metrics 
  nsHTMLReflowMetrics childDesiredSize(aDesiredSize.maxElementSize,
                      aDesiredSize.mFlags | NS_REFLOW_CALC_BOUNDING_METRICS);
  nsSize availSize(aReflowState.mComputedWidth, aReflowState.mComputedHeight);
  nsReflowStatus childStatus;

  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = aDesiredSize.descent = 0;

  nsBoundingMetrics bmSqr, bmBase, bmIndex;
  nsIRenderingContext& renderingContext = *aReflowState.rendContext;

  //////////////////
  // Reflow Children

  PRInt32 count = 0;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* indexFrame = nsnull;
  nsHTMLReflowMetrics baseSize(nsnull);
  nsHTMLReflowMetrics indexSize(nsnull);
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext,
                     childDesiredSize, childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) return rv;
    if (0 == count) {
      // base 
      baseFrame = childFrame;
      baseSize = childDesiredSize;
      bmBase = childDesiredSize.mBoundingMetrics;
    }
    else if (1 == count) {
      // index
      indexFrame = childFrame;
      indexSize = childDesiredSize;
      bmIndex = childDesiredSize.mBoundingMetrics;
    }
    count++;
    childFrame->GetNextSibling(&childFrame);
  }
  if (2 != count) {
    // report an error, encourage people to get their markups in order
    NS_WARNING("invalid markup");
    return ReflowError(aPresContext, renderingContext, aDesiredSize);
  }

  ////////////
  // Prepare the radical symbol and the overline bar

  const nsStyleFont *font = NS_STATIC_CAST(const nsStyleFont*,
    mStyleContext->GetStyleData(eStyleStruct_Font));
  renderingContext.SetFont(font->mFont);
  nsCOMPtr<nsIFontMetrics> fm;
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord ruleThickness, leading;
  GetRuleThickness(renderingContext, fm, ruleThickness);

  // get the leading to be left at the top of the resulting frame
  // this seems more reliable than using fm->GetLeading() on suspicious fonts               
  float em = float(font->mFont.size);
  leading = nscoord(0.2f * em); 

  // Rule 11, App. G, TeXbook
  // psi = clearance between rule and content
  nscoord phi = 0, psi = 0;
  if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    fm->GetXHeight(phi);
  else
    phi = ruleThickness;
  psi = ruleThickness + phi/4;
  
  // Stretch the radical symbol to the appropriate height if it is not big enough.
  nsBoundingMetrics contSize = bmBase;
  contSize.descent = bmBase.ascent + bmBase.descent + psi;
  contSize.ascent = ruleThickness;

  // height(radical) should be >= height(base) + psi + ruleThickness
  nsBoundingMetrics radicalSize;
  mSqrChar.Stretch(aPresContext, renderingContext,
                   NS_STRETCH_DIRECTION_VERTICAL, 
                   contSize, radicalSize,
                   NS_STRETCH_LARGER);
  // radicalSize have changed at this point, and should match with
  // the bounding metrics of the char
  mSqrChar.GetBoundingMetrics(bmSqr);

  // According to TeX, the ascent of the returned radical should be
  // the thickness of the overline
  ruleThickness = bmSqr.ascent;
  // make sure that the rule appears on on screen
  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  if (ruleThickness < onePixel) {
    ruleThickness = onePixel;
  }

  // adjust clearance psi to absorb any excess difference if any
  // in height between radical and content

  if (bmSqr.descent > (bmBase.ascent + bmBase.descent) + psi)
    psi = (psi + bmSqr.descent - (bmBase.ascent + bmBase.descent))/2;

  // Update the desired size for the container (like msqrt, index is not yet included)
  // the baseline will be that of the base.
  mBoundingMetrics.ascent = bmBase.ascent + psi + ruleThickness;
  mBoundingMetrics.descent = 
    PR_MAX(bmBase.descent, (bmSqr.descent - (bmBase.ascent + psi)));
  mBoundingMetrics.width = bmSqr.width + bmBase.width;
  mBoundingMetrics.leftBearing = bmSqr.leftBearing;
  mBoundingMetrics.rightBearing = bmSqr.width + 
    PR_MAX(bmBase.width, bmBase.rightBearing); // take also care of the rule

  aDesiredSize.ascent = mBoundingMetrics.ascent + leading;
  aDesiredSize.descent =
    PR_MAX(baseSize.descent, (mBoundingMetrics.descent + ruleThickness));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;

  /////////////
  // Re-adjust the desired size to include the index.
  
  // the index is raised by some fraction of the height
  // of the radical, see \mroot macro in App. B, TexBook
  nscoord raiseIndexDelta = NSToCoordRound(0.6f * (bmSqr.ascent + bmSqr.descent));
  nscoord indexRaisedAscent = mBoundingMetrics.ascent // top of radical 
    - (bmSqr.ascent + bmSqr.descent) // to bottom of radical
    + raiseIndexDelta + bmIndex.ascent + bmIndex.descent; // to top of raised index

  nscoord indexClearance = 0;
  if (mBoundingMetrics.ascent < indexRaisedAscent) {
    indexClearance = 
      indexRaisedAscent - mBoundingMetrics.ascent; // excess gap introduced by a tall index 
    mBoundingMetrics.ascent = indexRaisedAscent;
    aDesiredSize.ascent = mBoundingMetrics.ascent + leading;
    aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  }

  // the index is tucked in closer to the radical while making sure
  // that the kern does not make the index and radical collide
  nscoord dxIndex, dxSqr, dx, dy;
  nscoord xHeight = 0;
  fm->GetXHeight(xHeight);
  nscoord indexRadicalKern = NSToCoordRound(1.35f * xHeight);
  if (indexRadicalKern > bmIndex.width) {
    dxIndex = indexRadicalKern - bmIndex.width;
    dxSqr = 0;
  }
  else {
    dxIndex = 0;
    dxSqr = bmIndex.width - indexRadicalKern;
  }
  // avoid collision by leaving a minimun space between index and radical
  nscoord minimumClearance = bmSqr.width/2;
  if (dxIndex + bmIndex.width + minimumClearance > dxSqr + bmSqr.width) {
    if (bmIndex.width + minimumClearance < bmSqr.width) {
      dxIndex = bmSqr.width - (bmIndex.width + minimumClearance);
      dxSqr = 0;
    }
    else {
      dxIndex = 0;
      dxSqr = (bmIndex.width + minimumClearance) - bmSqr.width;
    }
  }

  // place the index
  dx = dxIndex;
  dy = aDesiredSize.ascent - (indexRaisedAscent + indexSize.ascent - bmIndex.ascent);
  FinishReflowChild(indexFrame, aPresContext, indexSize, dx, dy, 0);

  // place the radical symbol and the radical bar
  dx = dxSqr;
  dy = indexClearance + leading; // leave a leading at the top
  mSqrChar.SetRect(nsRect(dx, dy, bmSqr.width, bmSqr.ascent + bmSqr.descent));
  dx += bmSqr.width;
  mBarRect.SetRect(dx, dy, bmBase.width, ruleThickness);

  // place the base
  dy = aDesiredSize.ascent - baseSize.ascent;
  FinishReflowChild(baseFrame, aPresContext, baseSize, dx, dy, 0);

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  mBoundingMetrics.width = dx + bmBase.width;
  mBoundingMetrics.leftBearing = 
    PR_MIN(dxIndex + bmIndex.leftBearing, dxSqr + bmSqr.leftBearing);
  mBoundingMetrics.rightBearing = dx +
    PR_MAX(bmBase.width, bmBase.rightBearing);

  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}


// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
NS_IMETHODIMP
nsMathMLmrootFrame::GetAdditionalStyleContext(PRInt32           aIndex, 
                                              nsIStyleContext** aStyleContext) const
{
  NS_PRECONDITION(aStyleContext, "null OUT ptr");
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  *aStyleContext = nsnull;
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    mSqrChar.GetStyleContext(aStyleContext);
    break;
  default:
    return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmrootFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                              nsIStyleContext* aStyleContext)
{
  if (aIndex < 0) {
    return NS_ERROR_INVALID_ARG;
  }
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    mSqrChar.SetStyleContext(aStyleContext);
    break;
  }
  return NS_OK;
}
