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

#ifndef nsMathMLFrame_h___
#define nsMathMLFrame_h___

#include "nsIPresContext.h"
#include "nsIRenderingContext.h"
#include "nsIStyleContext.h"
#include "nsMathMLOperators.h"
#include "nsIMathMLFrame.h"

// Concrete base class with default methods that derived MathML frames can override
class nsMathMLFrame : public nsIMathMLFrame {
public:

  // nsISupports ------

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD_(nsrefcnt) AddRef() {
    // not meaningfull for frames
    return 1;
  }

  NS_IMETHOD_(nsrefcnt) Release() {
    // not meaningfull for frames
    return 1;
  }

  // nsIMathMLFrame ---

  NS_IMETHOD
  GetBoundingMetrics(nsBoundingMetrics& aBoundingMetrics) {
    aBoundingMetrics = mBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  SetBoundingMetrics(const nsBoundingMetrics& aBoundingMetrics) {
    mBoundingMetrics = aBoundingMetrics;
    return NS_OK;
  }

  NS_IMETHOD
  GetReference(nsPoint& aReference) {
    aReference = mReference;
    return NS_OK;
  }

  NS_IMETHOD
  SetReference(const nsPoint& aReference) {
    mReference = aReference;
    return NS_OK;
  }

  NS_IMETHOD
  Stretch(nsIPresContext*      aPresContext,
          nsIRenderingContext& aRenderingContext,
          nsStretchDirection   aStretchDirection,
          nsBoundingMetrics&   aContainerSize,
          nsHTMLReflowMetrics& aDesiredStretchSize)
  {
    return NS_OK;
  }

  NS_IMETHOD
  Place(nsIPresContext*      aPresContext,
        nsIRenderingContext& aRenderingContext,
        PRBool               aPlaceOrigin,
        nsHTMLReflowMetrics& aDesiredSize)
  {
    return NS_OK;
  }

  NS_IMETHOD
  EmbellishOperator() {
    return NS_OK;
  }

  NS_IMETHOD
  GetEmbellishData(nsEmbellishData& aEmbellishData) {
    aEmbellishData = mEmbellishData;
    return NS_OK;
  }
 
  NS_IMETHOD
  SetEmbellishData(const nsEmbellishData& aEmbellishData) {
    mEmbellishData = aEmbellishData;
    return NS_OK;
  }

  NS_IMETHOD
  GetPresentationData(nsPresentationData& aPresentationData) {
    aPresentationData = mPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  SetPresentationData(const nsPresentationData& aPresentationData) {
    mPresentationData = aPresentationData;
    return NS_OK;
  }

  NS_IMETHOD
  UpdatePresentationData(PRInt32  aScriptLevelIncrement,
                         PRUint32 aFlagsValues,
                         PRUint32 aFlagsToUpdate);

  NS_IMETHOD
  UpdatePresentationDataFromChildAt(nsIPresContext* aPresContext,
                                    PRInt32         aFirstIndex,
                                    PRInt32         aLastIndex,
                                    PRInt32         aScriptLevelIncrement,
                                    PRUint32        aFlagsValues,
                                    PRUint32        aFlagsToUpdate)
  {
    return NS_OK;
  }

  NS_IMETHOD
  ReResolveScriptStyle(nsIPresContext*  aPresContext,
                       nsIStyleContext* aParentContext,
                       PRInt32          aParentScriptLevel)
  {
    return NS_OK;
  }

protected:
  // information about the presentation policy of the frame
  nsPresentationData mPresentationData;

  // information about a container that is an embellished operator
  nsEmbellishData mEmbellishData;
  
  // Metrics that _exactly_ enclose the text of the frame
  nsBoundingMetrics mBoundingMetrics;
  
  // Reference point of the frame: mReference.y is the baseline
  nsPoint mReference; 
};

#endif /* nsMathMLFrame_h___ */
