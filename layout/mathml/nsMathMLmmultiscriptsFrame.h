/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmmultiscriptsFrame_h___
#define nsMathMLmmultiscriptsFrame_h___

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base 
// <msub> -- attach a subscript to a base
// <msubsup> -- attach a subscript-superscript pair to a base
// <msup> -- attach a superscript to a base
//

class nsMathMLmmultiscriptsFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData() MOZ_OVERRIDE;

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize) MOZ_OVERRIDE;

  static nsresult
  PlaceMultiScript(nsPresContext*      aPresContext,
                    nsRenderingContext& aRenderingContext,
                    bool                 aPlaceOrigin,
                    nsHTMLReflowMetrics& aDesiredSize,
                    nsMathMLContainerFrame* aForFrame,
                    nscoord              aUserSubScriptShift,
                    nscoord              aUserSupScriptShift,
                    nscoord              aScriptSpace);

protected:
  nsMathMLmmultiscriptsFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmmultiscriptsFrame();
  

};

#endif /* nsMathMLmmultiscriptsFrame_h___ */
