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
 *   Shyjan Mahamud <mahamud@cs.cmu.edu> (added TeX rendering rules)
 */

#ifndef nsMathMLmsubFrame_h___
#define nsMathMLmsubFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <msub> -- attach a subscript to a base
//

class nsMathMLmsubFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmsubFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

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

  static nsresult
  PlaceSubScript (nsIPresContext*      aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  PRBool               aPlaceOrigin,
                  nsHTMLReflowMetrics& aDesiredSize,
                  nsIFrame*            aForFrame,
                  nscoord              aUserSubScriptShift = 0,
                  nscoord              aScriptSpace = NSFloatPointsToTwips(0.5f));

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;
    rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    // 1. The REC says:
    // The <msub> element increments scriptlevel by 1, and sets displaystyle to
    // "false", within subscript, but leaves both attributes unchanged within base.
    // 2. The TeXbook (Ch 17. p.141) says the subscript is compressed
    UpdatePresentationDataFromChildAt(1, -1, 1,
      ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
       NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
    // switch the style of the subscript
    InsertScriptLevelStyleContext(aPresContext);
    // check whether or not this is an embellished operator
    EmbellishOperator();
    return rv;
  }

 protected:
  nsMathMLmsubFrame();
  virtual ~nsMathMLmsubFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nscoord mScriptSpace;  // scriptspace from TeX for extra spacing after sup/subscript
                         // = 0.5pt in plain TeX
  nscoord mSubScriptShift;
};

#endif /* nsMathMLmsubFrame_h___ */
