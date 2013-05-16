/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmsubsupFrame_h___
#define nsMathMLmsubsupFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <msubsup> -- attach a subscript-superscript pair to a base
//

class nsMathMLmsubsupFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmsubsupFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE;

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

  static nsresult
  PlaceSubSupScript(nsPresContext*      aPresContext,
                    nsRenderingContext& aRenderingContext,
                    bool                 aPlaceOrigin,
                    nsHTMLReflowMetrics& aDesiredSize,
                    nsMathMLContainerFrame* aForFrame,
                    nscoord              aUserSubScriptShift,
                    nscoord              aUserSupScriptShift,
                    nscoord              aScriptSpace);

protected:
  nsMathMLmsubsupFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmsubsupFrame();
};

#endif /* nsMathMLmsubsupFrame_h___ */
