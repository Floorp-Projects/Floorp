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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsFrame.h"
#include "nsLineLayout.h"
#include "nsHTMLIIDs.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsXULAtoms.h"
#include "nsUnitConversion.h"
#include "nsIStyleContext.h"
#include "nsIContent.h"
#include "nsStyleConsts.h"
#include "nsINameSpaceManager.h"
#include "nsTreeIndentationFrame.h"
#include "nsCOMPtr.h"


nsTreeIndentationFrame::nsTreeIndentationFrame()
{
	mWidth = 0;
	mHaveComputedWidth = PR_FALSE;
}

nsresult
NS_NewTreeIndentationFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTreeIndentationFrame* it = new (aPresShell) nsTreeIndentationFrame();
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsTreeIndentationFrame::~nsTreeIndentationFrame()
{
}

NS_IMETHODIMP
nsTreeIndentationFrame::Reflow(nsIPresContext*          aPresContext,
							   nsHTMLReflowMetrics&     aMetrics,
							   const nsHTMLReflowState& aReflowState,
							   nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsTreeIndentationFrame", aReflowState.reason);
  aStatus = NS_FRAME_COMPLETE;

  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  // Compute our width based on the depth of our node within the content model
  if (!mHaveComputedWidth) {
    mWidth = 0;
	  nscoord level = 0;
	  
	  // First climb out to the tree item level.
	  nsIFrame* aFrame = this;
	  nsCOMPtr<nsIContent> content;
	  aFrame->GetContent(getter_AddRefs(content));
	  nsCOMPtr<nsIAtom> tag;
	  content->GetTag(*getter_AddRefs(tag));
		while (aFrame && tag && tag.get() != nsXULAtoms::treeitem) {
			aFrame->GetParent(&aFrame);
			
			// nsCOMPtr correctly handles releasing the old |content| and |tag|
			aFrame->GetContent(getter_AddRefs(content));
			content->GetTag(*getter_AddRefs(tag));
		}

		// We now have a tree row content node. Start counting our level of nesting.
		nsCOMPtr<nsIContent> parentContent;
		while (tag.get() != nsXULAtoms::tree && tag.get() != nsXULAtoms::treehead) {
			content->GetParent(*getter_AddRefs(parentContent));

			parentContent->GetTag(*getter_AddRefs(tag));
			content = parentContent;
			
			++level;
		}

		level = (level/2) - 1;
    if (level < 0) level = 0;

		mWidth = level*16; // Hardcode an indentation of 16 pixels for now. TODO: Make this a parameter or something
  }

  float p2t;
  aPresContext->GetScaledPixelsToTwips(&p2t);
 
  if (0 != mWidth) {
      aMetrics.width = NSIntPixelsToTwips(mWidth, p2t);
  }
  
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  return NS_OK;
}
