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
	width = 0;
	haveComputedWidth = PR_FALSE;
}

nsresult
NS_NewTreeIndentationFrame(nsIFrame*& aResult)
{
  nsIFrame* frame = new nsTreeIndentationFrame();
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}

nsTreeIndentationFrame::~nsTreeIndentationFrame()
{
}

NS_IMETHODIMP
nsTreeIndentationFrame::Reflow(nsIPresContext&          aPresContext,
							   nsHTMLReflowMetrics&     aMetrics,
							   const nsHTMLReflowState& aReflowState,
							   nsReflowStatus&          aStatus)
{
  aStatus = NS_FRAME_COMPLETE;

  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  nscoord width = 0;
  
  // Compute our width based on the depth of our node within the content model
  if (!haveComputedWidth)
  {
	  nscoord level = 0;
	  
	  // First climb out to the tree row level.
	  nsIFrame* aFrame = this;
	  nsCOMPtr<nsIContent> pContent;
	  aFrame->GetContent(getter_AddRefs(pContent));
	  nsCOMPtr<nsIAtom> pTag;
	  pContent->GetTag(*getter_AddRefs(pTag));
	  if (pTag)
	  {
		  while (aFrame && pTag && pTag.get() != nsXULAtoms::treeitem)
		  {
			  aFrame->GetParent(&aFrame);
			  
			  // nsCOMPtr correctly handles releasing the old |pContent| and |pTag|
			  aFrame->GetContent(getter_AddRefs(pContent));
			  pContent->GetTag(*getter_AddRefs(pTag));
		  }

		  // We now have a tree row content node. Start counting our level of nesting.
		  nsCOMPtr<nsIContent> pParentContent;
		  while (pTag.get() != nsXULAtoms::treebody && pTag.get() != nsXULAtoms::treehead)
		  {
			  pContent->GetParent(*getter_AddRefs(pParentContent));

			  pParentContent->GetTag(*getter_AddRefs(pTag));
			  pContent = pParentContent;
			  
			  ++level;
		  }

		  level = (level-1)/2;

		  width = level*16; // Hardcode an indentation of 16 pixels for now. TODO: Make this a parameter or something
	  }
  }

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
 
  if (0 != width) {
      aMetrics.width = NSIntPixelsToTwips(width, p2t);
  }
  
  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  return NS_OK;
}
