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

#ifndef nsMathMLmoverFrame_h___
#define nsMathMLmoverFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mover> -- attach an overscript to a base
//

class nsMathMLmoverFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmoverFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize);

  NS_IMETHOD
  InheritAutomaticData(nsIPresContext* aPresContext,
                       nsIFrame*       aParent);

  NS_IMETHOD
  TransmitAutomaticData(nsIPresContext* aPresContext);

  NS_IMETHOD
  UpdatePresentationData(nsIPresContext* aPresContext,
                         PRInt32         aScriptLevelIncrement,
                         PRUint32        aFlagsValues,
                         PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate);

  NS_IMETHOD
  AttributeChanged(nsIPresContext* aPresContext,
                   nsIContent*     aChild,
                   PRInt32         aNameSpaceID,
                   nsIAtom*        aAttribute,
                   PRInt32         aModType);

protected:
  nsMathMLmoverFrame();
  virtual ~nsMathMLmoverFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }
};


#endif /* nsMathMLmoverFrame_h___ */
