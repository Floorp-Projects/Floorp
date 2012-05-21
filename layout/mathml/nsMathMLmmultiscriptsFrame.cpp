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

#include "nsMathMLmmultiscriptsFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base - implementation
//

nsIFrame*
NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLmmultiscriptsFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLmmultiscriptsFrame)

nsMathMLmmultiscriptsFrame::~nsMathMLmmultiscriptsFrame()
{
}

NS_IMETHODIMP
nsMathMLmmultiscriptsFrame::TransmitAutomaticData()
{
  // if our base is an embellished operator, let its state bubble to us
  mPresentationData.baseFrame = mFrames.FirstChild();
  GetEmbellishDataFrom(mPresentationData.baseFrame, mEmbellishData);

  // The REC says:
  // The <mmultiscripts> element increments scriptlevel by 1, and sets
  // displaystyle to "false", within each of its arguments except base
  UpdatePresentationDataFromChildAt(1, -1,
    ~NS_MATHML_DISPLAYSTYLE, NS_MATHML_DISPLAYSTYLE);

  // The TeXbook (Ch 17. p.141) says the superscript inherits the compression
  // while the subscript is compressed. So here we collect subscripts and set
  // the compression flag in them.
  PRInt32 count = 0;
  bool isSubScript = false;
  nsAutoTArray<nsIFrame*, 8> subScriptFrames;
  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (childFrame->GetContent()->Tag() == nsGkAtoms::mprescripts_) {
      // mprescripts frame
    }
    else if (0 == count) {
      // base frame
    }
    else {
      // super/subscript block
      if (isSubScript) {
        // subscript
        subScriptFrames.AppendElement(childFrame);
      }
      else {
        // superscript
      }
      isSubScript = !isSubScript;
    }
    count++;
    childFrame = childFrame->GetNextSibling();
  }
  for (PRInt32 i = subScriptFrames.Length() - 1; i >= 0; i--) {
    childFrame = subScriptFrames[i];
    PropagatePresentationDataFor(childFrame,
      NS_MATHML_COMPRESSED, NS_MATHML_COMPRESSED);
  }

  return NS_OK;
}

void
nsMathMLmmultiscriptsFrame::ProcessAttributes()
{
  mSubScriptShift = 0;
  mSupScriptShift = 0;

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
  nsAutoString value;
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::subscriptshift_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &mSubScriptShift,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      PresContext(), mStyleContext);
  }
  // superscriptshift
  //
  // "Specifies the minimum amount to shift the baseline of superscript up; the
  // default is for the rendering agent to use its own positioning rules."
  //
  // values: length
  // default: automatic
  //
  // We use 0 as the default value so unitless values can be ignored.
  // XXXfredw Should we forbid negative values? (bug 411227)
  //
  GetAttribute(mContent, mPresentationData.mstyle,
               nsGkAtoms::superscriptshift_, value);
  if (!value.IsEmpty()) {
    ParseNumericValue(value, &mSupScriptShift,
                      nsMathMLElement::PARSE_ALLOW_NEGATIVE,
                      PresContext(), mStyleContext);
  }
}

