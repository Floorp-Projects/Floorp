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

  ProcessTextData();

  return NS_OK;
}

eMathMLFrameType
nsMathMLTokenFrame::GetMathMLFrameType()
{
  // treat everything other than <mi> as ordinary...
  if (mContent->Tag() != nsGkAtoms::mi_) {
    return eMathMLFrameType_Ordinary;
  }

  // for <mi>, distinguish between italic and upright...
  nsAutoString style;
  // mathvariant overrides fontstyle
  // http://www.w3.org/TR/2003/REC-MathML2-20031021/chapter3.html#presm.deprecatt
  mContent->GetAttr(kNameSpaceID_None,
                    nsGkAtoms::_moz_math_fontstyle_, style) ||
    GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::mathvariant_,
                 style) ||
    GetAttribute(mContent, mPresentationData.mstyle, nsGkAtoms::fontstyle_,
                 style);

  if (style.EqualsLiteral("italic") || style.EqualsLiteral("bold-italic") ||
      style.EqualsLiteral("script") || style.EqualsLiteral("bold-script") ||
      style.EqualsLiteral("sans-serif-italic") ||
      style.EqualsLiteral("sans-serif-bold-italic")) {
    return eMathMLFrameType_ItalicIdentifier;
  }
  else if(style.EqualsLiteral("invariant")) {
    nsAutoString data;
    nsContentUtils::GetNodeTextContent(mContent, false, data);
    data.CompressWhitespace();
    eMATHVARIANT variant = nsMathMLOperators::LookupInvariantChar(data);

    switch (variant) {
    case eMATHVARIANT_italic:
    case eMATHVARIANT_bold_italic:
    case eMATHVARIANT_script:
    case eMATHVARIANT_bold_script:
    case eMATHVARIANT_sans_serif_italic:
    case eMATHVARIANT_sans_serif_bold_italic:
      return eMathMLFrameType_ItalicIdentifier;
    default:
      ; // fall through to upright
    }
  }
  return eMathMLFrameType_UprightIdentifier;
}

void
nsMathMLTokenFrame::MarkTextFramesAsTokenMathML()
{
  // Set flags on child text frames to force them to trim their leading and
  // trailing whitespaces.
  for (nsIFrame* childFrame = GetFirstPrincipalChild(); childFrame;
       childFrame = childFrame->GetNextSibling()) {
    for (nsIFrame* childFrame2 = childFrame->GetFirstPrincipalChild();
         childFrame2; childFrame2 = childFrame2->GetNextSibling()) {
      if (childFrame2->GetType() == nsGkAtoms::textFrame) {
        childFrame2->AddStateBits(TEXT_IS_IN_TOKEN_MATHML);
      }
    }
  }
}

NS_IMETHODIMP
nsMathMLTokenFrame::SetInitialChildList(ChildListID     aListID,
                                        nsFrameList&    aChildList)
{
  // First, let the base class do its work
  nsresult rv = nsMathMLContainerFrame::SetInitialChildList(aListID, aChildList);
  if (NS_FAILED(rv))
    return rv;

  MarkTextFramesAsTokenMathML();

  ProcessTextData();
  return rv;
}

NS_IMETHODIMP
nsMathMLTokenFrame::AppendFrames(ChildListID aListID,
                                 nsFrameList& aChildList)
{
  nsresult rv = nsMathMLContainerFrame::AppendFrames(aListID, aChildList);
  if (NS_FAILED(rv))
    return rv;

  MarkTextFramesAsTokenMathML();

  return rv;
}

