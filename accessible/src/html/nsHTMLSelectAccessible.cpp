/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 * 
 * Contributor(s):
 *           Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIListControlFrame.h"
#include "nsIOptionElement.h"
#include "nsISelectControlFrame.h"
#include "nsIServiceManager.h"
#include "nsIWidget.h"
#include "nsLayoutAtoms.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains 
  *  all of the widgets for both of the Selects, for HTML only. Some of them
  *  extend classes from nsSelectAccessible.cpp, which contains base classes 
  *  that are also extended by the XUL Select Accessibility support.
  *
  *  Listbox:
  *     - nsHTMLListboxAccessible
  *        - nsHTMLSelectListAccessible
  *           - nsHTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsHTMLComboboxAccessilbe
  *        - nsHTMLComboboxTextFieldAccessible
  *        - nsHTMLComboboxButtonAccessible
  *        - nsHTMLComboboxWindowAccessilbe
  *           - nsHTMLSelectListAccessible
  *              - nsHTMLSelectOptionAccessible
  */

/** ------------------------------------------------------ */
/**  Impl. of nsHTMLSelectableAccessible                   */
/** ------------------------------------------------------ */

// Helper class
nsHTMLSelectableAccessible::iterator::iterator(nsHTMLSelectableAccessible *aParent) : mParent(aParent)
{
  mLength = mIndex = 0;
  mSelCount = 0;

  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mParent->mDOMNode));
  if (htmlSelect) {
    htmlSelect->GetOptions(getter_AddRefs(mOptions));
    if (mOptions)
      mOptions->GetLength(&mLength);
  }
}

PRBool nsHTMLSelectableAccessible::iterator::Advance() 
{
  if (mIndex < mLength) {
    nsCOMPtr<nsIDOMNode> tempNode;
    if (mOptions) {
      mOptions->Item(mIndex, getter_AddRefs(tempNode));
      mOption = do_QueryInterface(tempNode);
    }
    mIndex++;
    return PR_TRUE;
  }
  return PR_FALSE;
}

void nsHTMLSelectableAccessible::iterator::CalcSelectionCount(PRInt32 *aSelectionCount)
{
  PRBool isSelected = PR_FALSE;

  if (mOption)
    mOption->GetSelected(&isSelected);

  if (isSelected)
    (*aSelectionCount)++;
}

void nsHTMLSelectableAccessible::iterator::AddAccessibleIfSelected(nsIAccessibilityService *aAccService, 
                                                                   nsISupportsArray *aSelectedAccessibles, 
                                                                   nsIPresContext *aContext)
{
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsIAccessible> tempAccess;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected)
      aAccService->CreateHTMLSelectOptionAccessible(mOption, mParent, aContext, getter_AddRefs(tempAccess));
  }

  if (tempAccess)
    aSelectedAccessibles->AppendElement(tempAccess);
}

PRBool nsHTMLSelectableAccessible::iterator::GetAccessibleIfSelected(PRInt32 aIndex, 
                                                                     nsIAccessibilityService *aAccService, 
                                                                     nsIPresContext *aContext, 
                                                                     nsIAccessible **_retval)
{
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsIAccessible> tempAccess;

  *_retval = nsnull;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected) {
      if (mSelCount == aIndex) {
        aAccService->CreateHTMLSelectOptionAccessible(mOption, mParent, aContext, getter_AddRefs(tempAccess));
        *_retval = tempAccess;
        NS_IF_ADDREF(*_retval);
        return PR_TRUE;
      }
      mSelCount++;
    }
  }

  return PR_FALSE;
}

void nsHTMLSelectableAccessible::iterator::Select(PRBool aSelect)
{
  if (mOption)
    mOption->SetSelected(aSelect);
}

nsHTMLSelectableAccessible::nsHTMLSelectableAccessible(nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell):
nsAccessible(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLSelectableAccessible, nsAccessible, nsIAccessibleSelectable)

