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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsCOMPtr.h"
#include "nsXULTreeCellFrame.h"
#include "nsXULAtoms.h"
#include "nsIContent.h"
#include "nsIStyleContext.h"
#include "nsINamespaceManager.h" 

//
// NS_NewXULTreeCellFrame
//
// Creates a new TreeCell frame
//
nsresult
NS_NewXULTreeCellFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                        nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeCellFrame* it = new (aPresShell) nsXULTreeCellFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeCellFrame


// Constructor
nsXULTreeCellFrame::nsXULTreeCellFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal) 
{}

// Destructor
nsXULTreeCellFrame::~nsXULTreeCellFrame()
{
}

NS_IMETHODIMP
nsXULTreeCellFrame::GetFrameForPoint(nsIPresContext* aPresContext,
                                     const nsPoint& aPoint, 
                                     nsFramePaintLayer aWhichLayer,
                                     nsIFrame**     aFrame)
{
  nsAutoString value;
  mContent->GetAttribute(kNameSpaceID_None, nsXULAtoms::allowevents, value);
  if (value.EqualsWithConversion("true"))
  {
    return nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
  }
  else
  {
    if (! ( mRect.Contains(aPoint) || ( mState & NS_FRAME_OUTSIDE_CHILDREN)) )
    {
      return NS_ERROR_FAILURE;
    }
    nsresult result = nsBoxFrame::GetFrameForPoint(aPresContext, aPoint, aWhichLayer, aFrame);
    nsCOMPtr<nsIContent> content;
    if (result == NS_OK) {
      (*aFrame)->GetContent(getter_AddRefs(content));
      if (content) {
        // This allows selective overriding for subcontent.
        content->GetAttribute(kNameSpaceID_None, nsXULAtoms::allowevents, value);
        if (value.EqualsWithConversion("true"))
          return result;
      }
    }
    if (mRect.Contains(aPoint)) {
      const nsStyleDisplay* disp = (const nsStyleDisplay*)
        mStyleContext->GetStyleData(eStyleStruct_Display);
      if (disp->IsVisible()) {
        *aFrame = this; // Capture all events so that we can perform selection and expand/collapse.
        return NS_OK;
      }
    }
    return NS_ERROR_FAILURE;
  }
}