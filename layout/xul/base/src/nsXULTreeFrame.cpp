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
#include "nsXULTreeFrame.h"
#include "nsIScrollableFrame.h"
#include "nsIDOMElement.h"
#include "nsXULTreeOuterGroupFrame.h"
#include "nsXULAtoms.h"

//
// NS_NewXULTreeFrame
//
// Creates a new tree frame
//
nsresult
NS_NewXULTreeFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame, PRBool aIsRoot, 
                   nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsXULTreeFrame* it = new (aPresShell) nsXULTreeFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  *aNewFrame = it;
  return NS_OK;
  
} // NS_NewXULTreeFrame


// Constructor
nsXULTreeFrame::nsXULTreeFrame(nsIPresShell* aPresShell, PRBool aIsRoot, nsIBoxLayout* aLayoutManager, PRBool aIsHorizontal)
:nsBoxFrame(aPresShell, aIsRoot, aLayoutManager, aIsHorizontal) 
{
  mPresShell = aPresShell;
}

// Destructor
nsXULTreeFrame::~nsXULTreeFrame()
{
}

NS_IMETHODIMP_(nsrefcnt) 
nsXULTreeFrame::AddRef(void)
{
  return NS_OK;
}

NS_IMETHODIMP_(nsrefcnt)
nsXULTreeFrame::Release(void)
{
  return NS_OK;
}

//
// QueryInterface
//
NS_INTERFACE_MAP_BEGIN(nsXULTreeFrame)
  NS_INTERFACE_MAP_ENTRY(nsITreeFrame)
NS_INTERFACE_MAP_END_INHERITING(nsBoxFrame)

static void
GetImmediateChild(nsIContent* aParent, nsIAtom* aTag, nsIContent** aResult) 
{
  *aResult = nsnull;
  PRInt32 childCount;
  aParent->ChildCount(childCount);
  for (PRInt32 i = 0; i < childCount; i++) {
    nsCOMPtr<nsIContent> child;
    aParent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIAtom> tag;
    child->GetTag(*getter_AddRefs(tag));
    if (aTag == tag.get()) {
      *aResult = child;
      NS_ADDREF(*aResult);
      return;
    }
  }

  return;
}

NS_IMETHODIMP
nsXULTreeFrame::DoLayout(nsBoxLayoutState& aBoxLayoutState)
{
  nsXULTreeOuterGroupFrame* treeOuterGroup = nsnull;
  GetTreeBody(&treeOuterGroup);

  if (treeOuterGroup)
    treeOuterGroup->SetLayingOut(PR_TRUE);

  nsresult rv = nsBoxFrame::DoLayout(aBoxLayoutState);

  if (treeOuterGroup)
    treeOuterGroup->SetLayingOut(PR_FALSE);

  return rv;
}

NS_IMETHODIMP
nsXULTreeFrame::EnsureRowIsVisible(PRInt32 aRowIndex)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* XULTreeOuterGroup = nsnull;
  GetTreeBody(&XULTreeOuterGroup);

  if (!XULTreeOuterGroup) return NS_OK;
  
  XULTreeOuterGroup->EnsureRowIsVisible(aRowIndex);
  return NS_OK;
}

/* void scrollToIndex (in long rowIndex); */
NS_IMETHODIMP 
nsXULTreeFrame::ScrollToIndex(PRInt32 aRowIndex)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* XULTreeOuterGroup = nsnull;
  GetTreeBody(&XULTreeOuterGroup);

  if (!XULTreeOuterGroup) return NS_OK;
  
  XULTreeOuterGroup->ScrollToIndex(aRowIndex);
  return NS_OK;
}