// Helper methods
NS_IMETHODIMP nsHTMLSelectableAccessible::ChangeSelection(PRInt32 aIndex, PRUint8 aMethod, PRBool *aSelState)
{
  *aSelState = PR_FALSE;

  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mDOMNode));
  if (!htmlSelect)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLCollection> options;
  htmlSelect->GetOptions(getter_AddRefs(options));
  if (!options)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> tempNode;
  options->Item(aIndex, getter_AddRefs(tempNode));
  nsCOMPtr<nsIDOMHTMLOptionElement> tempOption(do_QueryInterface(tempNode));
  if (!tempOption)
    return NS_ERROR_FAILURE;

  tempOption->GetSelected(aSelState);
  if (eSelection_Add == aMethod && !(*aSelState))
    tempOption->SetSelected(PR_TRUE);
  else if (eSelection_Remove == aMethod && (*aSelState))
    tempOption->SetSelected(PR_FALSE);
  return NS_OK;
}

// Interface methods
NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsArray> selectedAccessibles;
  NS_NewISupportsArray(getter_AddRefs(selectedAccessibles));
  if (!selectedAccessibles)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this);
  while (iter.Advance())
    iter.AddAccessibleIfSelected(accService, selectedAccessibles, context);

  PRUint32 uLength = 0;
  selectedAccessibles->Count(&uLength); 
  if (uLength != 0) { // length of nsISupportsArray containing selected options
    *_retval = selectedAccessibles;
    NS_ADDREF(*_retval);
  }
  return NS_OK;
}

// return the nth selected child's nsIAccessible object
NS_IMETHODIMP nsHTMLSelectableAccessible::RefSelection(PRInt32 aIndex, nsIAccessible **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this);
  while (iter.Advance())
    if (iter.GetAccessibleIfSelected(aIndex, accService, context, _retval))
      return NS_OK;
  
  // No matched item found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  nsHTMLSelectableAccessible::iterator iter(this);
  while (iter.Advance())
    iter.CalcSelectionCount(aSelectionCount);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::AddSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::RemoveSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Remove, &isSelected);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::IsChildSelected(PRInt32 aIndex, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return ChangeSelection(aIndex, eSelection_GetState, _retval);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::ClearSelection()
{
  nsHTMLSelectableAccessible::iterator iter(this);
  while (iter.Advance())
    iter.Select(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::SelectAllSelection(PRBool *_retval)
{
  *_retval = PR_FALSE;
  
  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mDOMNode));
  if (!htmlSelect)
    return NS_ERROR_FAILURE;

  htmlSelect->GetMultiple(_retval);
  if (*_retval) {
    nsHTMLSelectableAccessible::iterator iter(this);
    while (iter.Advance())
      iter.Select(PR_TRUE);
  }
  return NS_OK;
}

/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/** ----- nsHTMLSelectListAccessible ----- */

/** Default Constructor */
nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIAccessible* aParent, 
                                                       nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell)
:nsSelectListAccessible(aParent, aDOMNode, aShell)
{
}

/**
  * As a nsHTMLSelectListAccessible we can have the following states:
  *     STATE_MULTISELECTABLE
  *     STATE_EXTSELECTABLE
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccState(PRUint32 *_retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *_retval |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

/**
  * Gets the last child of the DOM node and creates and returns
  *  a nsHTMLSelectOptionAccessible.
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> last;
  mDOMNode->GetLastChild(getter_AddRefs(last));

  *_retval = new nsHTMLSelectOptionAccessible(this, last, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * Gets the child count of a Select List Accessible. We want to count 
  *  all the <optgroup>s and <option>s as children because we want a 
  *  flat tree under the Select List.
  */

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
    // Count the number of <Option Group> and <option> elements and return 
    // this number. This is so the tree can be flattened
  nsCOMPtr<nsIDOMNode> next, nextInner, nextChild;
  nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(mDOMNode));

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  PRInt32 countChild;

  countChild = 0;
  mDOMNode->GetFirstChild(getter_AddRefs(next));

  while (next) {
    nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroupElement(do_QueryInterface(next));
    countChild++;
    if (optGroupElement) {
      next->GetFirstChild(getter_AddRefs(nextInner));
      while (nextInner ) {
        nsCOMPtr<nsIDOMHTMLOptionElement> optionElement(do_QueryInterface(nextInner));
        if (optionElement) {
          countChild++;
        }
        nextInner->GetNextSibling(getter_AddRefs(nextChild));
        nextInner = nextChild;
      } // endWhile nextInner
    }  // endif optGroupElement
    next->GetNextSibling(getter_AddRefs(nextInner));
    next = nextInner;
  }  // endWhile next
  *aAccChildCount = countChild;
  return NS_OK;

}

