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
class nsPlaceholderFrame;

// Base class for html container frames that provides common
// functionality.
class nsHTMLContainerFrame : public nsContainerFrame {
public:
  nsHTMLContainerFrame(nsIContent* aContent,
                       nsIFrame* aParent);

  NS_IMETHOD  Paint(nsIPresContext& aPresContext,
                    nsIRenderingContext& aRenderingContext,
                    const nsRect& aDirtyRect);
  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent* aEvent,
                          nsEventStatus& aEventStatus);
  NS_IMETHOD  GetCursorAndContentAt(nsIPresContext& aPresContext,
                          const nsPoint& aPoint,
                          nsIFrame** aFrame,
                          nsIContent** aContent,
                          PRInt32& aCursor);

  NS_IMETHOD ContentDeleted(nsIPresShell*   aShell,
                            nsIPresContext* aPresContext,
                            nsIContent*     aContainer,
                            nsIContent*     aChild,
                            PRInt32         aIndexInParent);

  NS_IMETHOD AttributeChanged(nsIPresShell* aShell,
                              nsIPresContext* aPresContext,
                              nsIContent* aChild,
                              nsIAtom* aAttribute);

  nsresult WrapFrames(nsIPresContext& aPresContext,
                      nsIFrame* aPrevFrame, nsIFrame* aFrame);

  nsPlaceholderFrame* CreatePlaceholderFrame(nsIPresContext& aPresContext,
                                             nsIFrame*       aFloatedFrame);

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

  // Helper methods for handling attribute changes
  static void ApplyStyleChangeToTree(nsIPresContext& aPresContext,
                                     nsIFrame* aFrame);
  static void ApplyRenderingChangeToTree(nsIPresContext& aPresContext,
                                         nsIFrame* aFrame);
  static void ApplyReflowChangeToTree(nsIPresContext& aPresContext,
                                      nsIFrame* aFrame);

protected:
  virtual ~nsHTMLContainerFrame();

  virtual PRIntn GetSkipSides() const = 0;
};

#endif /* nsHTMLContainerFrame_h___ */

