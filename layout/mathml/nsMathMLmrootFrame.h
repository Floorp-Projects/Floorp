/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmrootFrame_h___
#define nsMathMLmrootFrame_h___

#include "mozilla/Attributes.h"
#include "nsMathMLContainerFrame.h"
#include "nsMathMLChar.h"

//
// <msqrt> and <mroot> -- form a radical
//

class nsMathMLmrootFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS(nsMathMLmrootFrame)

  friend nsIFrame* NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext) override;
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const override;

  virtual void
  Init(nsIContent*       aContent,
       nsContainerFrame* aParent,
       nsIFrame*         aPrevInFlow) override;

  NS_IMETHOD
  TransmitAutomaticData() override;

  virtual void
  Reflow(nsPresContext*          aPresContext,
         ReflowOutput&     aDesiredSize,
         const ReflowInput& aReflowInput,
         nsReflowStatus&          aStatus) override;

  void
  GetRadicalXOffsets(nscoord aIndexWidth, nscoord aSqrWidth,
                     nsFontMetrics* aFontMetrics,
                     nscoord* aIndexOffset,
                     nscoord* aSqrOffset);

  virtual void
  GetIntrinsicISizeMetrics(nsRenderingContext* aRenderingContext,
                           ReflowOutput& aDesiredSize) override;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) override;

  uint8_t
  ScriptIncrement(nsIFrame* aFrame) override
  {
    return (aFrame && aFrame == mFrames.LastChild()) ? 2 : 0;
  }

protected:
  explicit nsMathMLmrootFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmrootFrame();
  
  nsMathMLChar mSqrChar;
  nsRect       mBarRect;
};

#endif /* nsMathMLmrootFrame_h___ */