/* virtual */ nsresult
nsMathMLmmultiscriptsFrame::Place(nsRenderingContext& aRenderingContext,
                                  bool                 aPlaceOrigin,
                                  nsHTMLReflowMetrics& aDesiredSize)
{
  ////////////////////////////////////
  // Get the children's desired sizes

  nscoord minShiftFromXHeight, subDrop, supDrop;

  ////////////////////////////////////////
  // Initialize super/sub shifts that
  // depend only on the current font
  ////////////////////////////////////////

  ProcessAttributes();

  // get x-height (an ex)
  const nsStyleFont* font = GetStyleFont();
  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  aRenderingContext.SetFont(fm);

  nscoord xHeight = fm->XHeight();

  nscoord ruleSize;
  GetRuleThickness (aRenderingContext, fm, ruleSize);

  // scriptspace from TeX for extra spacing after sup/subscript (0.5pt in plain TeX)
  // forced to be at least 1 pixel here
  nscoord onePixel = nsPresContext::CSSPixelsToAppUnits(1);
  nscoord scriptSpace = NS_MAX(nsPresContext::CSSPointsToAppUnits(0.5f), onePixel);

  /////////////////////////////////////
  // first the shift for the subscript

  // subScriptShift{1,2}
  // = minimum amount to shift the subscript down
  // = sub{1,2} in TeXbook
  // subScriptShift1 = subscriptshift attribute * x-height
  nscoord subScriptShift1, subScriptShift2;

  // Get subScriptShift{1,2} default from font
  GetSubScriptShifts (fm, subScriptShift1, subScriptShift2);
  if (0 < mSubScriptShift) {
    // the user has set the subscriptshift attribute
    float scaler = ((float) subScriptShift2) / subScriptShift1;
    subScriptShift1 = NS_MAX(subScriptShift1, mSubScriptShift);
    subScriptShift2 = NSToCoordRound(scaler * subScriptShift1);
  }
  // the font dependent shift
  nscoord subScriptShift = NS_MAX(subScriptShift1,subScriptShift2);

  /////////////////////////////////////
  // next the shift for the superscript

  // supScriptShift{1,2,3}
  // = minimum amount to shift the supscript up
  // = sup{1,2,3} in TeX
  // supScriptShift1 = superscriptshift attribute * x-height
  // Note that there are THREE values for supscript shifts depending
  // on the current style
  nscoord supScriptShift1, supScriptShift2, supScriptShift3;
  // Set supScriptShift{1,2,3} default from font
  GetSupScriptShifts (fm, supScriptShift1, supScriptShift2, supScriptShift3);
  if (0 < mSupScriptShift) {
    // the user has set the superscriptshift attribute
    float scaler2 = ((float) supScriptShift2) / supScriptShift1;
    float scaler3 = ((float) supScriptShift3) / supScriptShift1;
    supScriptShift1 = NS_MAX(supScriptShift1, mSupScriptShift);
    supScriptShift2 = NSToCoordRound(scaler2 * supScriptShift1);
    supScriptShift3 = NSToCoordRound(scaler3 * supScriptShift1);
  }

  // get sup script shift depending on current script level and display style
  // Rule 18c, App. G, TeXbook
  nscoord supScriptShift;
  if ( font->mScriptLevel == 0 &&
       NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) &&
      !NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style D in TeXbook
    supScriptShift = supScriptShift1;
  }
  else if (NS_MATHML_IS_COMPRESSED(mPresentationData.flags)) {
    // Style C' in TeXbook = D',T',S',SS'
    supScriptShift = supScriptShift3;
  }
  else {
    // everything else = T,S,SS
    supScriptShift = supScriptShift2;
  }

  ////////////////////////////////////
  // Get the children's sizes
  ////////////////////////////////////

  nscoord width = 0, prescriptsWidth = 0, rightBearing = 0;
  nsIFrame* mprescriptsFrame = nsnull; // frame of <mprescripts/>, if there.
  bool isSubScript = false;
  nscoord minSubScriptShift = 0, minSupScriptShift = 0;
  nscoord trySubScriptShift = subScriptShift;
  nscoord trySupScriptShift = supScriptShift;
  nscoord maxSubScriptShift = subScriptShift;
  nscoord maxSupScriptShift = supScriptShift;
  PRInt32 count = 0;
  nsHTMLReflowMetrics baseSize;
  nsHTMLReflowMetrics subScriptSize;
  nsHTMLReflowMetrics supScriptSize;
  nsIFrame* baseFrame = nsnull;
  nsIFrame* subScriptFrame = nsnull;
  nsIFrame* supScriptFrame = nsnull;

  bool firstPrescriptsPair = false;
  nsBoundingMetrics bmBase, bmSubScript, bmSupScript;
  nscoord italicCorrection = 0;

  mBoundingMetrics.width = 0;
  mBoundingMetrics.ascent = mBoundingMetrics.descent = -0x7FFFFFFF;
  nscoord ascent = -0x7FFFFFFF, descent = -0x7FFFFFFF;
  aDesiredSize.width = aDesiredSize.height = 0;

  nsIFrame* childFrame = mFrames.FirstChild();
  while (childFrame) {
    if (childFrame->GetContent()->Tag() == nsGkAtoms::mprescripts_) {
      if (mprescriptsFrame) {
        // duplicate <mprescripts/> found
        // report an error, encourage people to get their markups in order
        return ReflowError(aRenderingContext, aDesiredSize);
      }
      mprescriptsFrame = childFrame;
      firstPrescriptsPair = true;
    }
    else {
      if (0 == count) {
        // base
        baseFrame = childFrame;
        GetReflowAndBoundingMetricsFor(baseFrame, baseSize, bmBase);
        GetItalicCorrection(bmBase, italicCorrection);
        // for the superscript, we always add "a little to spare"
        italicCorrection += onePixel;

        // we update mBoundingMetrics.{ascent,descent} with that
        // of the baseFrame only after processing all the sup/sub pairs
        // XXX need italic correction only *if* there are postscripts ?
        mBoundingMetrics.width = bmBase.width + italicCorrection;
        mBoundingMetrics.rightBearing = bmBase.rightBearing;
        mBoundingMetrics.leftBearing = bmBase.leftBearing; // until overwritten
      }
      else {
        // super/subscript block
        if (isSubScript) {
          // subscript
          subScriptFrame = childFrame;
          GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
          // get the subdrop from the subscript font
          GetSubDropFromChild (subScriptFrame, subDrop);
          // parameter v, Rule 18a, App. G, TeXbook
          minSubScriptShift = bmBase.descent + subDrop;
          trySubScriptShift = NS_MAX(minSubScriptShift,subScriptShift);
          mBoundingMetrics.descent =
            NS_MAX(mBoundingMetrics.descent,bmSubScript.descent);
          descent = NS_MAX(descent,subScriptSize.height - subScriptSize.ascent);
          width = bmSubScript.width + scriptSpace;
          rightBearing = bmSubScript.rightBearing;
        }
        else {
          // supscript
          supScriptFrame = childFrame;
          GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);
          // get the supdrop from the supscript font
          GetSupDropFromChild (supScriptFrame, supDrop);
          // parameter u, Rule 18a, App. G, TeXbook
          minSupScriptShift = bmBase.ascent - supDrop;
          // get min supscript shift limit from x-height
          // = d(x) + 1/4 * sigma_5, Rule 18c, App. G, TeXbook
          minShiftFromXHeight = NSToCoordRound
            ((bmSupScript.descent + (1.0f/4.0f) * xHeight));
          trySupScriptShift =
            NS_MAX(minSupScriptShift,NS_MAX(minShiftFromXHeight,supScriptShift));
          mBoundingMetrics.ascent =
            NS_MAX(mBoundingMetrics.ascent,bmSupScript.ascent);
          ascent = NS_MAX(ascent,supScriptSize.ascent);
          width = NS_MAX(width, bmSupScript.width + scriptSpace);
          rightBearing = NS_MAX(rightBearing, bmSupScript.rightBearing);

          if (!mprescriptsFrame) { // we are still looping over base & postscripts
            mBoundingMetrics.rightBearing = mBoundingMetrics.width + rightBearing;
            mBoundingMetrics.width += width;
          }
          else {
            prescriptsWidth += width;
            if (firstPrescriptsPair) {
              firstPrescriptsPair = false;
              mBoundingMetrics.leftBearing =
                NS_MIN(bmSubScript.leftBearing, bmSupScript.leftBearing);
            }
          }
          width = rightBearing = 0;

          // negotiate between the various shifts so that
          // there is enough gap between the sup and subscripts
          // Rule 18e, App. G, TeXbook
          nscoord gap =
            (trySupScriptShift - bmSupScript.descent) -
            (bmSubScript.ascent - trySubScriptShift);
          if (gap < 4.0f * ruleSize) {
            // adjust trySubScriptShift to get a gap of (4.0 * ruleSize)
            trySubScriptShift += NSToCoordRound ((4.0f * ruleSize) - gap);
          }

          // next we want to ensure that the bottom of the superscript
          // will be > (4/5) * x-height above baseline
          gap = NSToCoordRound ((4.0f/5.0f) * xHeight -
                  (trySupScriptShift - bmSupScript.descent));
          if (gap > 0.0f) {
            trySupScriptShift += gap;
            trySubScriptShift -= gap;
          }
          
          maxSubScriptShift = NS_MAX(maxSubScriptShift, trySubScriptShift);
          maxSupScriptShift = NS_MAX(maxSupScriptShift, trySupScriptShift);

          trySubScriptShift = subScriptShift;
          trySupScriptShift = supScriptShift;
        }
      }

      isSubScript = !isSubScript;
    }
    count++;
    childFrame = childFrame->GetNextSibling();
  }
  // note: width=0 if all sup-sub pairs match correctly
  if ((0 != width) || !baseFrame || !subScriptFrame || !supScriptFrame) {
    // report an error, encourage people to get their markups in order
    return ReflowError(aRenderingContext, aDesiredSize);
  }

  // we left out the width of prescripts, so ...
  mBoundingMetrics.rightBearing += prescriptsWidth;
  mBoundingMetrics.width += prescriptsWidth;

  // we left out the base during our bounding box updates, so ...
  mBoundingMetrics.ascent =
    NS_MAX(mBoundingMetrics.ascent+maxSupScriptShift,bmBase.ascent);
  mBoundingMetrics.descent =
    NS_MAX(mBoundingMetrics.descent+maxSubScriptShift,bmBase.descent);

  // get the reflow metrics ...
  aDesiredSize.ascent =
    NS_MAX(ascent+maxSupScriptShift,baseSize.ascent);
  aDesiredSize.height = aDesiredSize.ascent +
    NS_MAX(descent+maxSubScriptShift,baseSize.height - baseSize.ascent);
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.mBoundingMetrics = mBoundingMetrics;

  mReference.x = 0;
  mReference.y = aDesiredSize.ascent;

  //////////////////
  // Place Children

  // Place prescripts, followed by base, and then postscripts.
  // The list of frames is in the order: {base} {postscripts} {prescripts}
  // We go over the list in a circular manner, starting at <prescripts/>

  if (aPlaceOrigin) {
    nscoord dx = 0, dy = 0;

    count = 0;
    childFrame = mprescriptsFrame;
    do {
      if (!childFrame) { // end of prescripts,
        // place the base ...
        childFrame = baseFrame;
        dy = aDesiredSize.ascent - baseSize.ascent;
        FinishReflowChild (baseFrame, PresContext(), nsnull, baseSize,
                           MirrorIfRTL(aDesiredSize.width,
                                       baseSize.width,
                                       dx),
                           dy, 0);
        dx += bmBase.width + italicCorrection;
      }
      else if (mprescriptsFrame != childFrame) {
        // process each sup/sub pair
        if (0 == count) {
          subScriptFrame = childFrame;
          count = 1;
        }
        else if (1 == count) {
          supScriptFrame = childFrame;
          count = 0;

          // get the ascent/descent of sup/subscripts stored in their rects
          // rect.x = descent, rect.y = ascent
          GetReflowAndBoundingMetricsFor(subScriptFrame, subScriptSize, bmSubScript);
          GetReflowAndBoundingMetricsFor(supScriptFrame, supScriptSize, bmSupScript);

          // center w.r.t. largest width
          width = NS_MAX(subScriptSize.width, supScriptSize.width);

          dy = aDesiredSize.ascent - subScriptSize.ascent +
            maxSubScriptShift;
          FinishReflowChild (subScriptFrame, PresContext(), nsnull,
                             subScriptSize,
                             MirrorIfRTL(aDesiredSize.width,
                                         subScriptSize.width,
                                         dx + (width-subScriptSize.width)/2),
                             dy, 0);

          dy = aDesiredSize.ascent - supScriptSize.ascent -
            maxSupScriptShift;
          FinishReflowChild (supScriptFrame, PresContext(), nsnull,
                             supScriptSize,
                             MirrorIfRTL(aDesiredSize.width,
                                         supScriptSize.width,
                                         dx + (width-supScriptSize.width)/2),
                             dy, 0);

          dx += width + scriptSpace;
        }
      }
      childFrame = childFrame->GetNextSibling();
    } while (mprescriptsFrame != childFrame);
  }

  return NS_OK;
}
