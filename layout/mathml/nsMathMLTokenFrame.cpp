/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMathMLTokenFrame.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsTextFrame.h"
#include "mozilla/GeckoRestyleManager.h"
#include <algorithm>

using namespace mozilla;

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
  if (!mContent->IsMathMLElement(nsGkAtoms::mi_)) {
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
  for (nsIFrame* childFrame = PrincipalChildList().FirstChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    for (nsIFrame* childFrame2 = childFrame->PrincipalChildList().FirstChild();
         childFrame2; childFrame2 = childFrame2->GetNextSibling()) {
      if (childFrame2->IsTextFrame()) {
        childFrame2->AddStateBits(TEXT_IS_IN_TOKEN_MATHML);
        child = childFrame2;
        childCount++;
      }
    }
  }
  if (mContent->IsMathMLElement(nsGkAtoms::mi_) && childCount == 1) {
    nsAutoString data;
    nsContentUtils::GetNodeTextContent(mContent, false, data);

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

void
nsMathMLTokenFrame::SetInitialChildList(ChildListID     aListID,
                                        nsFrameList&    aChildList)
{
  // First, let the base class do its work
  nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);
  MarkTextFramesAsTokenMathML();
}

void
nsMathMLTokenFrame::AppendFrames(ChildListID aListID,
                                 nsFrameList& aChildList)
{
  nsMathMLContainerFrame::AppendFrames(aListID, aChildList);
  MarkTextFramesAsTokenMathML();
}

void
nsMathMLTokenFrame::InsertFrames(ChildListID aListID,
                                 nsIFrame* aPrevFrame,
                                 nsFrameList& aChildList)
{
  nsMathMLContainerFrame::InsertFrames(aListID, aPrevFrame, aChildList);
  MarkTextFramesAsTokenMathML();
}

void
nsMathMLTokenFrame::Reflow(nsPresContext*          aPresContext,
                           ReflowOutput&     aDesiredSize,
                           const ReflowInput& aReflowInput,
                           nsReflowStatus&          aStatus)
{
  MarkInReflow();
  mPresentationData.flags &= ~NS_MATHML_ERROR;

  // initializations needed for empty markup like <mtag></mtag>
  aDesiredSize.ClearSize();
  aDesiredSize.SetBlockStartAscent(0);
  aDesiredSize.mBoundingMetrics = nsBoundingMetrics();

  for (nsIFrame* childFrame : PrincipalChildList()) {
    // ask our children to compute their bounding metrics
    ReflowOutput childDesiredSize(aReflowInput.GetWritingMode(),
                                         aDesiredSize.mFlags
                                         | NS_REFLOW_CALC_BOUNDING_METRICS);
    WritingMode wm = childFrame->GetWritingMode();
    LogicalSize availSize = aReflowInput.ComputedSize(wm);
    availSize.BSize(wm) = NS_UNCONSTRAINEDSIZE;
    ReflowInput childReflowInput(aPresContext, aReflowInput,
                                       childFrame, availSize);
    ReflowChild(childFrame, aPresContext, childDesiredSize,
                childReflowInput, aStatus);
    //NS_ASSERTION(aStatus.IsComplete(), "bad status");
    SaveReflowAndBoundingMetricsFor(childFrame, childDesiredSize,
                                    childDesiredSize.mBoundingMetrics);
  }

  // place and size children
  FinalizeReflow(aReflowInput.mRenderingContext->GetDrawTarget(), aDesiredSize);

  aStatus.Reset();
  NS_FRAME_SET_TRUNCATION(aStatus, aReflowInput, aDesiredSize);
}

// For token elements, mBoundingMetrics is computed at the ReflowToken
// pass, it is not computed here because our children may be text frames
// that do not implement the GetBoundingMetrics() interface.
/* virtual */ nsresult
nsMathMLTokenFrame::Place(DrawTarget*          aDrawTarget,
                          bool                 aPlaceOrigin,
                          ReflowOutput& aDesiredSize)
{
  mBoundingMetrics = nsBoundingMetrics();
  for (nsIFrame* childFrame :PrincipalChildList()) {
    ReflowOutput childSize(aDesiredSize.GetWritingMode());
    GetReflowAndBoundingMetricsFor(childFrame, childSize,
                                   childSize.mBoundingMetrics, nullptr);
    // compute and cache the bounding metrics
    mBoundingMetrics += childSize.mBoundingMetrics;
  }

  RefPtr<nsFontMetrics> fm =
    nsLayoutUtils::GetInflatedFontMetricsForFrame(this);
  nscoord ascent = fm->MaxAscent();
  nscoord descent = fm->MaxDescent();

  aDesiredSize.mBoundingMetrics = mBoundingMetrics;
  aDesiredSize.Width() = mBoundingMetrics.width;
  aDesiredSize.SetBlockStartAscent(std::max(mBoundingMetrics.ascent, ascent));
  aDesiredSize.Height() = aDesiredSize.BlockStartAscent() +
                        std::max(mBoundingMetrics.descent, descent);

  if (aPlaceOrigin) {
    nscoord dy, dx = 0;
    for (nsIFrame* childFrame : PrincipalChildList()) {
      ReflowOutput childSize(aDesiredSize.GetWritingMode());
      GetReflowAndBoundingMetricsFor(childFrame, childSize,
                                     childSize.mBoundingMetrics);

      // place and size the child; (dx,0) makes the caret happy - bug 188146
      dy = childSize.Height() == 0 ? 0 : aDesiredSize.BlockStartAscent() - childSize.BlockStartAscent();
      FinishReflowChild(childFrame, PresContext(), childSize, nullptr, dx, dy, 0);
      dx += childSize.Width();
    }
  }

  SetReference(nsPoint(0, aDesiredSize.BlockStartAscent()));

  return NS_OK;
}

