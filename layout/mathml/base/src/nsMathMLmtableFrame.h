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
 */

#ifndef nsMathMLmtableFrame_h___
#define nsMathMLmtableFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mtable> -- table or matrix
//

class nsMathMLmtableOuterFrame : public nsTableOuterFrame,
                                 public nsMathMLFrame
{
public:
  friend nsresult NS_NewMathMLmtableOuterFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS_INHERITED

  // Overloaded nsIMathMLFrame methods

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext*  aPresContext,
                       nsIStyleContext* aParentContext,
                       PRInt32          aParentScriptLevel);

  // overloaded nsTableOuterFrame methods

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

protected:
  nsMathMLmtableOuterFrame();
  virtual ~nsMathMLmtableOuterFrame();
}; // class nsMathMLmtableOuterFrame


// --------------

class nsMathMLmtdInnerFrame : public nsBlockFrame,
                              public nsMathMLFrame {
public:
  friend nsresult NS_NewMathMLmtdInnerFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext*  aPresContext,
                       nsIStyleContext* aParentContext,
                       PRInt32          aParentScriptLevel);

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

protected:
  nsMathMLmtdInnerFrame();
  virtual ~nsMathMLmtdInnerFrame();

  virtual PRIntn GetSkipSides() const { return 0; }
};  // class nsMathMLmtdInnerFrame

#endif /* nsMathMLmtableFrame_h___ */
