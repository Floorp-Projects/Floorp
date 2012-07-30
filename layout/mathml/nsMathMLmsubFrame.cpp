/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCOMPtr.h"
#include "nsFrame.h"
#include "nsPresContext.h"
#include "nsStyleContext.h"
#include "nsStyleConsts.h"

#include "nsMathMLmsubFrame.h"

//
// <msub> -- attach a subscript to a base - implementation
//

nsIFrame*
NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmsubFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmsubFrame)

nsMathMLmsubFrame::~nsMathMLmsubFrame()
{
}

NS_IMETHODIMP
nsMathMLmsubFrame::TransmitAutomaticData()
{
  // if our base is an embellished operator, let its state bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  // 1. The REC says:
  // The <msub> element increments scriptlevel by 1, and sets displaystyle to
  // "false", within subscript, but leaves both attributes unchanged within base.
  // 2. The TeXbook (Ch 17. p.141) says the subscript is compressed
  UpdatePresentationDataFromChildAt(1, -1,
    ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
     NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);

  return NS_OK;
}

/* virtual */ nsresult
nsMathMLmsubFrame::Place (nsRenderingContext& aRenderingContext,
                          bool                 aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  // extra spacing after sup/subscript
  nscoord scriptSpace = nsPresContext::CSSPointsToAppUnits(0.5f); // 0.5pt as in plain TeX

  // subscriptshift
  //
  // "Specifies the minimum amount to shift the baseline of subscript down; the
  // default is for the rendering agent to use its own positioning rules."
  //
  // values: length
  // default: automatic
  //
  // We use 0 as the default value so unitless values can be ignored.
  // XXXfredw Should we forbid negative values? (bug 411227)
  //
  nscoord subScriptShift = 0;
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::subscriptshift_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &subScriptShift,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      PresContext(), mStyleContext);
  }

  return nsMathMLmsubFrame::PlaceSubScript(PresContext(), 
                                           aRenderingContext,
                                           aPlaceOrigin,
                                           aDesiredSize,
                                           this,
                                           subScriptShift,
                                           scriptSpace);
}

// exported routine that both munder and msub share.
// munder uses this when movablelimits is set.
nsresult
nsMathMLmsubFrame::PlaceSubScript (nsPresContext*      aPresContext,
                                   nsRenderingContext& aRenderingContext,
                                   bool                 aPlaceOrigin,
                                   nsHTMLReflowMetrics& aDesiredSize,
                                   nsMathMLContainerFrame* aFrame,
                                   nscoord              aUserSubScriptShift,
                                   nscoord              aScriptSpace)
{
  // force the scriptSpace to be atleast 1 pixel 
  aScriptSpace = NS_MAX(nsPresContext::CSSPixelsToAppUnits(1), aScriptSpace);

  ////////////////////////////////////
  // Get the children's desired sizes

  nsBoundingMetrics bmBase, bmSubScript;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics subScriptSize;
  nsIFrame* baseFrame = aFrame->GetFirstPrincipalChild();
  nsIFrame* subScriptFrame = nullptr;
  if (baseFrame)
    subScriptFrame = baseFrame->GetNextSibling();
  if (!baseFrame || !subScriptFrame || subScriptFrame->GetNextSibling()) {
    // report an error, encourage people to get their markups in order
    return aFrame->ReflowError(aRenderingContext, aDesiredSize);
  }
  GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
  GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);

  // get the subdrop from the subscript font
  nscoord subDrop;
  GetSubDropFromChild(subScriptFrame, subDrop);
  // parameter v, Rule 18a, App. G, TeXbook
  nscoord minSubScriptShift = bmBase.descent + subDrop;

  //////////////////
  // Place Children
  
  // get min subscript shift limit from x-height
  // = h(x) - 4/5 * sigma_5, Rule 18b, App. G, TeXbook
  nscoord xHeight = 0;
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(baseFrame, getter_AddRefs(fm));

  xHeight = fm->XHeight();
  nscoord minShiftFromXHeight = (nscoord) 
    (bmSubScript.ascent - (4.0f/5.0f) * xHeight);

  // subScriptShift
  // = minimum amount to shift the subscript down set by user or from the font
  // = sub1 in TeX
  // = subscriptshift attribute * x-height
  nscoord subScriptShift, dummy;
  // get subScriptShift default from font
  GetSubScriptShifts (fm, subScriptShift, dummy);

  subScriptShift = 
    NS_MAX(subScriptShift, aUserSubScriptShift);

  // get actual subscriptshift to be used
  // Rule 18b, App. G, TeXbook
  nscoord actualSubScriptShift = 
    NS_MAX(minSubScriptShift,NS_MAX(subScriptShift,minShiftFromXHeight));
  // get bounding box for base + subscript
  nsBoundingMetrics boundingMetrics;
  boundingMetrics.ascent = 
    NS_MAX(bmBase.ascent, bmSubScript.ascent - actualSubScriptShift);
  boundingMetrics.descent = 
    NS_MAX(bmBase.descent, bmSubScript.descent + actualSubScriptShift);

  // add aScriptSpace to the subscript's width
  boundingMetrics.width = bmBase.width + bmSubScript.width + aScriptSpace;
  boundingMetrics.leftBearing = bmBase.leftBearing;
  boundingMetrics.rightBearing = NS_MAX(bmBase.rightBearing, bmBase.width +
    NS_MAX(bmSubScript.width + aScriptSpace, bmSubScript.rightBearing));
  aFrame->SetBoundingMetrics (boundingMetrics);

  // reflow metrics
  aDesiredSize.ascent = 
    NS_MAX(baseSize.ascent, subScriptSize.ascent - actualSubScriptShift);
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(baseSize.height - baseSize.ascent,
           subScriptSize.height - subScriptSize.ascent + actualSubScriptShift);
  aDesiredSize.width = boundingMetrics.width;
  aDesiredSize.mBoundingMetrics = boundingMetrics;

  aFrame->SetReference(nsPoint(0, aDesiredSize.ascent));

  if (aPlaceOrigin) {
    nscoord dx, dy;
    // now place the base ...
    dx = aFrame->MirrorIfRTL(aDesiredSize.width, baseSize.width, 0);
    dy = aDesiredSize.ascent - baseSize.ascent;
    FinishReflowChild (baseFrame, aPresContext, nullptr, baseSize, dx, dy, 0);
    // ... and subscript
    dx = aFrame->MirrorIfRTL(aDesiredSize.width, subScriptSize.width,
                             bmBase.width);
    dy = aDesiredSize.ascent - (subScriptSize.ascent - actualSubScriptShift);
    FinishReflowChild (subScriptFrame, aPresContext, nullptr, subScriptSize, dx, dy, 0);
  }

  return NS_OK;
}
