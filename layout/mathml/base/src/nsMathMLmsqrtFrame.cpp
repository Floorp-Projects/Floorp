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

#include "nsMathMLmsqrtFrame.h"

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
NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsMathMLmsqrtFrame* it = new (aPresShell) nsMathMLmsqrtFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsMathMLmsqrtFrame::nsMathMLmsqrtFrame() :
  mSqrChar(),
  mBarRect()
{
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
nsMathMLmsqrtFrame::Paint(nsIPresContext*      aPresContext,
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
nsMathMLmsqrtFrame::Reflow(nsIPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  nsBoundingMetrics bmSqr, bmBase;

  ///////////////
  // Let the base class format our content like an inferred mrow
  nsHTMLReflowMetrics baseSize(aDesiredSize);
  rv = nsMathMLContainerFrame::Reflow(aPresContext, baseSize,
                                      aReflowState, aStatus);
  //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  if (NS_FAILED(rv)) return rv;

  bmBase = baseSize.mBoundingMetrics;

  ////////////
  // Prepare the radical symbol and the overline bar

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
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
  contSize.ascent = ruleThickness;
  contSize.descent = bmBase.ascent + bmBase.descent + psi;

  // height(radical) should be >= height(base) + psi + ruleThickness
  nsBoundingMetrics radicalSize;
  nsAutoString sqrChar; sqrChar.Assign(kSqrChar);
  mSqrChar.SetData(aPresContext, sqrChar); // XXX hack to reset the char, bug 45010
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
  // make sure that the rule appears on the screen
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

  nscoord dx = 0, dy = 0;
  // place the radical symbol and the radical bar
  dy = leading; // leave a leading at the top
  mSqrChar.SetRect(nsRect(dx, dy, bmSqr.width, bmSqr.ascent + bmSqr.descent));
  dx = bmSqr.width;
  mBarRect.SetRect(dx, dy, bmBase.width, ruleThickness);

  // Update the desired size for the container.
  // the baseline will be that of the base.
  mBoundingMetrics.ascent = bmBase.ascent + psi + ruleThickness;
  mBoundingMetrics.descent = 
    PR_MAX(bmBase.descent, (bmSqr.descent - (bmBase.ascent + psi)));
  mBoundingMetrics.width = bmSqr.width + bmBase.width;
  mBoundingMetrics.leftBearing = bmSqr.leftBearing;
  mBoundingMetrics.rightBearing = bmSqr.width + 
    PR_MAX(bmBase.width, bmBase.rightBearing); // take also care of the rule

  aDesiredSize.ascent = bmBase.ascent + psi + ruleThickness + leading;
  aDesiredSize.descent =
    PR_MAX(baseSize.descent, (mBoundingMetrics.descent + ruleThickness));
  aDesiredSize.height = aDesiredSize.ascent + aDesiredSize.descent;
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Adjust the origins to leave room for the sqrt char and the overline bar

  nsPoint origin;
  dx = radicalSize.width;
  dy = aDesiredSize.ascent - baseSize.ascent;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    childFrame->GetOrigin(origin);
    childFrame->MoveTo(aPresContext, origin.x + dx, origin.y + dy);
    childFrame->GetNextSibling(&childFrame);
  }

  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
NS_IMETHODIMP
nsMathMLmsqrtFrame::GetAdditionalStyleContext(PRInt32           aIndex, 
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
nsMathMLmsqrtFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
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
