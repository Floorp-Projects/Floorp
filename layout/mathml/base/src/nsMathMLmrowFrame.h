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

#ifndef nsMathMLmrowFrame_h___
#define nsMathMLmrowFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mrow> -- horizontally group any number of subexpressions 
//

class nsMathMLmrowFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmrowFrame(nsIFrame** aNewFrame);
  
  NS_IMETHOD
  Init(nsIPresContext&  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  // We override the nsMathMLContainerFrame::ReResolveStyleContext() method
  // because this container <tag>base arguments</tag> does not need its
  // 'arguments' in a smaller font size that the 'base'.
  NS_IMETHOD
  ReResolveStyleContext(nsIPresContext*    aPresContext, 
                        nsIStyleContext*   aParentContext,
                        PRInt32            aParentChange, 
                        nsStyleChangeList* aChangeList,
                        PRInt32*           aLocalChange)
  {
    return nsHTMLContainerFrame::ReResolveStyleContext(aPresContext, aParentContext, aParentChange, aChangeList, aLocalChange);
  }

protected:
  nsMathMLmrowFrame();
  virtual ~nsMathMLmrowFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
};

#endif /* nsMathMLmrowFrame_h___ */
