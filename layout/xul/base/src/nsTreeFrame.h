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

#include "nsTableFrame.h"
#include "nsVoidArray.h"

class nsTreeCellFrame;

class nsTreeFrame : public nsTableFrame
{
public:
  friend nsresult NS_NewTreeFrame(nsIFrame** aNewFrame);

  void SetSelection(nsIPresContext& presContext, nsTreeCellFrame* pFrame);
  void ToggleSelection(nsIPresContext& presContext, nsTreeCellFrame* pFrame);
  void RangedSelection(nsIPresContext& aPresContext, nsTreeCellFrame* pEndFrame);
  
  void MoveUp(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveDown(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveLeft(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveRight(nsIPresContext& aPresContext, nsTreeCellFrame* pFrame);
  void MoveToRowCol(nsIPresContext& aPresContext, PRInt32 row, PRInt32 col, nsTreeCellFrame* pFrame);
    
  PRBool IsSlatedForReflow() { return mSlatedForReflow; };
  void SlateForReflow() { mSlatedForReflow = PR_TRUE; };

  // Overridden methods
  NS_IMETHOD Destroy(nsIPresContext& aPresContext);
  PRBool RowGroupsShouldBeConstrained() { return PR_TRUE; }
  
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
							         nsHTMLReflowMetrics&     aMetrics,
							         const nsHTMLReflowState& aReflowState,
							         nsReflowStatus&          aStatus);
  NS_IMETHOD HandleEvent(nsIPresContext& aPresContext, 
                             nsGUIEvent*     aEvent,
                             nsEventStatus&  aEventStatus);

protected:
  nsTreeFrame();
  virtual ~nsTreeFrame();

protected: // Data Members
  PRBool mSlatedForReflow; // If set, don't waste time scheduling excess reflows.

}; // class nsTreeFrame
