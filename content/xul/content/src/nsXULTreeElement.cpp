/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULTreeElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIFrame.h"
#include "nsIDOMElement.h"
#include "nsIComponentManager.h"
#include "nsITreeFrame.h"
#include "nsIDOMRange.h"
#include "nsIContentIterator.h"
#include "nsLayoutCID.h"
#include "nsString.h"
#include "nsITreeBoxObject.h"
#include "nsGUIEvent.h"

static NS_DEFINE_CID(kCRangeCID, NS_RANGE_CID);
static NS_DEFINE_IID(kCContentIteratorCID, NS_CONTENTITERATOR_CID);

nsIAtom*             nsXULTreeElement::kSelectedAtom;
nsIAtom*             nsXULTreeElement::kOpenAtom;
nsIAtom*             nsXULTreeElement::kTreeRowAtom;
nsIAtom*             nsXULTreeElement::kTreeItemAtom;
nsIAtom*             nsXULTreeElement::kTreeChildrenAtom;
nsIAtom*             nsXULTreeElement::kCurrentAtom;
int                  nsXULTreeElement::gRefCnt = 0;

NS_IMPL_ADDREF_INHERITED(nsXULTreeElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULTreeElement, nsXULAggregateElement);

nsresult
nsXULTreeElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULTreeElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULTreeElement*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIXULTreeContent))) {
        *aResult = NS_STATIC_CAST(nsIXULTreeContent*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}


nsXULTreeElement::nsXULTreeElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
  if (gRefCnt++ == 0) {
    kSelectedAtom    = NS_NewAtom("selected");
    kOpenAtom        = NS_NewAtom("open");
    kTreeRowAtom     = NS_NewAtom("treerow");
    kTreeItemAtom    = NS_NewAtom("treeitem");
    kTreeChildrenAtom= NS_NewAtom("treechildren");
    kCurrentAtom     = NS_NewAtom("current");
  }

  nsresult rv;

  nsRDFDOMNodeList* children;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedItems = children;

  mCurrentItem = nsnull;
  mSelectionStart = nsnull;
  mSuppressOnSelect = PR_FALSE;
}

