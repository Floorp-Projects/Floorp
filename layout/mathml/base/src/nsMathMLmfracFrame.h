/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu>
 */

#ifndef nsMathMLmfracFrame_h___
#define nsMathMLmfracFrame_h___

#include "nsCOMPtr.h"
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
  friend nsresult NS_NewMathMLmfracFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD 
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;
    rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    // 1. The REC says:
    //    The <mfrac> element sets displaystyle to "false", or if it was already
    //    false increments scriptlevel by 1, within numerator and denominator.
    // 2. The TeXbook (Ch 17. p.141) says the numerator inherits the compression
    //    while the denominator is compressed
    PRInt32 increment =
       NS_MATHML_IS_DISPLAYSTYLE(mPresentationData.flags) ? 0 : 1;
    UpdatePresentationDataFromChildAt(aPresContext, 0, -1, increment,
      ~NS_MATHML_DISPLAYSTYLE,
       NS_MATHML_DISPLAYSTYLE);
    UpdatePresentationDataFromChildAt(aPresContext, 1,  1, 0,
       NS_MATHML_COMPRESSED,
       NS_MATHML_COMPRESSED);
    // check whether or not this is an embellished operator
    EmbellishOperator();
    return rv;
  }

  // helper to translate the thickness attribute into a usable form
  static nscoord 
  CalcLineThickness(nsIPresContext*  aPresContext,
                    nsIStyleContext* aStyleContext,
                    nsString&        aThicknessAttribute,
                    nscoord          onePixel,
                    nscoord          aDefaultRuleThickness);

protected:
  nsMathMLmfracFrame();
  virtual ~nsMathMLmfracFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
  
  nsRect  mLineRect;
};

#endif /* nsMathMLmfracFrame_h___ */
