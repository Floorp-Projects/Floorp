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

#ifndef nsMathMLmrootFrame_h___
#define nsMathMLmrootFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <msqrt> and <mroot> -- form a radical
//

class nsMathMLmrootFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmrootFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  SetAdditionalStyleContext(PRInt32          aIndex, 
                            nsIStyleContext* aStyleContext);
  NS_IMETHOD
  GetAdditionalStyleContext(PRInt32           aIndex, 
                            nsIStyleContext** aStyleContext) const;

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList)
  {
    nsresult rv;
    rv = nsMathMLContainerFrame::SetInitialChildList(aPresContext, aListName, aChildList);
    // 1. The REC says:
    //    The <mroot> element increments scriptlevel by 2, and sets displaystyle to
    //    "false", within index, but leaves both attributes unchanged within base.
    // 2. The TeXbook (Ch 17. p.141) says \sqrt is compressed
    UpdatePresentationDataFromChildAt(aPresContext, 1, 1, 2,
      ~NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED,
       NS_MATHML_DISPLAYSTYLE | NS_MATHML_COMPRESSED);
    UpdatePresentationDataFromChildAt(aPresContext, 0, 0, 0,
       NS_MATHML_COMPRESSED, NS_MATHML_COMPRESSED);
    return rv;
  }

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD 
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

protected:
  nsMathMLmrootFrame();
  virtual ~nsMathMLmrootFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  nsMathMLChar mSqrChar;
  nsRect       mBarRect;
};

#endif /* nsMathMLmrootFrame_h___ */
