/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"
#include "nsRenderingContext.h"

#include "nsMathMLmrootFrame.h"
#include <algorithm>

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
NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmrootFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmrootFrame)

nsMathMLmrootFrame::nsMathMLmrootFrame(nsStyleContext* aContext) :
  nsMathMLContainerFrame(aContext),
  mSqrChar(),
  mBarRect()
{
}

nsMathMLmrootFrame::~nsMathMLmrootFrame()
{
}

NS_IMETHODIMP
nsMathMLmrootFrame::Init(nsIContent*      aContent,
                         nsIFrame*        aParent,
                         nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsMathMLContainerFrame::Init(aContent, aParent, aPrevInFlow);
  
  nsPresContext *presContext = PresContext();

  // No need to track the style context given to our MathML char. 
  // The Style System will use Get/SetAdditionalStyleContext() to keep it
  // up-to-date if dynamic changes arise.
  nsAutoString sqrChar; sqrChar.Assign(kSqrChar);
  mSqrChar.SetData(presContext, sqrChar);
  ResolveMathMLCharStyle(presContext, mContent, mStyleContext, &mSqrChar, true);

  return rv;
}

NS_IMETHODIMP
nsMathMLmrootFrame::TransmitAutomaticData()
{
  // 1. The REC says:
  //    The <mroot> element increments scriptlevel by 2, and sets displaystyle to
  //    "false", within index, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says \sqrt is compressed
  UpdatePresentationDataFromChildAt(1, 1,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
  UpdatePresentationDataFromChildAt(0, 0,
     NS_MATHML_COMPRESSED, NS_MATHML_COMPRESSED);

  return NS_OK;
}

NS_IMETHODIMP
nsMathMLmrootFrame::BuildDisplayList(nsDisplayListBuilder*   aBuilder,
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
    rv = mSqrChar.Display(aBuilder, this, aLists, 0);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = DisplayBar(aBuilder, this, mBarRect, aLists);
    NS_ENSURE_SUCCESS(rv, rv);

#if defined(DEBUG) && defined(SHOW_BOUNDING_BOX)
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

static void
GetRadicalXOffsets(nscoord aIndexWidth, nscoord aSqrWidth,
                   nsFontMetrics* aFontMetrics,
                   nscoord* aIndexOffset, nscoord* aSqrOffset)
{
  // The index is tucked in closer to the radical while making sure
  // that the kern does not make the index and radical collide
  nscoord dxIndex, dxSqr;
  nscoord xHeight = aFontMetrics->XHeight();
  nscoord indexRadicalKern = NSToCoordRound(1.35f * xHeight);
  if (indexRadicalKern > aIndexWidth) {
    dxIndex = indexRadicalKern - aIndexWidth;
    dxSqr = 0;
  }
  else {
    dxIndex = 0;
    dxSqr = aIndexWidth - indexRadicalKern;
  }
  // avoid collision by leaving a minimum space between index and radical
  nscoord minimumClearance = aSqrWidth/2;
  if (dxIndex + aIndexWidth + minimumClearance > dxSqr + aSqrWidth) {
    if (aIndexWidth + minimumClearance < aSqrWidth) {
      dxIndex = aSqrWidth - (aIndexWidth + minimumClearance);
      dxSqr = 0;
    }
    else {
      dxIndex = 0;
      dxSqr = (aIndexWidth + minimumClearance) - aSqrWidth;
    }
  }

  if (aIndexOffset)
    *aIndexOffset = dxIndex;
  if (aSqrOffset)
    *aSqrOffset = dxSqr;
}

NS_IMETHODIMP
nsMathMLmrootFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;
  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsReflowStatus childStatus;

  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;

  nsBoundingMetrics bmSqr, bmBase, bmIndex;
  nsRenderingContext& renderingContext = *aReflowState.rendContext;

  //////////////////
  // Reflow Children

  int32_t count = 0;
  nsIFrame* baseFrame = nullptr;
  nsIFrame* indexFrame = nullptr;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics indexSize;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    // ask our children to compute their bounding metrics 
    nsHTMLReflowMetrics childDesiredSize(aDesiredSize.mFlags
                                         | NS_REFLOW_CALC_BOUNDING_METRICS);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext,
                     childDesiredSize, childReflowState, childStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(childStatus), "bad status");
    if (NS_FAILED(rv)) {
      // Call DidReflow() for the child frames we successfully did reflow.
      DidReflowChildren(mFrames.FirstChild(), childFrame);
      return rv;
    }
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
    childFrame = childFrame->GetNextSibling();
  }
  if (2 != count) {
    // report an error, encourage people to get their markups in order
    ReportChildCountError();
    rv = ReflowError(renderingContext, aDesiredSize);
    aStatus = NS_FRAME_COMPLETE;
    NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
    // Call DidReflow() for the child frames we successfully did reflow.
    DidReflowChildren(mFrames.FirstChild(), childFrame);
    return rv;
  }

  ////////////
  // Prepare the radical symbol and the overline bar

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  renderingContext.SetFont(fm);

  // For radical glyphs from TeX fonts and some of the radical glyphs from
  // Mathematica fonts, the thickness of the overline can be obtained from the
  // ascent of the glyph.  Most fonts however have radical glyphs above the
  // baseline so no assumption can be made about the meaning of the ascent.
  nscoord ruleThickness, leading, em;
  GetRuleThickness(renderingContext, fm, ruleThickness);

  PRUnichar one = '1';
  nsBoundingMetrics bmOne = renderingContext.GetBoundingMetrics(&one, 1);

  // get the leading to be left at the top of the resulting frame
  // this seems more reliable than using fm->GetLeading() on suspicious fonts
  GetEmHeight(fm, em);
  leading = nscoord(0.2f * em); 

  // Rule 11, App. G, TeXbook
  // psi = clearance between rule and content
  nscoord phi = 0, psi = 0;
  if (NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags))
    phi = fm->XHeight();
  else
    phi = ruleThickness;
  psi = ruleThickness + phi/4;

  // built-in: adjust clearance psi to emulate \mathstrut using '1' (TexBook, p.131)
  if (bmOne.ascent > bmBase.ascent)
    psi += bmOne.ascent - bmBase.ascent;

  // make sure that the rule appears on on screen
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  if (ruleThickness < onePixel) {
    ruleThickness = onePixel;
  }

  // adjust clearance psi to get an exact number of pixels -- this
  // gives a nicer & uniform look on stacked radicals (bug 130282)
  nscoord delta = psi % onePixel;
  if (delta)
    psi += onePixel - delta; // round up

  // Stretch the radical symbol to the appropriate height if it is not big enough.
  nsBoundingMetrics contSize = bmBase;
  contSize.descent = bmBase.ascent + bmBase.descent + psi;
  contSize.ascent = ruleThickness;

  // height(radical) should be >= height(base) + psi + ruleThickness
  nsBoundingMetrics radicalSize;
  mSqrChar.Stretch(aPresContext, renderingContext,
                   NS_STRETCH_DIRECTION_VERTICAL, 
                   contSize, radicalSize,
                   NS_STRETCH_LARGER,
                   NS_MATHML_IS_RTL(mPresentationData.flags));
  // radicalSize have changed at this point, and should match with
  // the bounding metrics of the char
  mSqrChar.GetBoundingMetrics(bmSqr);

  // Update the desired size for the container (like msqrt, index is not yet included)
  // the baseline will be that of the base.
  mBoundingMetrics.ascent = bmBase.ascent + psi + ruleThickness;
  mBoundingMetrics.descent = 
    std::max(bmBase.descent,
           (bmSqr.ascent + bmSqr.descent - mBoundingMetrics.ascent));
  mBoundingMetrics.width = bmSqr.width + bmBase.width;
  mBoundingMetrics.leftBearing = bmSqr.leftBearing;
  mBoundingMetrics.rightBearing = bmSqr.width + 
    std::max(bmBase.width, bmBase.rightBearing); // take also care of the rule

  aDesiredSize.ascent = mBoundingMetrics.ascent + leading;
  aDesiredSize.height = aDesiredSize.ascent +
    std::max(baseSize.height - baseSize.ascent,
           mBoundingMetrics.descent + ruleThickness);
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
    nscoord descent = aDesiredSize.height - aDesiredSize.ascent;
    aDesiredSize.ascent = mBoundingMetrics.ascent + leading;
    aDesiredSize.height = aDesiredSize.ascent + descent;
  }

  nscoord dxIndex, dxSqr;
  GetRadicalXOffsets(bmIndex.width, bmSqr.width, fm, &dxIndex, &dxSqr);

  mBoundingMetrics.width = dxSqr + bmSqr.width + bmBase.width;
  mBoundingMetrics.leftBearing = 
    std::min(dxIndex + bmIndex.leftBearing, dxSqr + bmSqr.leftBearing);
  mBoundingMetrics.rightBearing = dxSqr + bmSqr.width +
    std::max(bmBase.width, bmBase.rightBearing);

  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  GatherAndStoreOverflow(&aDesiredSize);

  // place the index
  nscoord dx = dxIndex;
  nscoord dy = aDesiredSize.ascent - (indexRaisedAscent + indexSize.ascent - bmIndex.ascent);
  FinishReflowChild(indexFrame, aPresContext, nullptr, indexSize,
                    MirrorIfRTL(aDesiredSize.width, indexSize.width, dx),
                    dy, 0);

  // place the radical symbol and the radical bar
  dx = dxSqr;
  dy = indexClearance + leading; // leave a leading at the top
  mSqrChar.SetRect(nsRect(MirrorIfRTL(aDesiredSize.width, bmSqr.width, dx),
                          dy, bmSqr.width, bmSqr.ascent + bmSqr.descent));
  dx += bmSqr.width;
  mBarRect.SetRect(MirrorIfRTL(aDesiredSize.width, bmBase.width, dx),
                   dy, bmBase.width, ruleThickness);

  // place the base
  dy = aDesiredSize.ascent - baseSize.ascent;
  FinishReflowChild(baseFrame, aPresContext, nullptr, baseSize,
                    MirrorIfRTL(aDesiredSize.width, baseSize.width, dx),
                    dy, 0);

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

