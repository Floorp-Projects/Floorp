/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsHTMLContainerFrame_h___
#define nsHTMLContainerFrame_h___

#include "nsContainerFrame.h"
class nsString;
class nsAbsoluteFrame;
class nsPlaceholderFrame;
struct nsStyleDisplay;
struct nsStylePosition;

// Base class for html container frames that provides common
// functionality.
class nsHTMLContainerFrame : public nsContainerFrame {
public:
  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect);
  nsPlaceholderFrame* CreatePlaceholderFrame(nsIPresContext& aPresContext,
                                             nsIFrame*       aFloatedFrame);
  nsAbsoluteFrame* CreateAbsolutePlaceholderFrame(nsIPresContext& aPresContext,
                                                  nsIFrame*       aAbsoluteFrame);
  PRBool CreateWrapperFrame(nsIPresContext& aPresContext,
                            nsIFrame*       aFrame,
                            nsIFrame*&      aWrapperFrame);

  // If the frame should be floated or absolutely positioned creates a placeholder
  // frame and returns PR_TRUE. The sibling list is modified so aFrame's next
  // sibling pointer is set to nsnull, and aPlaceholderFrame's sibling pointer
  // is set to what aFrame's sibling pointer was. It's up to the caller to adjust
  // any previous sibling pointers.
  PRBool MoveFrameOutOfFlow(nsIPresContext&        aPresContext,
                            nsIFrame*              aFrame,
                            const nsStyleDisplay*  aDisplay,
                            const nsStylePosition* aPosition,
                            nsIFrame*&             aPlaceholderFrame);

  /* helper methods for incremental reflow */
  /** */
  NS_IMETHOD AddFrame(const nsHTMLReflowState& aReflowState,
                      nsIFrame *               aAddedFrame);
  /** */
  NS_IMETHOD RemoveFrame(nsIFrame * aRemovedFrame);


  // Helper method to create next-in-flows if necessary
  static nsresult CreateNextInFlow(nsIPresContext& aPresContext,
                                   nsIFrame* aOuterFrame,
                                   nsIFrame* aFrame,
                                   nsIFrame*& aNextInFlowResult);

  // Helper method to wrap views around frames. Used by containers
  // under special circumstances (can be used by leaf frames as well)
  static nsresult CreateViewForFrame(nsIPresContext& aPresContext,
                                     nsIFrame* aFrame,
                                     nsIStyleContext* aStyleContext,
                                     PRBool aForce);

  static void UpdateStyleContexts(nsIPresContext& aPresContext,
                                  nsIFrame* aFrame,
                                  nsIFrame* aOldParent,
                                  nsIFrame* aNewParent);

protected:
  virtual PRIntn GetSkipSides() const = 0;
};

#endif /* nsHTMLContainerFrame_h___ */