/** ----- nsHTMLSelectOptionAccessible ----- */

/** Default Constructor */
nsHTMLSelectOptionAccessible::nsHTMLSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsSelectOptionAccessible(aParent, aDOMNode, aShell)
{
}

/**
  * Gets the next accessible sibling of the mDOMNode and creates and returns
  *  a nsHTMLSelectOptionAccessible or nsHTMLSelectOptGroupAccessible.
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  // Get next sibling and if found create and return an accessible for it
  // When getting the next sibling of an SelectOption we could be working with
  // either an optgroup or an option. We process this tree as flat.
  *_retval = nsnull;
  nsCOMPtr<nsIDOMNode> next = mDOMNode, currentNode;
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));

  while (!*_retval && next) {
    currentNode = next;
    next = nsnull;
  
    nsCOMPtr<nsIDOMHTMLOptGroupElement> currOptGroupElement(do_QueryInterface(currentNode));
  
    if (currOptGroupElement) {
      currentNode->GetFirstChild(getter_AddRefs(next));
    }
    if (!next)  // no child under a <optgroup> or we started with a <option>
      currentNode->GetNextSibling(getter_AddRefs(next));  // See if there is another <optgroup>
  
    if (next) {
      accService->GetAccessibleFor(next, _retval);  
      continue;
    }
    // else No child then or child is not a <option> nor an <optgroup>
    // go back up to the parent and get next sibling from there,
    nsCOMPtr<nsIDOMNode> parent, parentNextSib;
    currentNode->GetParentNode(getter_AddRefs(parent));
 
    if (!parent)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> selectNode;
    mParent->AccGetDOMNode(getter_AddRefs(selectNode));
    if (parent == selectNode)
      return NS_OK;  // End search for options at subtree's start

    parent->GetNextSibling(getter_AddRefs(next));
    if (!next) 
      return NS_OK; // done
  
    // We have a parent that is an option or option group
    // get accessible for either one and return it 
    accService->GetAccessibleFor(next, _retval);
  }
  return NS_OK;
} 

/**
  * Gets the previous accessible sibling of the mDOMNode and creates and returns
  *  a nsHTMLSelectOptionAccessible or nsHTMLSelectOptGroupAccessible.
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsCOMPtr<nsIAccessible> thisAcc, selectListAcc, nextSiblingAcc;  

  accService->GetAccessibleFor(mDOMNode, getter_AddRefs(thisAcc));

  // The accessible parent of an <option> or <optgroup> is always the SelectListAcc - see GetAccessibleFor()
  thisAcc->GetAccParent(getter_AddRefs(selectListAcc));

  if (!selectListAcc) {
    return NS_ERROR_FAILURE;  
  }
  nsCOMPtr<nsIDOMNode> siblingDOMNode;
  selectListAcc->GetAccFirstChild(_retval);

  // Go thru all the siblings until we find ourselves(mDOMNode) then use the 
  // sibling right before us.
  do {  
    (*_retval)->GetAccNextSibling(getter_AddRefs(nextSiblingAcc));
    if (!nextSiblingAcc) {
      *_retval = nsnull;
      return NS_ERROR_FAILURE;
    }
    nextSiblingAcc->AccGetDOMNode(getter_AddRefs(siblingDOMNode));
    if (siblingDOMNode == mDOMNode) {
      break;  // we found ourselves!
    }
    NS_RELEASE(*_retval);
    *_retval = nextSiblingAcc;
    NS_IF_ADDREF(*_retval);
  } while (nextSiblingAcc);
  
  return NS_OK;
} 

/**
  * As a nsHTMLSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE -- not implemented yet
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = 0;
  nsCOMPtr<nsIDOMNode> focusedOptionNode, parentNode;
  // Go up to parent <select> element
  nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(mDOMNode));
  do {
    thisNode->GetParentNode(getter_AddRefs(parentNode));
    nsCOMPtr<nsIDOMHTMLSelectElement> selectControl(do_QueryInterface(parentNode));
    if (selectControl) {
      break;
    }
    thisNode = parentNode;
  } while (parentNode);
  if (!parentNode) {
    return NS_ERROR_FAILURE;
  }
  
  // find out if we are the focused node
  GetFocusedOptionNode(parentNode, focusedOptionNode);
  if (focusedOptionNode == mDOMNode)
    *_retval |= STATE_FOCUSED;

  // Are we selected?
  nsCOMPtr<nsIDOMHTMLOptionElement> option (do_QueryInterface(mDOMNode));
  if ( option ) {
    PRBool isSelected = PR_FALSE;
    option->GetSelected(&isSelected);
    if ( isSelected ) 
      *_retval |= STATE_SELECTED;
  }

  *_retval |= STATE_SELECTABLE | STATE_FOCUSABLE;
  
  return NS_OK;
}

/** select us! close combo box if necessary*/
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Select) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("select"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Select) {   // default action
    nsCOMPtr<nsIDOMHTMLOptionElement> newHTMLOption(do_QueryInterface(mDOMNode));
    if (!newHTMLOption) 
      return NS_ERROR_FAILURE;
    // Clear old selection
    nsCOMPtr<nsIDOMNode> oldHTMLOptionNode, selectNode;
    mParent->AccGetDOMNode(getter_AddRefs(selectNode));
    nsHTMLSelectOptionAccessible::GetFocusedOptionNode(selectNode, oldHTMLOptionNode);
    nsCOMPtr<nsIDOMHTMLOptionElement> oldHTMLOption(do_QueryInterface(oldHTMLOptionNode));
    if (oldHTMLOption)
      oldHTMLOption->SetSelected(PR_FALSE);
    // Set new selection
    newHTMLOption->SetSelected(PR_TRUE);

    // If combo box, and open, close it
    // First, get the <select> widgets list control frame
    nsCOMPtr<nsIDOMNode> testSelectNode;
    nsCOMPtr<nsIDOMNode> thisNode(do_QueryInterface(mDOMNode));
    do {
      thisNode->GetParentNode(getter_AddRefs(testSelectNode));
      nsCOMPtr<nsIDOMHTMLSelectElement> selectControl(do_QueryInterface(testSelectNode));
      if (selectControl)
        break;
      thisNode = testSelectNode;
    } while (testSelectNode);

    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mPresShell));
    nsCOMPtr<nsIContent> selectContent(do_QueryInterface(testSelectNode));
    nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(mDOMNode));

    if (!testSelectNode || !selectContent || !presShell || !option) 
      return NS_ERROR_FAILURE;

    nsIFrame *selectFrame = nsnull;
    presShell->GetPrimaryFrameFor(selectContent, &selectFrame);
    nsIComboboxControlFrame *comboBoxFrame = nsnull;
    selectFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboBoxFrame);
    if (comboBoxFrame) {
      nsIFrame *listFrame = nsnull;
      comboBoxFrame->GetDropDown(&listFrame);
      PRBool isDroppedDown;
      comboBoxFrame->IsDroppedDown(&isDroppedDown);
      if (isDroppedDown && listFrame) {
        // use this list control frame to roll up the list
        nsIListControlFrame *listControlFrame = nsnull;
        listFrame->QueryInterface(NS_GET_IID(nsIListControlFrame), (void**)&listControlFrame);
        if (listControlFrame) {
          PRInt32 newIndex = 0;
          option->GetIndex(&newIndex);
          listControlFrame->ComboboxFinish(newIndex);
        }
      }
    }
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/**
  * Helper method for getting the focused DOM Node from our parent(list) node. We
  *  need to use the frame to get the focused option because for some reason we
  *  weren't getting the proper notification when the focus changed using the DOM
  */