/* virtual */ nscoord
nsMathMLmrootFrame::GetIntrinsicWidth(nsRenderingContext* aRenderingContext)
{
  nsIFrame* baseFrame = mFrames.FirstChild();
  nsIFrame* indexFrame = nullptr;
  if (baseFrame)
    indexFrame = baseFrame->GetNextSibling();
  if (!indexFrame || indexFrame->GetNextSibling()) {
    nsHTMLReflowMetrics desiredSize;
    ReflowError(*aRenderingContext, desiredSize);
    return desiredSize.width;
  }

  nscoord baseWidth =
    nsLayoutUtils::IntrinsicForContainer(aRenderingContext, baseFrame,
                                         nsLayoutUtils::PREF_WIDTH);
  nscoord indexWidth =
    nsLayoutUtils::IntrinsicForContainer(aRenderingContext, indexFrame,
                                         nsLayoutUtils::PREF_WIDTH);
  nscoord sqrWidth = mSqrChar.GetMaxWidth(PresContext(), *aRenderingContext);

  nscoord dxSqr;
  GetRadicalXOffsets(indexWidth, sqrWidth, aRenderingContext->FontMetrics(),
                     nullptr, &dxSqr);

  return dxSqr + sqrWidth + baseWidth;
}

// ----------------------
// the Style System will use these to pass the proper style context to our MathMLChar
nsStyleContext*
nsMathMLmrootFrame::GetAdditionalStyleContext(int32_t aIndex) const
{
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    return mSqrChar.GetStyleContext();
    break;
  default:
    return nullptr;
  }
}

void
nsMathMLmrootFrame::SetAdditionalStyleContext(int32_t          aIndex, 
                                              nsStyleContext*  aStyleContext)
{
  switch (aIndex) {
  case NS_SQR_CHAR_STYLE_CONTEXT_INDEX:
    mSqrChar.SetStyleContext(aStyleContext);
    break;
  }
}
