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

#include "nsTableCellFrame.h"

class nsTreeFrame;

class nsTreeCellFrame : public nsTableCellFrame
{
public:
  friend nsresult NS_NewTreeCellFrame(nsIFrame*& aNewFrame);

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, // Overridden to capture events
                              nsIFrame**     aFrame);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow); // Overridden to set whether we're a column header 

  NS_IMETHOD Reflow(nsIPresContext& aPresContext,
                    nsHTMLReflowMetrics& aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus& aStatus);

  NS_IMETHOD
  AttributeChanged(nsIPresContext* aPresContext,
                                  nsIContent* aChild,
                                  nsIAtom* aAttribute,
                                  PRInt32 aHint);

  void Select(nsIPresContext& presContext, PRBool isSelected, PRBool notifyForReflow = PR_TRUE);
  void Hover(nsIPresContext& presContext, PRBool isHover, PRBool notifyForReflow = PR_TRUE);

  nsTableFrame* GetTreeFrame();

  void SetAllowEvents(PRBool allowEvents) { mAllowEvents = allowEvents; };

protected:
  nsTreeCellFrame();
  virtual ~nsTreeCellFrame();

  nsresult HandleMouseDownEvent(nsIPresContext& aPresContext, 
								                nsGUIEvent*     aEvent,
							                  nsEventStatus&  aEventStatus);
  
  nsresult HandleMouseEnterEvent(nsIPresContext& aPresContext, 
								                nsGUIEvent*     aEvent,
							                  nsEventStatus&  aEventStatus);
  
  nsresult HandleMouseExitEvent(nsIPresContext& aPresContext, 
								                nsGUIEvent*     aEvent,
							                  nsEventStatus&  aEventStatus);

  nsresult HandleDoubleClickEvent(nsIPresContext& aPresContext, 
								                  nsGUIEvent*     aEvent,
							                    nsEventStatus&  aEventStatus);

protected:
  // Data members
  PRBool mIsHeader; // Whether or not we're a column header
  nsTreeFrame* mTreeFrame; // Our parent tree frame.
  PRBool mAllowEvents; // Whether we let events go through.

}; // class nsTableCellFrame
