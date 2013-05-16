/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsMathMLmencloseFrame_h___
#define nsMathMLmencloseFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

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

enum nsMencloseNotation
  {
    NOTATION_LONGDIV = 0x1,
    NOTATION_RADICAL = 0x2,
    NOTATION_ROUNDEDBOX = 0x4,
    NOTATION_CIRCLE = 0x8,
    NOTATION_LEFT = 0x10,
    NOTATION_RIGHT = 0x20,
    NOTATION_TOP = 0x40,
    NOTATION_BOTTOM = 0x80,
    NOTATION_UPDIAGONALSTRIKE = 0x100,
    NOTATION_DOWNDIAGONALSTRIKE = 0x200,
    NOTATION_VERTICALSTRIKE = 0x400,
    NOTATION_HORIZONTALSTRIKE = 0x800
  };

class nsMathMLmencloseFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmencloseFrame(nsIPresShell*   aPresShell,
                                             nsStyleContext* aContext);
  
  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;
  
  virtual nsresult
  MeasureForWidth(nsRenderingContext& aRenderingContext,
                  nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;
  
  NS_IMETHOD
  AttributeChanged(int32_t         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   int32_t         aModType) MOZ_OVERRIDE;
  
  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext) MOZ_OVERRIDE;
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  NS_IMETHOD
  InheritAutomaticData(nsIFrame* aParent) MOZ_OVERRIDE;

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE;

  virtual nscoord
  FixInterFrameSpacing(nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

protected:
  nsMathMLmencloseFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmencloseFrame();

  nsresult PlaceInternal(nsRenderingContext& aRenderingContext,
                         bool                 aPlaceOrigin,
                         nsHTMLReflowMetrics& aDesiredSize,
                         bool                 aWidthOnly);
  
  // functions to parse the "notation" attribute.
  nsresult AddNotation(const nsAString& aNotation);
  void InitNotations();

  // Description of the notations to draw
  uint32_t mNotationsToDraw;
  bool IsToDraw(nsMencloseNotation mask)
  {
    return mask & mNotationsToDraw;
  }

  nscoord mRuleThickness;
  nsTArray<nsMathMLChar> mMathMLChar;
  int8_t mLongDivCharIndex, mRadicalCharIndex;
  nscoord mContentWidth;
  nsresult AllocateMathMLChar(nsMencloseNotation mask);

  // Display a frame of the specified type.
  // @param aType Type of frame to display
  void DisplayNotation(nsDisplayListBuilder* aBuilder,
                       nsIFrame* aFrame, const nsRect& aRect,
                       const nsDisplayListSet& aLists,
                       nscoord aThickness, nsMencloseNotation aType);
};

#endif /* nsMathMLmencloseFrame_h___ */