nsresult nsHTMLSelectOptionAccessible::GetFocusedOptionNode(nsIDOMNode *aListNode, 
                                                            nsCOMPtr<nsIDOMNode>& aFocusedOptionNode)
{
  NS_ASSERTION(aListNode, "Called GetFocusedOptionNode without a valid list node");

  nsCOMPtr<nsIContent> content(do_QueryInterface(aListNode));
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));
  nsCOMPtr<nsIPresShell> shell;
  if (document) 
    document->GetShellAt(0,getter_AddRefs(shell));
  if (!shell)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame)
    return NS_ERROR_FAILURE;

  PRInt32 focusedOptionIndex = 0;

  // Get options
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(aListNode));
  NS_ASSERTION(selectElement, "No select element where it should be");

  nsCOMPtr<nsIDOMHTMLCollection> options;
  nsresult rv = selectElement->GetOptions(getter_AddRefs(options));
  
  if (NS_SUCCEEDED(rv)) {
    nsIListControlFrame *listFrame = nsnull;
    frame->QueryInterface(NS_GET_IID(nsIListControlFrame), (void**)&listFrame);
    if (listFrame) {
      // Get what's focused in listbox by asking frame for "selected item". 
      // Can't use dom interface for this, because it will always return the first selected item
      // when there is more than 1 item selected. We need the focused item, not
      // the first selected item.
      rv = listFrame->GetSelectedIndex(&focusedOptionIndex);
    }
    else  // Combo boxes can only have 1 selected option, so they can use the dom interface for this
      rv = selectElement->GetSelectedIndex(&focusedOptionIndex);
  }

  // Either use options and focused index, or default to list node itself
  if (NS_SUCCEEDED(rv) && options && focusedOptionIndex >= 0)   // Something is focused
    rv = options->Item(focusedOptionIndex, getter_AddRefs(aFocusedOptionNode));
  else {  // If no options in list or focusedOptionIndex <0, then we are not focused on an item
    aFocusedOptionNode = aListNode;  // return normal target content
    rv = NS_OK;
  }

  return rv;
}

