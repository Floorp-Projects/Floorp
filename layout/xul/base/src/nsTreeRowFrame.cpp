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

#include "nsCOMPtr.h"
#include "nsTreeFrame.h"
#include "nsTreeRowFrame.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsCSSRendering.h"
#include "nsTreeCellFrame.h"
#include "nsCellMap.h"
#include "nsIDOMXULTreeElement.h"
#include "nsINameSpaceManager.h"
#include "nsTreeRowGroupFrame.h"
#include "nsXULAtoms.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMXULTreeElement.h"
#include "nsTreeTwistyListener.h"

//
// NS_NewTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewTreeRowFrame (nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeRowFrame* it = new nsTreeRowFrame;
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewTreeRowFrame


// Constructor
nsTreeRowFrame::nsTreeRowFrame()
:nsTableRowFrame()
{ }

// Destructor
nsTreeRowFrame::~nsTreeRowFrame()
{
}

NS_METHOD nsTreeRowFrame::IR_TargetIsChild(nsIPresContext&      aPresContext,
                                            nsHTMLReflowMetrics& aDesiredSize,
                                            RowReflowState&      aReflowState,
                                            nsReflowStatus&      aStatus,
                                            nsIFrame *           aNextFrame)
{
  nsRect rect;
  GetRect(rect);
  nsresult rv = nsTableRowFrame::IR_TargetIsChild(aPresContext, aDesiredSize, aReflowState, aStatus, aNextFrame);
  if (rv == NS_OK) {
    // Find out if our height changed.  With the tree widget, changing the height of a row is a
    // big deal, since it may force us to dynamically isntantiate newly exposed frames.
    if (rect.height != aDesiredSize.height) {
      // Retrieve the table frame and invalidate the cell map.
      nsTableFrame* tableFrame = nsnull;
      nsTableFrame::GetTableFrame(this, tableFrame);
  
      tableFrame->InvalidateCellMap(); 
    }
  }
  return rv;
}
