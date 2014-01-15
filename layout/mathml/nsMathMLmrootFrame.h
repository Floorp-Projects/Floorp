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
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  virtual void
  SetAdditionalStyleContext(int32_t          aIndex, 
                            nsStyleContext*  aStyleContext) MOZ_OVERRIDE;
  virtual nsStyleContext*
  GetAdditionalStyleContext(int32_t aIndex) const MOZ_OVERRIDE;

  virtual void
  Init(nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIFrame*        aPrevInFlow) MOZ_OVERRIDE;

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE;

  NS_IMETHOD
  Reflow(nsPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus) MOZ_OVERRIDE;

  virtual void
  GetIntrinsicWidthMetrics(nsRenderingContext* aRenderingContext,
                           nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

  virtual void BuildDisplayList(nsDisplayListBuilder*   aBuilder,
                                const nsRect&           aDirtyRect,
                                const nsDisplayListSet& aLists) MOZ_OVERRIDE;

  uint8_t
  ScriptIncrement(nsIFrame* aFrame) MOZ_OVERRIDE
  {
    return (aFrame && aFrame == mFrames.LastChild()) ? 2 : 0;
  }

protected:
  nsMathMLmrootFrame(nsStyleContext* aContext);
  virtual ~nsMathMLmrootFrame();
  
  nsMathMLChar mSqrChar;
  nsRect       mBarRect;
};

#endif /* nsMathMLmrootFrame_h___ */