/** ----- nsHTMLSelectOptGroupAccessible ----- */

/** Default Constructor */
nsHTMLSelectOptGroupAccessible::nsHTMLSelectOptGroupAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectOptionAccessible(aParent, aDOMNode, aShell)
{
}


/**
  * As a nsHTMLSelectOptGroupAccessible we can have the following states:
  *     STATE_SELECTABLE
  */
NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetAccState(PRUint32 *_retval)
{
  nsHTMLSelectOptionAccessible::GetAccState(_retval);
  *_retval &= ~(STATE_FOCUSABLE|STATE_SELECTABLE);
  
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::AccDoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetAccNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/** ------------------------------------------------------ */
/**  Secondly, the Listbox widget                          */
/** ------------------------------------------------------ */

/** ----- nsHTMLListboxAccessible ----- */

/** Constructor */
nsHTMLListboxAccessible::nsHTMLListboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectableAccessible(aDOMNode, aShell)
{
}

/** We are a window, as far as MSAA is concerned */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_WINDOW;
  return NS_OK;
}

/**
  * We always have 1 child: a subclass of nsSelectListAccessible.
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/**
  * As a nsHTMLListboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetAccState(_retval);

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

/** 
  * Our last (and only) child is an nsHTMLSelectListAccessible object 
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** 
  * Our first (and only) child is an nsHTMLSelectListAccessible object
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * Our value is the value of our ( first ) selected child. nsIDOMHTMLSelectElement
  *     returns this by default with GetValue().
  */
