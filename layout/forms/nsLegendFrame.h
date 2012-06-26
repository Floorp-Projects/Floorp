/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsLegendFrame_h___
#define nsLegendFrame_h___

#include "nsBlockFrame.h"

class nsLegendFrame : public nsBlockFrame {
public:
  NS_DECL_QUERYFRAME_TARGET(nsLegendFrame)
  NS_DECL_QUERYFRAME
  NS_DECL_FRAMEARENA_HELPERS

  nsLegendFrame(nsStyleContext* aContext) : nsBlockFrame(aContext) {}

  NS_IMETHOD Reflow(nsPresContext*           aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  virtual void DestroyFrom(nsIFrame* aDestructRoot);

  virtual nsIAtom* GetType() const;

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsAString& aResult) const;
#endif

  PRInt32 GetAlign();
};


#endif // guard
