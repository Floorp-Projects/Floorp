/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#ifndef nsScrollFrame_h___
#define nsScrollFrame_h___

#include "nsHTMLContainerFrame.h"

/**
 * The scroll frame creates and manages the scrolling view. It creates
 * a nsScrollViewFrame which handles padding and rendering of the
 * background.
 */
class nsScrollFrame : public nsHTMLContainerFrame {
public:
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext);

  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);

  NS_IMETHOD DidReflow(nsIPresContext&   aPresContext,
                       nsDidReflowStatus aStatus);

  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);

  NS_IMETHOD Paint(nsIPresContext&      aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect&        aDirtyRect,
                   nsFramePaintLayer    aWhichLayer);

  NS_IMETHOD GetFrameName(nsString& aResult) const;

  NS_IMETHOD ReResolveStyleContext(nsIPresContext* aPresContext, 
                                     nsIStyleContext* aParentContext);

protected:
  virtual PRIntn GetSkipSides() const;

private:
  nsresult CreateScrollingView();
};

#endif /* nsScrollFrame_h___ */

