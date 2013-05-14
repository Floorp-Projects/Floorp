/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmsubFrame_h___
#define nsMathMLmsubFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <msub> -- attach a subscript to a base
//

class nsMathMLmsubFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE;

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

  static nsresult
  PlaceSubScript (nsPresContext*      aPresContext,
                  nsRenderingContext& aRenderingContext,
                  bool                 aPlaceOrigin,
                  nsHTMLReflowMetrics& aDesiredSize,
                  nsMathMLContainerFrame* aForFrame,
                  nscoord              aUserSubScriptShift,
                  nscoord              aScriptSpace);

 protected:
  nsMathMLmsubFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmsubFrame();
};

#endif /* nsMathMLmsubFrame_h___ */
