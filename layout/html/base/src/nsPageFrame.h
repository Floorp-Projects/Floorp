/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef nsPageFrame_h___
#define nsPageFrame_h___

#include "nsHTMLContainerFrame.h"

// Page frame class used by the simple page sequence frame
class nsPageFrame : public nsContainerFrame {
public:
  friend nsresult NS_NewPageFrame(nsIFrame** aResult);

  NS_IMETHOD  Reflow(nsIPresContext&      aPresContext,
                     nsHTMLReflowMetrics& aDesiredSize,
                     const nsHTMLReflowState& aMaxSize,
                     nsReflowStatus&      aStatus);
  NS_IMETHOD IsPercentageBase(PRBool& aBase) const;

  /**
   * Get the "type" of the frame
   *
   * @see nsLayoutAtoms::pageFrame
   */
  NS_IMETHOD GetFrameType(nsIAtom** aType) const;
  
#ifdef DEBUG
  // Debugging
  NS_IMETHOD  GetFrameName(nsString& aResult) const;
#endif

protected:
  nsPageFrame();
};

#endif /* nsPageFrame_h___ */