nsXULTreeElement::~nsXULTreeElement()
{
#ifdef DEBUG_REFS
    --gInstanceCount;
    fprintf(stdout, "%d - RDF: nsXULTreeElement\n", gInstanceCount);
#endif

  if (mSelectTimer)
    mSelectTimer->Cancel();

  NS_IF_RELEASE(mSelectedItems);
  NS_IF_RELEASE(mCurrentItem);
  
  if (--gRefCnt == 0) {
    NS_IF_RELEASE(kSelectedAtom);
    NS_IF_RELEASE(kTreeItemAtom);
    NS_IF_RELEASE(kTreeRowAtom);
    NS_IF_RELEASE(kTreeChildrenAtom);
    NS_IF_RELEASE(kOpenAtom);
    NS_IF_RELEASE(kCurrentAtom);
  }
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedItems(nsIDOMNodeList** aSelectedItems)
{
  NS_IF_ADDREF(mSelectedItems);
  *aSelectedItems = mSelectedItems;
  return NS_OK;
}

nsresult
nsXULTreeElement::GetSuppressOnSelect(PRBool* aSuppressOnSelect)
{
    *aSuppressOnSelect = mSuppressOnSelect;
    return NS_OK;
}

nsresult
nsXULTreeElement::SetSuppressOnSelect(PRBool aSuppressOnSelect)
{
    if (!aSuppressOnSelect && mSuppressOnSelect)
        FireOnSelectHandler();
    mSuppressOnSelect = aSuppressOnSelect;
    return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectItem(nsIDOMXULElement* aTreeItem)
{
  NS_ASSERTION(aTreeItem, "trying to select a null tree item");
  if (!aTreeItem) return NS_OK;

  // Sanity check. If we're the only item, just bail.
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  if (length == 1) {
    // See if the single item already selected is us.
    nsCOMPtr<nsIDOMNode> domNode;
    mSelectedItems->Item(0, getter_AddRefs(domNode));
    nsCOMPtr<nsIDOMXULElement> treeItem = do_QueryInterface(domNode);
    if (treeItem.get() == aTreeItem)
      return NS_OK;
  }

  // First clear our selection.
  PRBool suppressSelect = mSuppressOnSelect;
  SetSuppressOnSelect(PR_TRUE);
  ClearItemSelection();

  // Now add ourselves to the selection by setting our selected attribute.
  AddItemToSelection(aTreeItem);

  SetCurrentItem(aTreeItem);
  mSelectionStart = nsnull;

  SetSuppressOnSelect(suppressSelect);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::TimedSelect(nsIDOMXULElement* aTreeItem, PRInt32 aMsec)
{
  PRBool suppressSelect = mSuppressOnSelect;

  if (aMsec != -1)
      SetSuppressOnSelect(PR_TRUE);

  nsresult rv = SelectItem(aTreeItem);

  if (aMsec != -1) {
    // Note, not using SetSuppressOnSelect here because that will
    // force FireOnSelect to fire immediately (if selection was not
    // initially suppressed).
    mSuppressOnSelect = suppressSelect;
    if (!mSuppressOnSelect) {
        if (mSelectTimer)
          mSelectTimer->Cancel();

        mSelectTimer = do_CreateInstance("@mozilla.org/timer;1");
        mSelectTimer->Init(SelectCallback, this, aMsec, NS_PRIORITY_HIGH);
    }
  }

  return rv;
}

NS_IMETHODIMP    
nsXULTreeElement::ClearItemSelection()
{
  // Enumerate the elements and remove them from the selection.
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  for (PRUint32 i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    mSelectedItems->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->UnsetAttr(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  }
  mSelectionStart = nsnull;

  if (!mSuppressOnSelect)
      FireOnSelectHandler();
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::AddItemToSelection(nsIDOMXULElement* aTreeItem)
{
  NS_ASSERTION(aTreeItem,"attepting to add a null tree item to the selection");
  if (!aTreeItem) return NS_ERROR_FAILURE;

  // Without clearing the selection, perform the add.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->SetAttr(kNameSpaceID_None, kSelectedAtom, NS_LITERAL_STRING("true"), PR_TRUE);

  if (!mSuppressOnSelect)
    FireOnSelectHandler();
  return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::RemoveItemFromSelection(nsIDOMXULElement* aTreeItem)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->UnsetAttr(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  if (!mSuppressOnSelect)
    FireOnSelectHandler();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::ToggleItemSelection(nsIDOMXULElement* aTreeItem)
{
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  
  nsAutoString multiple;
  mOuter->GetAttribute(NS_LITERAL_STRING("multiple"), multiple);

  nsAutoString isSelected;
  aTreeItem->GetAttribute(NS_LITERAL_STRING("selected"), isSelected);

  PRBool suppressSelect = mSuppressOnSelect;
  SetSuppressOnSelect(PR_TRUE);

  if (isSelected.EqualsWithConversion("true"))
    RemoveItemFromSelection(aTreeItem);
  else if (multiple.EqualsWithConversion("true") || length == 0)
    AddItemToSelection(aTreeItem);
  else 
    return NS_OK;

  SetCurrentItem(aTreeItem);
  SetSuppressOnSelect(suppressSelect);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)
{
  nsAutoString multiple;
  mOuter->GetAttribute(NS_LITERAL_STRING("multiple"), multiple);

  if (!multiple.EqualsWithConversion("true")) {
    // We're a single selection tree only. This
    // is not allowed.
    return NS_OK;
  }

  nsCOMPtr<nsIDOMXULElement> startItem;
  if (aStartItem == nsnull) {
    // Continue the ranged selection based off the first item selected
    if (mSelectionStart)
      startItem = mSelectionStart;
    else
      startItem = mCurrentItem;
  }
  else startItem = aStartItem;

  if (!startItem)
    startItem = aEndItem;

  // First clear our selection out completely.
  PRBool suppressSelect = mSuppressOnSelect;
  SetSuppressOnSelect(PR_TRUE);
  ClearItemSelection();

  mSelectionStart = startItem;

  PRInt32 startIndex = 0,
          endIndex = 0;

  // Get the indices of the starting and ending rows
  // so we can determine if this is a forward or backward
  // selection

  nsCOMPtr<nsIBoxObject> boxObject;
  mOuter->GetBoxObject(getter_AddRefs(boxObject));
  nsCOMPtr<nsITreeBoxObject> treebox = do_QueryInterface(boxObject);

  treebox->GetIndexOfItem(startItem, &startIndex);
  treebox->GetIndexOfItem(aEndItem, &endIndex);

  nsCOMPtr<nsIDOMElement> currentItem;
  // If it's a backward selection, swap the starting and
  // ending items so we always iterate forward
  if (endIndex < startIndex) {
      currentItem = do_QueryInterface(aEndItem);
      aEndItem = startItem;
      startItem = do_QueryInterface(currentItem);
  } else
      currentItem = do_QueryInterface(startItem);

  nsAutoString trueString; trueString.AssignWithConversion("true", 4);
  nsCOMPtr<nsIContent> content;
  nsCOMPtr<nsIAtom> tag;

  while (PR_TRUE) {
      content = do_QueryInterface(currentItem);
      content->GetTag(*getter_AddRefs(tag));
      if (tag && tag.get() == kTreeItemAtom)
          content->SetAttr(kNameSpaceID_None, kSelectedAtom, 
          trueString, /*aNotify*/ PR_TRUE);
      if (currentItem == aEndItem)
          break;
      nsCOMPtr<nsIDOMElement> nextItem;
      treebox->GetNextItem(currentItem, 1, getter_AddRefs(nextItem));
      currentItem = nextItem;
  }

  SetSuppressOnSelect(suppressSelect);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectAll()
{
  PRInt32 childCount;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  content->ChildCount(childCount);
  if (childCount == 0)
    return NS_OK;

  // Get the total row count.
  nsCOMPtr<nsIBoxObject> boxObject;
  mOuter->GetBoxObject(getter_AddRefs(boxObject));
  nsCOMPtr<nsITreeBoxObject> treebox = do_QueryInterface(boxObject);
  PRInt32 rowCount;
  treebox->GetRowCount(&rowCount);
  
  if (rowCount == 0)
    return NS_OK;

  // Get the item at index 0
  nsCOMPtr<nsIDOMXULElement> startContent;
  nsCOMPtr<nsIDOMElement> startElement;
  treebox->GetItemAtIndex(0, getter_AddRefs(startElement));
  startContent = do_QueryInterface(startElement);

  // Get the item at index rowCount-1
  nsCOMPtr<nsIDOMXULElement> endContent;
  nsCOMPtr<nsIDOMElement> endElement;
  treebox->GetItemAtIndex(rowCount-1, getter_AddRefs(endElement));
  endContent = do_QueryInterface(endElement);

  // Select the whole range.
  SelectItemRange(startContent, endContent);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::InvertSelection()
{
  // XXX Woo hoo. Write this later.
  // Yikes. Involves an enumeration of the whole tree.
  return NS_OK;
}

nsresult
nsXULTreeElement::FireOnSelectHandler()
{
#ifdef DEBUG_bryner
  printf("FireOnSelectHandler\n");
#endif
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

  // If there's no document (e.g., a selection is occuring in a
  // 'orphaned' node), then there ain't a whole lot to do here!
  if (! document) {
    NS_WARNING("FireOnSelectHandler occurred in orphaned node");
    return NS_OK;
  }

  // The frame code can suppress the firing of this handler by setting an attribute
  // for us.  Look for that and bail if it's present.
  nsCOMPtr<nsIAtom> kSuppressSelectChange = dont_AddRef(NS_NewAtom("suppressonselect"));
  nsAutoString value;
  content->GetAttr(kNameSpaceID_None, kSuppressSelectChange, value);
  if (value.EqualsWithConversion("true"))
    return NS_OK;

  PRInt32 count = document->GetNumberOfShells();
  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIPresShell> shell;
    document->GetShellAt(i, getter_AddRefs(shell));
    if (!shell)
      continue;

    // Retrieve the context in which our DOM event will fire.
    nsCOMPtr<nsIPresContext> aPresContext;
    shell->GetPresContext(getter_AddRefs(aPresContext));

    nsEventStatus status = nsEventStatus_eIgnore;
    nsEvent event;
    event.eventStructType = NS_EVENT;
    event.message = NS_FORM_SELECTED;

    content->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::GetCurrentItem(nsIDOMXULElement** aResult)
{
  *aResult = mCurrentItem;
  NS_IF_ADDREF(mCurrentItem);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SetCurrentItem(nsIDOMXULElement* aCurrentItem)
{
  nsCOMPtr<nsIContent> current;
  if (mCurrentItem) {
    current = do_QueryInterface(mCurrentItem);
    current->UnsetAttr(kNameSpaceID_None, kCurrentAtom, PR_TRUE);
    NS_RELEASE(mCurrentItem);
  }
  mCurrentItem = aCurrentItem;
  NS_IF_ADDREF(aCurrentItem);
  current = do_QueryInterface(mCurrentItem);
  if (current) 
    current->SetAttr(kNameSpaceID_None, kCurrentAtom, NS_LITERAL_STRING("true"), PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::CheckSelection(nsIDOMXULElement* aDeletedItem)
{
  if (aDeletedItem == mSelectionStart)
    mSelectionStart = nsnull;
  return NS_OK;
}

void
nsXULTreeElement::SelectCallback(nsITimer *aTimer, void *aClosure)
{
  nsXULTreeElement* self = NS_STATIC_CAST(nsXULTreeElement*, aClosure);
  if (self) {
    self->FireOnSelectHandler();
    aTimer->Cancel();
    self->mSelectTimer = nsnull;
  }
} // sTimerCallback