/* nsIDOMElement getNextItem (in nsIDOMElement startItem, in long delta); */
NS_IMETHODIMP 
nsXULTreeFrame::GetNextItem(nsIDOMElement *aStartItem, PRInt32 aDelta, nsIDOMElement **aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* treeOuterGroup = nsnull;
  GetTreeBody(&treeOuterGroup);

  if (!treeOuterGroup)
    return NS_OK; // No tree body. Just bail.

  nsCOMPtr<nsIContent> start(do_QueryInterface(aStartItem));
  nsCOMPtr<nsIContent> row;
  GetImmediateChild(start, nsXULAtoms::treerow, getter_AddRefs(row));

  nsCOMPtr<nsIContent> result;
  treeOuterGroup->FindNextRowContent(aDelta, row, nsnull, getter_AddRefs(result));
  if (!result)
    return NS_OK;

  nsCOMPtr<nsIContent> parent;
  result->GetParent(*getter_AddRefs(parent));
  if (!parent)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> item(do_QueryInterface(parent));
  *aResult = item;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

/* nsIDOMElement getPreviousItem (in nsIDOMElement startItem, in long delta); */
NS_IMETHODIMP 
nsXULTreeFrame::GetPreviousItem(nsIDOMElement* aStartItem, PRInt32 aDelta, nsIDOMElement** aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* treeOuterGroup = nsnull;
  GetTreeBody(&treeOuterGroup);

  if (!treeOuterGroup)
    return NS_OK; // No tree body. Just bail.

  nsCOMPtr<nsIContent> start(do_QueryInterface(aStartItem));
  nsCOMPtr<nsIContent> row;
  GetImmediateChild(start, nsXULAtoms::treerow, getter_AddRefs(row));

  nsCOMPtr<nsIContent> result;
  treeOuterGroup->FindPreviousRowContent(aDelta, row, nsnull, getter_AddRefs(result));
  if (!result)
    return NS_OK;

  nsCOMPtr<nsIContent> parent;
  result->GetParent(*getter_AddRefs(parent));
  if (!parent)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> item(do_QueryInterface(parent));
  *aResult = item;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

/* nsIDOMElement getItemAtIndex (in long index); */
NS_IMETHODIMP 
nsXULTreeFrame::GetItemAtIndex(PRInt32 aIndex, nsIDOMElement **aResult)
{
  *aResult = nsnull;

  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* treeOuterGroup = nsnull;
  GetTreeBody(&treeOuterGroup);

  if (!treeOuterGroup)
    return NS_OK; // No tree body. Just bail.

  nsCOMPtr<nsIContent> result;
  treeOuterGroup->FindRowContentAtIndex(aIndex, nsnull, getter_AddRefs(result));
  if (!result)
    return NS_OK;

  nsCOMPtr<nsIContent> parent;
  result->GetParent(*getter_AddRefs(parent));
  if (!parent)
    return NS_OK;

  nsCOMPtr<nsIDOMElement> item(do_QueryInterface(parent));
  *aResult = item;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

/* long getIndexOfItem (in nsIDOMElement item); */
NS_IMETHODIMP 
nsXULTreeFrame::GetIndexOfItem(nsIPresContext* aPresContext, nsIDOMElement* aElement, PRInt32* aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* treeOuterGroup = nsnull;
  GetTreeBody(&treeOuterGroup);

  if (!treeOuterGroup)
    return NS_OK; // No tree body. Just bail.

  *aResult = 0;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aElement));
  nsCOMPtr<nsIContent> root;
  treeOuterGroup->GetContent(getter_AddRefs(root));
  return treeOuterGroup->IndexOfItem(root, content, PR_FALSE, PR_TRUE, aResult);
}

/* void getNumberOfVisibleRows (); */
NS_IMETHODIMP 
nsXULTreeFrame::GetNumberOfVisibleRows(PRInt32 *aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* XULTreeOuterGroup = nsnull;
  GetTreeBody(&XULTreeOuterGroup);

  if (!XULTreeOuterGroup) return NS_OK;
  
  return XULTreeOuterGroup->GetNumberOfVisibleRows(aResult);
}

/* void getIndexOfFirstVisibleRow (); */
NS_IMETHODIMP 
nsXULTreeFrame::GetIndexOfFirstVisibleRow(PRInt32 *aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* XULTreeOuterGroup = nsnull;
  GetTreeBody(&XULTreeOuterGroup);

  if (!XULTreeOuterGroup) return NS_OK;
  
  return XULTreeOuterGroup->GetIndexOfFirstVisibleRow(aResult);
}

NS_IMETHODIMP 
nsXULTreeFrame::GetRowCount(PRInt32 *aResult)
{
  // Get our treechildren child frame.
  nsXULTreeOuterGroupFrame* XULTreeOuterGroup = nsnull;
  GetTreeBody(&XULTreeOuterGroup);

  if (!XULTreeOuterGroup) return NS_OK;
  
  return XULTreeOuterGroup->GetRowCount(aResult);
}

void
nsXULTreeFrame::GetTreeBody(nsXULTreeOuterGroupFrame** aResult)
{
  nsCOMPtr<nsIContent> child;
  PRInt32 count;
  mContent->ChildCount(count);
  for (PRInt32 i = 0; i < count; i++) {
    mContent->ChildAt(i, *getter_AddRefs(child));
    if (child) {
      nsCOMPtr<nsIAtom> tag;
      child->GetTag(*getter_AddRefs(tag));
      if (tag && tag.get() == nsXULAtoms::treechildren) {
        // This is our actual treechildren frame.
        nsIFrame* frame;
        mPresShell->GetPrimaryFrameFor(child, &frame);
        if (frame) {
          nsCOMPtr<nsIScrollableFrame> scroll(do_QueryInterface(frame));
          if (scroll) {
            scroll->GetScrolledFrame(nsnull, frame);
          }
          *aResult = (nsXULTreeOuterGroupFrame*)frame;
          return;
        }
      }
    }
  }
  *aResult = nsnull;
}