NS_IMETHODIMP nsHTMLListboxAccessible::GetAccValue(nsAString& _retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    select->GetValue(_retval);  
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/** ------------------------------------------------------ */
/**  Finally, the Combobox widgets                         */
/** ------------------------------------------------------ */

/** ----- nsHTMLComboboxAccessible ----- */

nsHTMLComboboxAccessible::nsHTMLComboboxAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectableAccessible(aDOMNode, aShell)
{
}

/** We are a combobox */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_COMBOBOX;
  return NS_OK;
}

/**
  * We always have 3 children: TextField, Button, Window. In that order
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 3;
  return NS_OK;
}

/**
  * As a nsComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetAccState(_retval);

  // we are open or closed
  PRBool isOpen = PR_FALSE;
  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (comboFrame)
    comboFrame->IsDroppedDown(&isOpen);

  *_retval |= isOpen ? STATE_EXPANDED : STATE_COLLAPSED;
  *_retval |= STATE_HASPOPUP | STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

/** 
  * Our last child is an nsHTMLComboboxWindowAccessible object 
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLComboboxWindowAccessible(this, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** 
  * Our last child is an nsHTMLComboboxTextFieldAccessible object 
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLComboboxTextFieldAccessible(this, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * Our value is the value of our ( first ) selected child. nsIDOMHTMLSelectElement
  *     returns this by default with GetValue().
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetAccValue(nsAString& _retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if (select) {
    select->GetValue(_retval);  
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

/** ----- nsHTMLComboboxTextFieldAccessible ----- */

/** Constructor */
nsHTMLComboboxTextFieldAccessible::nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, 
                                                                 nsIDOMNode* aDOMNode, 
                                                                 nsIWeakReference* aShell):
nsComboboxTextFieldAccessible(aParent, aDOMNode, aShell)
{
}

/**
  * Our next sibling is an nsHTMLComboboxButtonAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLComboboxButtonAccessible(parent, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

/** -----ComboboxButtonAccessible ----- */

/** Constructor -- cache our parent */
nsHTMLComboboxButtonAccessible::nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsComboboxButtonAccessible(aParent, aDOMNode, aShell)
{
}

/**
  * Programmaticaly click on the button, causing either the display or
  *     the hiding of the drop down box ( window ).
  *     Walks the Frame tree and checks for proper frames.
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::AccDoAction(PRUint8 index)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!context)
    return NS_ERROR_FAILURE;

  frame->FirstChild(context, nsnull, &frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::blockFrame))
    return NS_ERROR_FAILURE;
#endif

  frame->GetNextSibling(&frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::gfxButtonControlFrame))
    return NS_ERROR_FAILURE;
#endif

  nsCOMPtr<nsIContent> content;
  frame->GetContent(getter_AddRefs(content));

  // We only have one action, click. Any other index is meaningless(wrong)
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(content));
    if (element)
    {
       element->Click();
       return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Our next sibling is an nsHTMLComboboxWindowAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLComboboxWindowAccessible(parent, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

/**
  * Our next sibling is an nsHTMLComboboxTextFieldAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLComboboxTextFieldAccessible(parent, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

/** ----- nsHTMLComboboxWindowAccessible ----- */

/**
  * Constructor -- cache our parent
  */
nsHTMLComboboxWindowAccessible::nsHTMLComboboxWindowAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsComboboxWindowAccessible(aParent, aDOMNode, aShell)
{
}
 
/**
  * Our previous sibling is a nsHTMLComboboxButtonAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxWindowAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLComboboxButtonAccessible(parent, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

/**
  * We only have one child, a list
  */
NS_IMETHODIMP nsHTMLComboboxWindowAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * We only have one child, a list
  */
NS_IMETHODIMP nsHTMLComboboxWindowAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if (! *_retval)
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

