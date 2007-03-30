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
 * The Original Code is Mozilla MathML Project.
 *
 * The Initial Developer of the Original Code is
 * The University Of Queensland.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Vilya Harvey <vilya@nag.co.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
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


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsUnitConversion.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"

#include "nsMathMLmsqrtFrame.h"

//
// <msqrt> and <mroot> -- form a radical - implementation
//

//NOTE:
//  The code assumes that TeX fonts are picked.
//  There is no fall-back to draw the branches of the sqrt explicitly
//  in the case where TeX fonts are not there. In general, there are no
//  fall-back(s) in MathML when some (freely-downloadable) fonts are missing.
//  Otherwise, this will add much work and unnecessary complexity to the core
//  MathML  engine. Assuming that authors have the free fonts is part of the
//  deal. We are not responsible for cases of misconfigurations out there.

// additional style context to be used by our MathMLChar.
#define NS_SQR_CHAR_STYLE_CONTEXT_INDEX   0

static const PRUnichar kSqrChar = PRUnichar(0x221A);

nsIFrame*
NS_NewMathMLmsqrtFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsqrtFrame(aContext);
}

nsMathMLmsqrtFrame::nsMathMLmsqrtFrame(nsStyleContext* aContext) :
  nsMathMLContainerFrame(aContext),
  mSqrChar(),
  mBarRect()
{
}

nsMathMLmsqrtFrame::~nsMathMLmsqrtFrame()
{
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Init(nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
                                             
  nsPresContext *presContext = PresContext();

  // No need to tract the style context given to our MathML char. 
  // The Style System will use Get/SetAdditionalStyleContext() to keep it
  // up-to-date if dynamic changes arise.
  nsAutoString sqrChar; sqrChar.Assign(kSqrChar);
  mSqrChar.SetData(presContext, sqrChar);
  ResolveMathMLCharStyle(presContext, mContent, mStyleContext, &mSqrChar, PR_TRUE);

  return rv;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  mPresentationData.flags |= NS_MATHML_STRETCH_ALL_CHILDREN_VERTICALLY;

  return NS_OK;
}


NS_IMETHODIMP
nsMathMLmsqrtFrame::TransmitAutomaticData()
{
  // 1. The REC says:
  //    The <msqrt> element leaves both attributes [displaystyle and scriptlevel]
  //    unchanged within all its arguments.
  // 2. The TeXBook (Ch 17. p.141) says that \sqrt is cramped 
  UpdatePresentationDataFromChildAt(0, -1, 0,
     NS_MATHML_COMPRESSED,
     NS_MATHML_COMPRESSED);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                     const nsRect&           aDirtyRect,
                                     const nsDisplayListSet& aLists)
{
  /////////////
  // paint the content we are square-rooting
  nsresult rv = nsMathMLContainerFrame::BuildDisplayList(aBuilder, aDirtyRect, aLists);
  NS_ENSURE_SUCCESS(rv, rv);

  /////////////
  // paint the sqrt symbol
  if (!NS_MATHML_HAS_ERROR(mPresentationData.flags)) {
    rv = mSqrChar.Display(aBuilder, this, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = DisplayBar(aBuilder, this, mBarRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
    // for visual debug
    nsRect rect;
    mSqrChar.GetRect(rect);
    nsBoundingMetrics bm;
    mSqrChar.GetBoundingMetrics(bm);
    rv = DisplayBoundingMetrics(aBuilder, this, rect.TopLeft(), bm, aLists);
#endif
  }

  return rv;
}

NS_IMETHODIMP
nsMathMLmsqrtFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  ///////////////
  // Let the base class format our content like an inferred mrow
  nsHTMLReflowMetrics baseSize(aDesiredSize);
  nsresult rv = nsMathMLContainerFrame::Reflow(aPresContext, baseSize,
                                               aReflowState, aStatus);
  //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  if (NS_FAILED(rv)) return rv;

  nsBoundingMetrics bmSqr, bmBase;
  bmBase = baseSize.mBoundingMetrics;

  ////////////
  // Prepare the radical symbol and the overline bar

  nsIRenderingContext& renderingContext = *aReflowState.rendContext;
  renderingContext.SetFont(GetStyleFont()->mFont, nsnull);
  nsCOMPtr<nsIFontMetrics> fm;
  renderingContext.GetFontMetrics(*getter_AddRefs(fm));

  nscoord ruleThickness, leading, em;
  GetRuleThickness(renderingContext, fm, ruleThickness);

  nsBoundingMetrics bmOne;
  renderingContext.GetBoundingMetrics(NS_LITERAL_STRING("1").get(), 1, bmOne);

  // get the leading to be left at the top of the resulting frame
  // this seems more reliable than using fm->GetLeading() on suspicious fonts               
  GetEmHeight(fm, em);
  leading = nscoord(0.2f * em); 

  // Rule 11, App. G, TeXbook
  // psi = clearance between rule and content
  nscoord phi = 0, psi = 0;
  if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    fm->GetXHeight(phi);
  else
    phi = ruleThickness;
  psi = ruleThickness + phi/4;

  // built-in: adjust clearance psi to emulate \mathstrut using '1' (TexBook, p.131)
  if (bmOne.ascent > bmBase.ascent)
    psi += bmOne.ascent - bmBase.ascent;

  // Stretch the radical symbol to the appropriate height if it is not big enough.
  nsBoundingMetrics contSize = bmBase;
  contSize.ascent = ruleThickness;
  contSize.descent = bmBase.ascent + bmBase.descent + psi;

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
  // make sure that the rule appears on the screen
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  if (ruleThickness < onePixel) {
    ruleThickness = onePixel;
  }

  // adjust clearance psi to get an exact number of pixels -- this
  // gives a nicer & uniform look on stacked radicals (bug 130282)
  nscoord delta = psi % onePixel;
  if (delta)
    psi += onePixel - delta; // round up

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

  aDesiredSize.ascent = mBoundingMetrics.ascent + leading;
  aDesiredSize.height = aDesiredSize.ascent +
    PR_MAX(baseSize.height - baseSize.ascent,
           mBoundingMetrics.descent + ruleThickness);
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Adjust the origins to leave room for the sqrt char and the overline bar

  dx = radicalSize.width;
  dy = aDesiredSize.ascent - baseSize.ascent;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    childFrame->SetPosition(childFrame->GetPosition() + nsPoint(dx, dy));
    childFrame = childFrame->GetNextSibling();
  }

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

nscoord
nsMathMLmsqrtFrame::FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize)
{
  nscoord gap = nsMathMLContainerFrame::FixInterFrameSpacing(aDesiredSize);
  if (!gap) return 0;

  nsRect rect;
  mSqrChar.GetRect(rect);
  rect.MoveBy(gap, 0);
  mSqrChar.SetRect(rect);
  mBarRect.MoveBy(gap, 0);
  return gap;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmsqrtFrame::GetAdditionalStyleContext(PRInt32 aIndex) const
{
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    return mSqrChar.GetStyleContext();
    break;
  default:
    return nsnull;
  }
}

void
nsMathMLmsqrtFrame::SetAdditionalStyleContext(PRInt32          aIndex, 
                                              nsStyleContext*  aStyleContext)
{
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    mSqrChar.SetStyleContext(aStyleContext);
    break;
  }
}
