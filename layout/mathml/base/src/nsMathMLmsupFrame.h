/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitions under the License.
 * 
 * The Original Code is Mozilla MathML Project.
 * 
 * The Initial Developer of the Original Code is The University Of 
 * Queensland.  Portions created by The University Of Queensland are
 * Copyright (C) 1999 The University Of Queensland.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Roger B. Sidje <rbs@maths.uq.edu.au>
 *   David J. Fiddes <D.J.Fiddes@hw.ac.uk>
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
 */

#ifndef nsMathMLmsupFrame_h___
#define nsMathMLmsupFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <msup> -- attach a superscript to a base
//

class nsMathMLmsupFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmsupFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  static nsresult
  PlaceSuperScript (nsIPresContext*      aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    PRBool               aPlaceOrigin,
                    nsHTMLReflowMetrics& aDesiredSize,
                    nsIFrame*            aForFrame,
                    nscoord              aUserSupScriptShift = 0,
                    nscoord              aScriptSpace = NSFloatPointsToTwips(0.5f));

  NS_IMETHOD
  TransmitAutomaticData(nsIPresContext* aPresContext)
  {
#if defined(NS_DEBUG) && defined(SHOW_BOUNDING_BOX)
    mPresentationData.flags |= NS_MATHML_SHOW_BOUNDING_METRICS;
#endif

    // check whether or not this is an embellished operator
    EmbellishOperator();
    // 1. The REC says:
    // The <msup> element increments scriptlevel by 1, and sets displaystyle to
    // "false", within superscript, but leaves both attributes unchanged within base.
    // 2. The TeXbook (Ch 17. p.141) says the superscript *inherits* the compression,
    // so we don't set the compression flag. Our parent will propagate its own.
    UpdatePresentationDataFromChildAt(aPresContext, 1, -1, 1,
      ~NS_MATHML_DISPLAYSTYLE,
       NS_MATHML_DISPLAYSTYLE);
    return NS_OK;
  }

protected:
  nsMathMLmsupFrame();
  virtual ~nsMathMLmsupFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
};

#endif /* nsMathMLmsupFrame_h___ */
