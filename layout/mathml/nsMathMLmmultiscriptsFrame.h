/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMathMLmmultiscriptsFrame_h___
#define nsMathMLmmultiscriptsFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mmultiscripts> -- attach prescripts and tensor indices to a base 
//

class nsMathMLmmultiscriptsFrame : public nsMathMLContainerFrame {
public:
  NS_DECL_FRAMEARENA_HELPERS

  friend nsIFrame* NS_NewMathMLmmultiscriptsFrame(nsIPresShell* aPresShell, nsStyleContext* aContext);

  NS_IMETHOD
  TransmitAutomaticData();

  virtual nsresult
  Place(nsRenderingContext& aRenderingContext,
        bool                 aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

protected:
  nsMathMLmmultiscriptsFrame(nsStyleContext* aContext) : nsMathMLContainerFrame(aContext) {}
  virtual ~nsMathMLmmultiscriptsFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nscoord mSubScriptShift;
  nscoord mSupScriptShift;

  void
  ProcessAttributes();
};

#endif /* nsMathMLmmultiscriptsFrame_h___ */
