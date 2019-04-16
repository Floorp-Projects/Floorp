/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmencloseFrame_h___
#define nsMathMLmencloseFrame_h___

#include "mozilla/Attributes.h"
#include "mozilla/EnumSet.h"
#include "nsMathMLContainerFrame.h"

namespace mozilla {
class PresShell;
}  // namespace mozilla

//
// <menclose> -- enclose content with a stretching symbol such
// as a long division sign.
//

/*
  The MathML REC describes:

  The menclose element renders its content inside the enclosing notation
  specified by its notation attribute. menclose accepts any number of arguments;
  if this number is not 1, its contents are treated as a single "inferred mrow"
  containing its arguments, as described in Section 3.1.3 Required Arguments.
*/

enum nsMencloseNotation {
  NOTATION_LONGDIV,
  NOTATION_RADICAL,
  NOTATION_ROUNDEDBOX,
  NOTATION_CIRCLE,
  NOTATION_LEFT,
  NOTATION_RIGHT,
  NOTATION_TOP,
  NOTATION_BOTTOM,
  NOTATION_UPDIAGONALSTRIKE,
  NOTATION_DOWNDIAGONALSTRIKE,
  NOTATION_VERTICALSTRIKE,
  NOTATION_HORIZONTALSTRIKE,
  NOTATION_UPDIAGONALARROW,
  NOTATION_PHASORANGLE
};

class nsMathMLmencloseFrame : public nsMathMLContainerFrame {
 public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmencloseFrame)

  friend nsIFrame* NS_NewMathMLmencloseFrame(mozilla::PresShell* aPresShell,
                                             ComputedStyle* aStyle);

  virtual nsresult Place(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                         ReflowOutput& aDesiredSize) override;

  virtual nsresult MeasureForWidth(DrawTarget* aDrawTarget,
                                   ReflowOutput& aDesiredSize) override;

  virtual nsresult AttributeChanged(int32_t aNameSpaceID, nsAtom* aAttribute,
                                    int32_t aModType) override;

  virtual void SetAdditionalComputedStyle(
      int32_t aIndex, ComputedStyle* aComputedStyle) override;
  virtual ComputedStyle* GetAdditionalComputedStyle(
      int32_t aIndex) const override;

  virtual void BuildDisplayList(nsDisplayListBuilder* aBuilder,
                                const nsDisplayListSet& aLists) override;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) override;

  NS_IMETHOD
  TransmitAutomaticData() override;

  virtual nscoord FixInterFrameSpacing(ReflowOutput& aDesiredSize) override;

  bool IsMrowLike() override {
    return mFrames.FirstChild() != mFrames.LastChild() || !mFrames.FirstChild();
  }

 protected:
  explicit nsMathMLmencloseFrame(ComputedStyle* aStyle,
                                 nsPresContext* aPresContext,
                                 ClassID aID = kClassID);
  virtual ~nsMathMLmencloseFrame();

  nsresult PlaceInternal(DrawTarget* aDrawTarget, bool aPlaceOrigin,
                         ReflowOutput& aDesiredSize, bool aWidthOnly);

  // functions to parse the "notation" attribute.
  nsresult AddNotation(const nsAString& aNotation);
  void InitNotations();

  // Description of the notations to draw
  mozilla::EnumSet<nsMencloseNotation> mNotationsToDraw;
  bool IsToDraw(nsMencloseNotation notation) {
    return mNotationsToDraw.contains(notation);
  }

  nscoord mRuleThickness;
  nscoord mRadicalRuleThickness;
  nsTArray<nsMathMLChar> mMathMLChar;
  int8_t mLongDivCharIndex, mRadicalCharIndex;
  nscoord mContentWidth;
  nsresult AllocateMathMLChar(nsMencloseNotation mask);

  // Display a frame of the specified type.
  // @param aType Type of frame to display
  void DisplayNotation(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       const nsRect& aRect, const nsDisplayListSet& aLists,
                       nscoord aThickness, nsMencloseNotation aType);
};

#endif /* nsMathMLmencloseFrame_h___ */
