/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmfracFrame_h___
#define nsMathMLmfracFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"

//
// <mfrac> -- form a fraction from two subexpressions
//

/*
The MathML REC describes:

The <mfrac> element is used for fractions. It can also be used to mark up
fraction-like objects such as binomial coefficients and Legendre symbols.
The syntax for <mfrac> is:
  <mfrac> numerator denominator </mfrac>

Attributes of <mfrac>:
     Name                      values                     default
  linethickness  number [ v-unit ] | thin | medium | thick  1

E.g.,
linethickness=2 actually means that linethickness=2*DEFAULT_THICKNESS
(DEFAULT_THICKNESS is not specified by MathML, see below.)

The linethickness attribute indicates the thickness of the horizontal
"fraction bar", or "rule", typically used to render fractions. A fraction
with linethickness="0" renders without the bar, and might be used within
binomial coefficients. A linethickness greater than one might be used with
nested fractions.

In general, the value of linethickness can be a number, as a multiplier
of the default thickness of the fraction bar (the default thickness is
not specified by MathML), or a number with a unit of vertical length (see
Section 2.3.3), or one of the keywords medium (same as 1), thin (thinner
than 1, otherwise up to the renderer), or thick (thicker than 1, otherwise
up to the renderer).

The <mfrac> element sets displaystyle to "false", or if it was already
false increments scriptlevel by 1, within numerator and denominator.
These attributes are inherited by every element from its rendering
environment, but can be set explicitly only on the <mstyle>
element.
*/

class nsMathMLmfracFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmfracFrame)

  friend nsIFrame* NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual eMathMLFrameType GetMathMLFrameType() override;

  virtual nsresult
  MeasureForWidth(DrawTarget* aDrawTarget,
                  ReflowOutput& aDesiredSize) override;

  virtual nsresult
  Place(DrawTarget*          aDrawTarget,
        bool                 aPlaceOrigin,
        ReflowOutput& aDesiredSize) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  virtual nsresult
  AttributeChanged(int32_t  aNameSpaceID,
                   nsIAtom* aAttribute,
                   int32_t  aModType) override;

  NS_IMETHOD
  TransmitAutomaticData() override;

  // override the base method so that we can deal with the fraction line
  virtual nscoord
  FixInterFrameSpacing(ReflowOutput& aDesiredSize) override;

  // helper to translate the thickness attribute into a usable form
  static nscoord
  CalcLineThickness(nsPresContext*  aPresContext,
                    nsStyleContext*  aStyleContext,
                    nsString&        aThicknessAttribute,
                    nscoord          onePixel,
                    nscoord          aDefaultRuleThickness,
                    float            aFontSizeInflation);

  uint8_t
  ScriptIncrement(nsIFrame* aFrame) override;

protected:
  explicit nsMathMLmfracFrame(nsStyleContext* aContext)
    : nsMathMLContainerFrame(aContext, kClassID)
    , mLineRect()
    , mSlashChar(nullptr)
    , mLineThickness(0)
    , mIsBevelled(false)
  {}
  virtual ~nsMathMLmfracFrame();

  nsresult PlaceInternal(DrawTarget*          aDrawTarget,
                         bool                 aPlaceOrigin,
                         ReflowOutput& aDesiredSize,
                         bool                 aWidthOnly);

  // Display a slash
  void DisplaySlash(nsDisplayListBuilder* aBuilder,
                    nsIFrame* aFrame, const nsRect& aRect,
                    nscoord aThickness,
                    const nsDisplayListSet& aLists);

  nsRect        mLineRect;
  nsMathMLChar* mSlashChar;
  nscoord       mLineThickness;
  bool          mIsBevelled;
};

#endif /* nsMathMLmfracFrame_h___ */
