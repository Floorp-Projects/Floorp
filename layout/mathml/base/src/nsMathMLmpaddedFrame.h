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

#ifndef nsMathMLmpaddedFrame_h___
#define nsMathMLmpaddedFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mpadded> -- adjust space around content  
//

class nsMathMLmpaddedFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmpaddedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

  NS_IMETHOD
  InheritAutomaticData(nsIPresContext* aPresContext,
                       nsIFrame*       aParent);

  NS_IMETHOD
  Reflow(nsIPresContext*          aPresContext,
         nsHTMLReflowMetrics&     aDesiredSize,
         const nsHTMLReflowState& aReflowState,
         nsReflowStatus&          aStatus);
  
protected:
  nsMathMLmpaddedFrame();
  virtual ~nsMathMLmpaddedFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

private:
  nsCSSValue mWidth;
  nsCSSValue mHeight;
  nsCSSValue mDepth;
  nsCSSValue mLeftSpace;

  PRInt32    mWidthSign;
  PRInt32    mHeightSign;
  PRInt32    mDepthSign;
  PRInt32    mLeftSpaceSign;

  PRInt32    mWidthPseudoUnit;
  PRInt32    mHeightPseudoUnit;
  PRInt32    mDepthPseudoUnit;
  PRInt32    mLeftSpacePseudoUnit;

  // helpers to process the attributes
  void
  ProcessAttributes(nsIPresContext* aPresContext);

  static PRBool
  ParseAttribute(nsString&   aString,
                 PRInt32&    aSign,
                 nsCSSValue& aCSSValue,
                 PRInt32&    aPseudoUnit);

  static void
  UpdateValue(nsIPresContext*      aPresContext,
              nsStyleContext*      aStyleContext,
              PRInt32              aSign,
              PRInt32              aPseudoUnit,
              nsCSSValue&          aCSSValue,
              nscoord              aLeftSpace,
              nsBoundingMetrics&   aBoundingMetrics,
              nscoord&             aValueToUpdate);
};

#endif /* nsMathMLmpaddedFrame_h___ */
