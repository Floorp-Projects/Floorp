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

#ifndef nsMathMLmspaceFrame_h___
#define nsMathMLmspaceFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mspace> -- space
//

class nsMathMLmspaceFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmspaceFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);
  
protected:
  nsMathMLmspaceFrame();
  virtual ~nsMathMLmspaceFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nscoord mWidth;
  nscoord mHeight;
  nscoord mDepth;

  // helper method to initialize our member data
  void 
  ProcessAttributes(nsIPresContext* aPresContext);
};

#endif /* nsMathMLmspaceFrame_h___ */
