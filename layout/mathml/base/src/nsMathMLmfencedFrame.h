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

#ifndef nsMathMLmfencedFrame_h___
#define nsMathMLmfencedFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"

//
// <mfenced> -- surround content with a pair of fences
//

class nsMathMLmfencedFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmfencedFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame);

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
  nsMathMLmfencedFrame();
  virtual ~nsMathMLmfencedFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  nsMathMLChar* mOpenChar;
  nsMathMLChar* mCloseChar;
  nsMathMLChar* mSeparatorsChar;
  PRInt32       mSeparatorsCount;

  // helper routines to format the MathMLChars involved here
  static nsresult
  ReflowChar(nsIPresContext*      aPresContext,
             nsIRenderingContext& aRenderingContext,
             nsMathMLChar*        aMathMLChar,
             nsOperatorFlags      aForm,
             PRInt32              aScriptLevel,
             nscoord              fontAscent,
             nscoord              fontDescent,
             nscoord              axisHeight,
             nscoord              em,
             nsBoundingMetrics&   aContainerSize,
             nsHTMLReflowMetrics& aDesiredSize);

  static void
  PlaceChar(nsMathMLChar*      aMathMLChar,
            nscoord            aFontAscent,
            nscoord            aDesiredAscent,
            nsBoundingMetrics& bm,
            nscoord&           dx);

  // clean up
  void
  RemoveFencesAndSeparators();

  // add fences and separators when all child frames are known
  nsresult
  CreateFencesAndSeparators(nsIPresContext* aPresContext);
};

#endif /* nsMathMLmfencedFrame_h___ */
