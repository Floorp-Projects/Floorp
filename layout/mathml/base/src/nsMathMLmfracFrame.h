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
nested fractions. These cases are shown below: 

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
element. (See Section 3.3.4.) 
*/

/*
TODO:
-  Take care of displaystyle=true|false and inheritered <mstyle> 
   Before reflow, <mfrac> should change the font-size of the children with scriptlevel<=2
   How to do it? From the DOM interface?
   From direct CSS manipulation? In nsCSSStyleRule, there is
   nsCSSFont*  ourFont;
    if (NS_OK == aDeclaration->GetData(kCSSFontSID, (nsCSSStruct**)&ourFont)) 

- CalcLength(..) in nsCSSStyleRule.cpp is where CSS units are implemented. 
How to use that for linethickness and, in general, how to factor the use
of units in the MathML world?
ID  Description
em  ems (font-relative unit traditionally used for horizontal lengths)
ex  exs (font-relative unit traditionally used for vertical lengths)
px  pixels, or pixel size of a "typical computer display"
in  inches (1 inch = 2.54 centimeters)
cm  centimeters
mm  millimeters
pt  points (1 point = 1/72 inch)
pc  picas (1 pica = 12 points)
%   percentage of default value
*/

// default fraction line thickness in pixels
#define DEFAULT_FRACTION_LINE_THICKNESS 1

#define THIN_FRACTION_LINE_THICKNESS    1
#define MEDIUM_FRACTION_LINE_THICKNESS  2
#define THICK_FRACTION_LINE_THICKNESS   4

// use an upper bound just in case someone set a too high value? why bother?
// #define MAX_FRACTION_LINE_THICKNESS 5 

class nsMathMLmfracFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmfracFrame(nsIFrame** aNewFrame);

  NS_IMETHOD
  Init(nsIPresContext&  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Reflow(nsIPresContext&          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD 
  Paint(nsIPresContext&      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext& aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;
    rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    UpdatePresentationDataFromChildAt(0, 1, PR_FALSE);
    ReResolveStyleContext(&aPresContext, mStyleContext, NS_STYLE_HINT_REFLOW, nsnull, nsnull);
    return rv;
  }

  void
  SetLineThickness(const nscoord aLineThickness) {
     mLineThickness = aLineThickness;
  }

  void 
  SetLineOrigin(const nsPoint& aOrigin) {
     mLineOrigin = aOrigin;
  }

protected:
  nsMathMLmfracFrame();
  virtual ~nsMathMLmfracFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
  
  nsPoint mLineOrigin;  
  nscoord mLineThickness;
};

#endif /* nsMathMLmfracFrame_h___ */
