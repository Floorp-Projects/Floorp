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

#include "nsTableRowFrame.h"
#include "nsVoidArray.h"

class nsTreeCellFrame;
class nsTreeRowGroupFrame;
class nsTableColFrame;

class nsTreeRowFrame : public nsTableRowFrame
{
public:
  friend nsresult NS_NewTreeRowFrame(nsIFrame** aNewFrame);

  // Overridden methods
  NS_IMETHOD     IR_TargetIsChild(nsIPresContext&      aPresContext,
                                      nsHTMLReflowMetrics& aDesiredSize,
                                      RowReflowState& aReflowState,
                                      nsReflowStatus&      aStatus,
                                      nsIFrame *           aNextFrame);

  NS_IMETHOD     Reflow(nsIPresContext&          aPresContext,
							         nsHTMLReflowMetrics&     aDesiredSize,
							         const nsHTMLReflowState& aReflowState,
							         nsReflowStatus&          aStatus);

  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow); // Overridden to set whether we're a column header 

  NS_IMETHOD GetFrameForPoint(const nsPoint& aPoint, // Overridden to capture events
                              nsIFrame**     aFrame);

  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext,
                        nsGUIEvent* aEvent,
                        nsEventStatus& aEventStatus);

  NS_IMETHOD HandleMouseUpEvent(nsIPresContext& aPresContext,
                                nsGUIEvent* aEvent,
                                nsEventStatus& aEventStatus);

  NS_IMETHOD HandleHeaderDragEvent(nsIPresContext& aPresContext,
                                   nsGUIEvent* aEvent,
                                   nsEventStatus& aEventStatus);

  NS_IMETHOD GetCursor(nsIPresContext& aPresContext,
                                     nsPoint&        aPoint,
                                     PRInt32&        aCursor);

  NS_IMETHOD HeaderDrag(PRBool aGrabber);
  PRBool DraggingHeader() { return mDraggingHeader; };

  void SetFlexingColumn(nsTableColFrame* aTableColFrame) { mFlexingCol = aTableColFrame; };
  void SetHeaderPosition(PRInt32 aHeaderPos) { mHeaderPosition = aHeaderPos; };

protected:
  nsTreeRowFrame();
  virtual ~nsTreeRowFrame();

protected: // Data Members
  PRBool mIsHeader;
  PRInt32 mGeneration;
  PRBool mDraggingHeader;
  nsIFrame* mHitFrame;
  nsTableColFrame* mFlexingCol;
  PRInt32 mHeaderPosition;
}; // class nsTreeRowFrame
