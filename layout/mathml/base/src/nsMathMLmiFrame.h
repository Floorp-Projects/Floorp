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

#ifndef nsMathMLmiFrame_h___
#define nsMathMLmiFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mi> -- identifier 
//

class nsMathMLmiFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmiFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  TransmitAutomaticData(nsIPresContext* aPresContext);

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

protected:
  nsMathMLmiFrame();
  virtual ~nsMathMLmiFrame();

  virtual PRIntn GetSkipSides() const { return 0; }

 /* <mi> needs to switch to a normal-font (rather than italics) if
  * its text content is not a single character (as per the MathML REC).
  * special care is also needed for style-invariant chars - bug 65951
  */
  void
  ProcessTextData(nsIPresContext* aPresContext);
};

#endif /* nsMathMLmiFrame_h___ */
