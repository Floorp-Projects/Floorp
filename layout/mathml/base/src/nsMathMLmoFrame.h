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

#ifndef nsMathMLmoFrame_h___
#define nsMathMLmoFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mo> -- operator, fence, or separator
//

class nsMathMLmoFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmoFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

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
                      nsIFrame*       aChildList);

  NS_IMETHOD
  Paint(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer,
        PRUint32             aFlags = 0);

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

  // This method is called by the parent frame to ask <mo> 
  // to stretch itself.
  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize);

  // helper method to lookup the operator dictionary and initialize our member data
  void 
  InitData(nsIPresContext* aPresContext);

protected:
  nsMathMLmoFrame();
  virtual ~nsMathMLmoFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  nsMathMLChar     mMathMLChar; // Here is the MathMLChar that will deal with the operator.

  nsOperatorFlags  mFlags;
  float            mLeftSpace;
  float            mRightSpace;
  float            mMinSize;
  float            mMaxSize;
};

#endif /* nsMathMLmoFrame_h___ */
