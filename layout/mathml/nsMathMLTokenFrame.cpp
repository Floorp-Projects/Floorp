/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLTokenFrame.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsTextFrame.h"
#include "RestyleManager.h"
#include <algorithm>

nsIFrame*
NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsStyleContext* aContext)
{
  return new (aPresShell) nsMathMLTokenFrame(aContext);
}

NS_IMPL_FRAMEARENA_HELPERS(nsMathMLTokenFrame)

nsMathMLTokenFrame::~nsMathMLTokenFrame()
{
}

NS_IMETHODIMP
nsMathMLTokenFrame::InheritAutomaticData(nsIFrame* aParent)
{
  // let the base class get the default from our parent
  nsMathMLContainerFrame::InheritAutomaticData(aParent);

  return NS_OK;
}

eMathMLFrameType
nsMathMLTokenFrame::GetMathMLFrameType()
{
  // treat everything other than <mi> as ordinary...
  if (mContent->Tag() != nsGkAtoms::mi_) {
    return eMathMLFrameType_Ordinary;
  }

  uint8_t mathVariant = StyleFont()->mMathVariant;
  if ((mathVariant == NS_MATHML_MATHVARIANT_NONE &&
       (StyleFont()->mFont.style == NS_STYLE_FONT_STYLE_ITALIC ||
        HasAnyStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI))) ||
      mathVariant == NS_MATHML_MATHVARIANT_ITALIC ||
      mathVariant == NS_MATHML_MATHVARIANT_BOLD_ITALIC ||
      mathVariant == NS_MATHML_MATHVARIANT_SANS_SERIF_ITALIC ||
      mathVariant == NS_MATHML_MATHVARIANT_SANS_SERIF_BOLD_ITALIC) {
    return eMathMLFrameType_ItalicIdentifier;
  }
  return eMathMLFrameType_UprightIdentifier;
}

void
nsMathMLTokenFrame::MarkTextFramesAsTokenMathML()
{
  nsIFrame* child = nullptr;
  uint32_t childCount = 0;

  // Set flags on child text frames
  // - to force them to trim their leading and trailing whitespaces.
  // - Indicate which frames are suitable for mathvariant
  // - flag single character <mi> frames for special italic treatment
  for (nsIFrame* childFrame = GetFirstPrincipalChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    for (nsIFrame* childFrame2 = childFrame->GetFirstPrincipalChild();
         childFrame2; childFrame2 = childFrame2->GetNextSibling()) {
      if (childFrame2->GetType() == nsGkAtoms::textFrame) {
        childFrame2->AddStateBits(TEXT_IS_IN_TOKEN_MATHML);
        child = childFrame2;
        childCount++;
      }
    }
  }
  if (mContent->Tag() == nsGkAtoms::mi_ && childCount == 1) {
    nsAutoString data;
    if (!nsContentUtils::GetNodeTextContent(mContent, false, data)) {
      NS_RUNTIMEABORT("OOM");
    }

    data.CompressWhitespace();
    int32_t length = data.Length();

    bool isSingleCharacter = length == 1 ||
      (length == 2 && NS_IS_HIGH_SURROGATE(data[0]));

    if (isSingleCharacter) {
      child->AddStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI);
      AddStateBits(NS_FRAME_IS_IN_SINGLE_CHAR_MI);
    }
  }
}

nsresult
nsMathMLTokenFrame::SetInitialChildList(ChildListID     aListID,
                                        nsFrameList&    aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);
  if (NS_FAILED(rv))
    return rv;

  MarkTextFramesAsTokenMathML();

  return rv;
}

nsresult
nsMathMLTokenFrame::AppendFrames(ChildListID aListID,
                                 nsFrameList& aChildList)
{
  nsresult rv = nsMathMLContainerFrame::AppendFrames(aListID, aChildList);
  if (NS_FAILED(rv))
    return rv;

  MarkTextFramesAsTokenMathML();

  return rv;
}

