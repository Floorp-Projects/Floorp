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
#include "nsINameSpaceManager.h" 
#include "nsIXULTreeSlice.h"
#include "nsIMonument.h"
#include "nsIBoxLayout.h"
#include "nsMonumentLayout.h"
#include "nsTempleLayout.h"


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

    // If we are in either the first 2 pixels or the last 2 pixels, we're going to
    // do something really strange.  Check for an adjacent splitter.
    PRBool left = PR_FALSE;
    PRBool right = PR_FALSE;
    if (mRect.x + mRect.width - 60 < aPoint.x)
      right = PR_TRUE;
    else if (mRect.x + 60 > aPoint.x)
      left = PR_TRUE;

    if (left || right) {
      // Figure out if we're a header.
      nsIFrame* grandpa;
      mParent->GetParent(&grandpa);
      nsCOMPtr<nsIXULTreeSlice> slice(do_QueryInterface(grandpa));
      if (!slice) {
        // We are a header.
        // Determine our index.
        nsCOMPtr<nsIContent> par;
        mParent->GetContent(getter_AddRefs(par));
        PRInt32 i;
        par->IndexOf(mContent, i);

        // Now find the nth column in the other temple.
        nsCOMPtr<nsIBox> box(do_QueryInterface(mParent));
        nsCOMPtr<nsIBoxLayout> lm;
        box->GetLayoutManager(getter_AddRefs(lm));
        nsCOMPtr<nsIMonument> mon(do_QueryInterface(lm));
        nsTempleLayout* temple = nsnull;
        nsIBox* templeBox = nsnull;
        mon->GetOtherTemple(box, &temple, &templeBox);
        NS_IF_RELEASE(temple);
        
        nsMonumentIterator iter(templeBox);
        nsIBox* child = nsnull;
        nsObeliskLayout* ob;
        PRInt32 currIndex = 0;
        if (left)
          i--;

        do {
          if (i < 0) break;

          iter.GetNextObelisk(&ob, PR_TRUE);
          iter.GetBox(&child);

          if (currIndex >= i)
            break;

          currIndex++;

        } while (child);

        nsIBox* splitBox = nsnull;
        if (child) 
          child->GetNextBox(&splitBox);
        nsIFrame* splitter = nsnull;
        if (splitBox)
          splitBox->QueryInterface(NS_GET_IID(nsIFrame), (void**)&splitter); 
        nsCOMPtr<nsIAtom> tag;
        nsCOMPtr<nsIContent> content;
        if (splitter) {
          splitter->GetContent(getter_AddRefs(content));
          content->GetTag(*getter_AddRefs(tag));
          if (tag.get() == nsXULAtoms::splitter) {
            *aFrame = splitter;
            return NS_OK;
          }
        }
      }
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