NS_IMETHODIMP
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
  aDesiredSize.width = aDesiredSize.height = 0;
  aDesiredSize.ascent = 0;
  aDesiredSize.mBoundingMetrics = nsBoundingMetrics();

  nsSize availSize(aReflowState.ComputedWidth(), NS_UNCONSTRAINEDSIZE);
  nsIFrame* childFrame = GetFirstPrincipalChild();
  while (childFrame) {
    // ask our children to compute their bounding metrics
    nsHTMLReflowMetrics childDesiredSize(aDesiredSize.mFlags
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
    nsHTMLReflowMetrics childSize;
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
  aDesiredSize.width = mBoundingMetrics.width;
  aDesiredSize.ascent = std::max(mBoundingMetrics.ascent, ascent);
  aDesiredSize.height = aDesiredSize.ascent +
                        std::max(mBoundingMetrics.descent, descent);

  if (aPlaceOrigin) {
    nscoord dy, dx = 0;
    for (nsIFrame* childFrame = GetFirstPrincipalChild(); childFrame;
         childFrame = childFrame->GetNextSibling()) {
      nsHTMLReflowMetrics childSize;
      GetReflowAndBoundingMetricsFor(childFrame, childSize,
                                     childSize.mBoundingMetrics);

      // place and size the child; (dx,0) makes the caret happy - bug 188146
      dy = childSize.height == 0 ? 0 : aDesiredSize.ascent - childSize.ascent;
      FinishReflowChild(childFrame, PresContext(), nullptr, childSize, dx, dy, 0);
      dx += childSize.width;
    }
  }

  SetReference(nsPoint(0, aDesiredSize.ascent));

  return NS_OK;
}

/* virtual */ void
nsMathMLTokenFrame::MarkIntrinsicWidthsDirty()
{
  // this could be called due to changes in the nsTextFrame beneath us
  // when something changed in the text content. So re-process our text
  ProcessTextData();

  nsMathMLContainerFrame::MarkIntrinsicWidthsDirty();
}

void
nsMathMLTokenFrame::ProcessTextData()
{
  // see if the style changes from normal to italic or vice-versa
  if (!SetTextStyle())
    return;

  // explicitly request a re-resolve to pick up the change of style
  PresContext()->RestyleManager()->
    PostRestyleEvent(mContent->AsElement(), eRestyle_Subtree, NS_STYLE_HINT_NONE);
}

///////////////////////////////////////////////////////////////////////////
// For <mi>, if the content is not a single character, turn the font to
// normal (this function will also query attributes from the mstyle hierarchy)
// Returns true if there is a style change.
//
// http://www.w3.org/TR/2003/REC-MathML2-20031021/chapter3.html#presm.commatt
//
//  "It is important to note that only certain combinations of
//   character data and mathvariant attribute values make sense.
//   ...
//   By design, the only cases that have an unambiguous
//   interpretation are exactly the ones that correspond to SMP Math
//   Alphanumeric Symbol characters, which are enumerated in Section
//   6.2.3 Mathematical Alphanumeric Symbols Characters. In all other
//   cases, it is suggested that renderers ignore the value of the
//   mathvariant attribute if it is present."
//
// There are no corresponding characters for mathvariant=normal, suggesting
// that this value should be ignored, but this (from the same section of
// Chapter 3) implies that font-style should not be inherited, but set to
// normal for mathvariant=normal:
//
//  "In particular, inheritance of the mathvariant attribute does not follow
//   the CSS model. The default value for this attribute is "normal"
//   (non-slanted) for all tokens except mi. ... (The deprecated fontslant
//   attribute also behaves this way.)"

bool
nsMathMLTokenFrame::SetTextStyle()
{
  if (mContent->Tag() != nsGkAtoms::mi_)
    return false;

  if (!mFrames.FirstChild())
    return false;

  // Get the text content that we enclose and its length
  nsAutoString data;
  nsContentUtils::GetNodeTextContent(mContent, false, data);
  data.CompressWhitespace();
  int32_t length = data.Length();
  if (!length)
    return false;

  nsAutoString fontstyle;
  bool isSingleCharacter =
    length == 1 ||
    (length == 2 && NS_IS_HIGH_SURROGATE(data[0]));
  if (isSingleCharacter &&
      nsMathMLOperators::LookupInvariantChar(data) != eMATHVARIANT_NONE) {
    // bug 65951 - a non-stylable character has its own intrinsic appearance
    fontstyle.AssignLiteral("invariant");
  }
  else {
    // Attributes override the default behavior.
    nsAutoString value;
    if (!(GetAttribute(mContent, mPresentationData.mstyle,
                       nsGkAtoms::mathvariant_, value) ||
          GetAttribute(mContent, mPresentationData.mstyle,
                       nsGkAtoms::fontstyle_, value))) {
      if (!isSingleCharacter) {
        fontstyle.AssignLiteral("normal");
      }
      else if (length == 1 && // BMP
               !nsMathMLOperators::
                TransformVariantChar(data[0], eMATHVARIANT_italic).
                Equals(data)) {
        // Transformation exists.  Try to make the BMP character look like the
        // styled character using the style system until bug 114365 is resolved.
        fontstyle.AssignLiteral("italic");
      }
      // else single character but there is no corresponding Math Alphanumeric
      // Symbol character: "ignore the value of the [default] mathvariant
      // attribute".
    }
  }

  // set the _moz-math-font-style attribute without notifying that we want a reflow
  if (fontstyle.IsEmpty()) {
    if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::_moz_math_fontstyle_)) {
      mContent->UnsetAttr(kNameSpaceID_None, nsGkAtoms::_moz_math_fontstyle_,
                          false);
      return true;
    }
  }
  else if (!mContent->AttrValueIs(kNameSpaceID_None,
                                  nsGkAtoms::_moz_math_fontstyle_,
                                  fontstyle, eCaseMatters)) {
    mContent->SetAttr(kNameSpaceID_None, nsGkAtoms::_moz_math_fontstyle_,
                      fontstyle, false);
    return true;
  }

  return false;
}