nsresult
nsMathMLTokenFrame::InsertFrames(ChildListID aListID,
                                 nsIFrame* aPrevFrame,
                                 nsFrameList& aChildList)
{
  nsresult rv = nsMathMLContainerFrame::InsertFrames(aListID, aPrevFrame,
                                                     aChildList);
  if (NS_FAILED(rv))
    return rv;

  MarkTextFramesAsTokenMathML();

  return rv;
}

nsresult
nsMathMLTokenFrame::Reflow(nsPresContext*          aPresContext,
                           nsHTMLReflowMetrics&     aDesiredSize,
                           const nsHTMLReflowState& aReflowState,
                           nsReflowStatus&          aStatus)
{
  nsresult rv = NS_OK;

  // initializations needed for empty markup like <mtag></mtag>
  aDesiredSize.Width() = aDesiredSize.Height() = 0;
  aDesiredSize.SetTopAscent(0);
  aDesiredSize.mBoundingMetrics = nsBoundingMetrics();

  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsIFrame* childFrame = GetFirstPrincipalChild();
  while (childFrame) {
    // ask our children to compute their bounding metrics
    nsHTMLReflowMetrics childDesiredSize(aReflowState.GetWritingMode(),
                                         aDesiredSize.mFlags
                                         | NS_REFLOW_CALC_BOUNDING_METRICS);
    nsHTMLReflowState childReflowState(aPresContext, aReflowState,
                                       childFrame, availSize);
    rv = ReflowChild(childFrame, aPresContext, childDesiredSize,
                     childReflowState, aStatus);
    //NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
    if (NS_FAILED(rv)) {
      // Call DidReflow() for the child frames we successfully did reflow.
      DidReflowChildren(GetFirstPrincipalChild(), childFrame);
      return rv;
    }

    SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                    childDesiredSize.mBoundingMetrics);

    childFrame = childFrame->GetNextSibling();
  }


  // place and size children
  FinalizeReflow(*aReflowState.rendContext, aDesiredSize);

  aStatus = NS_FRAME_COMPLETE;
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aDesiredSize);
  return NS_OK;
}

// For token elements, mBoundingMetrics is computed at the ReflowToken
// pass, it is not computed here because our children may be text frames
// that do not implement the GetBoundingMetrics() interface.
/* virtual */ nsresult
nsMathMLTokenFrame::Place(nsRenderingContext& aRenderingContext,
                          bool                 aPlaceOrigin,
                          nsHTMLReflowMetrics& aDesiredSize)
{
  mBoundingMetrics = nsBoundingMetrics();
  for (nsIFrame* childFrame = GetFirstPrincipalChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    nsHTMLReflowMetrics childSize(aDesiredSize.GetWritingMode());
    GetReflowAndBoundingMetricsFor(childFrame, childSize,
                                   childSize.mBoundingMetrics, nullptr);
    // compute and cache the bounding metrics
    mBoundingMetrics += childSize.mBoundingMetrics;
  }

  nsRefPtr<nsFontMetrics> fm;
  nsLayoutUtils::GetFontMetricsForFrame(this, getter_AddRefs(fm));
  nscoord ascent = fm->MaxAscent();
  nscoord descent = fm->MaxDescent();

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.SetTopAscent(std::max(mBoundingMetrics.ascent, ascent));
  aDesiredSize.Height() = aDesiredSize.TopAscent() +
                        std::max(mBoundingMetrics.descent, descent);

  if (aPlaceOrigin) {
    nscoord dy, dx = 0;
    for (nsIFrame* childFrame = GetFirstPrincipalChild(); childFrame;
         childFrame = childFrame->GetNextSibling()) {
      nsHTMLReflowMetrics childSize(aDesiredSize.GetWritingMode());
      GetReflowAndBoundingMetricsFor(childFrame, childSize,
                                     childSize.mBoundingMetrics);

      // place and size the child; (dx,0) makes the caret happy - bug 188146
      dy = childSize.Height() == 0 ? 0 : aDesiredSize.TopAscent() - childSize.TopAscent();
      FinishReflowChild(childFrame, PresContext(), childSize, nullptr, dx, dy, 0);
      dx += childSize.Width();
    }
  }

  SetReference(nsPoint(0, aDesiredSize.TopAscent()));

  return NS_OK;
}

