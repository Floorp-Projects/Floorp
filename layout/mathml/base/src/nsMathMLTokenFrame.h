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

#ifndef nsMathMLTokenFrame_h___
#define nsMathMLTokenFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// Base class to handle token elements
//

class nsMathMLTokenFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLTokenFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  virtual nsIAtom* GetType() const;

  NS_IMETHOD
  Init(nsIPresContext*  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsStyleContext*  aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  SetInitialChildList(nsIPresContext* aPresContext,
                      nsIAtom*        aListName,
                      nsIFrame*       aChildList);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  ReflowDirtyChild(nsIPresShell* aPresShell,
                   nsIFrame*     aChild);

  NS_IMETHOD
  AttributeChanged(nsIPresContext* aPresContext,
                   nsIContent*     aChild,
                   PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType);
protected:
  nsMathMLTokenFrame();
  virtual ~nsMathMLTokenFrame();

  virtual PRIntn GetSkipSides() const { return 0; }

  // hook to perform MathML-specific actions depending on the tag
  virtual void
  ProcessTextData(nsIPresContext* aPresContext);

  // helper to set the style of <mi> which has to be italic or normal
  // depending on its textual content
  void SetTextStyle(nsIPresContext* aPresContext);

  // helper to set the quotes of <ms>
  void SetQuotes(nsIPresContext* aPresContext);
};

#endif /* nsMathMLTokentFrame_h___ */
