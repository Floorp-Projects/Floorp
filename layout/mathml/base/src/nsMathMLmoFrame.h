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

#ifndef nsMathMLmoFrame_h___
#define nsMathMLmoFrame_h___

#include "nsCOMPtr.h"
#include "nsMathMLContainerFrame.h"
#include "nsMathMLChar.h"

//
// <mo> -- operator, fence, or separator
//

class nsMathMLmoFrame : public nsMathMLContainerFrame {
public:
  friend nsresult NS_NewMathMLmoFrame(nsIFrame** aNewFrame);
  
  NS_IMETHOD
  Init(nsIPresContext&  aPresContext,
       nsIContent*      aContent,
       nsIFrame*        aParent,
       nsIStyleContext* aContext,
       nsIFrame*        aPrevInFlow);

  NS_IMETHOD
  Paint(nsIPresContext&      aPresContext,
        nsIRenderingContext& aRenderingContext,
        const nsRect&        aDirtyRect,
        nsFramePaintLayer    aWhichLayer);

  // This method is called by the parent frame to ask <mo> 
  // to stretch itself.
  NS_IMETHOD
  Stretch(nsIPresContext&    aPresContext,
          nsStretchDirection aStretchDirection,
          nsCharMetrics&     aContainerSize,
          nsCharMetrics&     aDesiredStretchSize);

protected:
  nsMathMLmoFrame();
  virtual ~nsMathMLmoFrame();
  
  virtual PRIntn GetSkipSides() const { return 0; }

  PRBool mOperator; // True if we are listed in the operator dictionary.
  nsMathMLChar mMathMLChar; // Here is the MathChar that will deal with the operator.
};

#endif /* nsMathMLmoFrame_h___ */
