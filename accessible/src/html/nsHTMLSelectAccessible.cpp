/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Contributor(s):
 *   Kyle Yuan (kyle.yuan@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h"
#include "nsHTMLSelectAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIListControlFrame.h"
#include "nsIServiceManager.h"
#include "nsITextContent.h"
#include "nsArray.h"

/**
  * Selects, Listboxes and Comboboxes, are made up of a number of different
  *  widgets, some of which are shared between the two. This file contains 
  *  all of the widgets for both of the Selects, for HTML only.
  *
  *  Listbox:
  *     - nsHTMLSelectListAccessible
  *       - nsHTMLSelectOptionAccessible
  *
  *  Comboboxes:
  *     - nsHTMLComboboxAccessible
  *        - nsHTMLComboboxTextFieldAccessible
  *        - nsHTMLComboboxButtonAccessible
  *        - nsHTMLComboboxListAccessible  [ inserted in accessible tree ]
  *           - nsHTMLSelectOptionAccessible(s)
  */

/** ------------------------------------------------------ */
/**  Impl. of nsHTMLSelectableAccessible                   */
/** ------------------------------------------------------ */

// Helper class
nsHTMLSelectableAccessible::iterator::iterator(nsHTMLSelectableAccessible *aParent, nsIWeakReference *aWeakShell): 
  mWeakShell(aWeakShell), mParentSelect(aParent)
{
  mLength = mIndex = 0;
  mSelCount = 0;

  nsCOMPtr<nsIDOMHTMLSelectElement> htmlSelect(do_QueryInterface(mParentSelect->mDOMNode));
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
                                                                   nsIMutableArray *aSelectedAccessibles, 
                                                                   nsPresContext *aContext)
{
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsIAccessible> tempAccess;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected) {
      nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(mOption));
      aAccService->GetAccessibleInWeakShell(optionNode, mWeakShell, getter_AddRefs(tempAccess));
    }
  }

  if (tempAccess)
    aSelectedAccessibles->AppendElement(NS_STATIC_CAST(nsISupports*, tempAccess), PR_FALSE);
}

PRBool nsHTMLSelectableAccessible::iterator::GetAccessibleIfSelected(PRInt32 aIndex, 
                                                                     nsIAccessibilityService *aAccService, 
                                                                     nsPresContext *aContext, 
                                                                     nsIAccessible **aAccessible)
{
  PRBool isSelected = PR_FALSE;

  *aAccessible = nsnull;

  if (mOption) {
    mOption->GetSelected(&isSelected);
    if (isSelected) {
      if (mSelCount == aIndex) {
        nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(mOption));
        aAccService->GetAccessibleInWeakShell(optionNode, mWeakShell, aAccessible);
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
nsAccessibleWrap(aDOMNode, aShell)
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

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
  htmlSelect->GetOptions(getter_AddRefs(options));
  if (!options)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMNode> tempNode;
  options->Item(aIndex, getter_AddRefs(tempNode));
  nsCOMPtr<nsIDOMHTMLOptionElement> tempOption(do_QueryInterface(tempNode));
  if (!tempOption)
    return NS_ERROR_FAILURE;

  tempOption->GetSelected(aSelState);
  nsresult rv = NS_OK;
  if (eSelection_Add == aMethod && !(*aSelState))
    rv = tempOption->SetSelected(PR_TRUE);
  else if (eSelection_Remove == aMethod && (*aSelState))
    rv = tempOption->SetSelected(PR_FALSE);
  return rv;
}

// Interface methods
NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectedChildren(nsIArray **_retval)
{
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!accService)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIMutableArray> selectedAccessibles;
  NS_NewArray(getter_AddRefs(selectedAccessibles));
  if (!selectedAccessibles)
    return NS_ERROR_OUT_OF_MEMORY;
  
  nsPresContext *context = GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    iter.AddAccessibleIfSelected(accService, selectedAccessibles, context);

  PRUint32 uLength = 0;
  selectedAccessibles->GetLength(&uLength); 
  if (uLength != 0) { // length of nsIArray containing selected options
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

  nsPresContext *context = GetPresContext();
  if (!context)
    return NS_ERROR_FAILURE;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    if (iter.GetAccessibleIfSelected(aIndex, accService, context, _retval))
      return NS_OK;
  
  // No matched item found
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::GetSelectionCount(PRInt32 *aSelectionCount)
{
  *aSelectionCount = 0;

  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
  while (iter.Advance())
    iter.CalcSelectionCount(aSelectionCount);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectableAccessible::AddChildToSelection(PRInt32 aIndex)
{
  PRBool isSelected;
  return ChangeSelection(aIndex, eSelection_Add, &isSelected);
}

NS_IMETHODIMP nsHTMLSelectableAccessible::RemoveChildFromSelection(PRInt32 aIndex)
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
  nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
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
    nsHTMLSelectableAccessible::iterator iter(this, mWeakShell);
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
nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell)
:nsHTMLSelectableAccessible(aDOMNode, aShell)
{
}

/**
  * As a nsHTMLSelectListAccessible we can have the following states:
  *     STATE_MULTISELECTABLE
  *     STATE_EXTSELECTABLE
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetState(PRUint32 *_retval)
{
  nsHTMLSelectableAccessible::GetState(_retval);
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *_retval |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}


/**
  * Gets the first child of the DOM node and creates and returns
  *  a nsHTMLSelectOptionAccessible.
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetFirstChild(nsIAccessible **aFirstChild)
{
  nsCOMPtr<nsIDOMNode> first;
  mDOMNode->GetFirstChild(getter_AddRefs(first));

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsresult rv = accService->GetAccessibleInWeakShell(first, mWeakShell, aFirstChild);
  mFirstChild = *aFirstChild;
  return rv;
}

/**
  * Gets the last child of the DOM node and creates and returns
  *  a nsHTMLSelectOptionAccessible.
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  nsCOMPtr<nsIDOMNode> last;
  mDOMNode->GetLastChild(getter_AddRefs(last));

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  return accService->GetAccessibleInWeakShell(last, mWeakShell, aLastChild);
}

/**
  * Gets the child count of a Select List Accessible. We want to count 
  *  all the <optgroup>s and <option>s as children because we want a 
  *  flat tree under the Select List.
  */

NS_IMETHODIMP nsHTMLSelectListAccessible::GetChildCount(PRInt32 *aAccChildCount) 
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
nsHTMLSelectOptionAccessible::nsHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsCOMPtr<nsIDOMNode> parentNode;
  aDOMNode->GetParentNode(getter_AddRefs(parentNode));
  nsCOMPtr<nsIAccessible> parentAccessible;
  if (parentNode) {
    // If the parent node is a Combobox, then the option's accessible parent
    // is nsHTMLComboboxListAccessible, not the nsHTMLComboboxAccessible that
    // GetParent would normally return. This is because the 
    // nsHTMLComboboxListAccessible is inserted into the accessible hierarchy
    // where there is no DOM node for it.
    accService->GetAccessibleInWeakShell(parentNode, mWeakShell, getter_AddRefs(parentAccessible));
    if (parentAccessible) {
      PRUint32 role;
      parentAccessible->GetRole(&role);
      if (role == ROLE_COMBOBOX) {
        nsCOMPtr<nsIAccessible> comboAccessible(parentAccessible);
        comboAccessible->GetLastChild(getter_AddRefs(parentAccessible));
      }
    }
  }
  SetParent(parentAccessible);
}

/** We are a ListItem */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_LISTITEM;
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetParent(nsIAccessible **aParent)
{   
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

/**
  * Gets the next accessible sibling of the mDOMNode and creates and returns
  *  a nsHTMLSelectOptionAccessible or nsHTMLSelectOptGroupAccessible.
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{ 
  // Get next sibling and if found create and return an accessible for it
  // When getting the next sibling of an SelectOption we could be working with
  // either an optgroup or an option. We process this tree as flat.

  *aNextSibling = nsnull;
  if (mNextSibling) {
    if (mNextSibling != DEAD_END_ACCESSIBLE) {
      NS_IF_ADDREF(*aNextSibling = mNextSibling);
    }
    return NS_OK;
  }
  if (!mParent) {
    // Don't try the following algorithm without a parent select accessible
    return NS_OK;
  }
  nsCOMPtr<nsIDOMNode> next = mDOMNode, currentNode;
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));

  while (!*aNextSibling && next) {
    currentNode = next;
    next = nsnull;
  
    nsCOMPtr<nsIDOMHTMLOptGroupElement> currOptGroupElement(do_QueryInterface(currentNode));
  
    if (currOptGroupElement) {
      currentNode->GetFirstChild(getter_AddRefs(next));
    }
    if (!next)  // no child under a <optgroup> or we started with a <option>
      currentNode->GetNextSibling(getter_AddRefs(next));  // See if there is another <optgroup>
  
    if (next) {
      accService->GetAccessibleInWeakShell(next, mWeakShell, aNextSibling);  
      continue;
    }
    // else No child then or child is not a <option> nor an <optgroup>
    // go back up to the parent and get next sibling from there,
    nsCOMPtr<nsIDOMNode> parent, parentNextSib;
    currentNode->GetParentNode(getter_AddRefs(parent));
 
    next = nsnull;
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(mParent));
    NS_ASSERTION(accessNode, "Unable to QI to nsIAccessNode");
    nsCOMPtr<nsIDOMNode> selectNode;
    accessNode->GetDOMNode(getter_AddRefs(selectNode));
    if (parent && parent != selectNode) { // End search for options at subtree's start
      parent->GetNextSibling(getter_AddRefs(next));
      if (next) {
        // We have a parent that is an option or option group
        // get accessible for either one and return it 
        accService->GetAccessibleInWeakShell(next, mWeakShell, aNextSibling);
      }
    }
  }
  SetNextSibling(*aNextSibling);
  return NS_OK;
} 

/**
  * Get our Name from our Content's subtree
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetName(nsAString& aName)
{
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
  nsresult rv = domElement->GetAttribute(NS_LITERAL_STRING("label"), aName) ;

  if (NS_SUCCEEDED(rv) && !aName.IsEmpty() ) {
    return NS_OK;
  }
  
  // CASE #2 -- no label parameter, get the first child, 
  // use it if it is a text node
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  if (child) {
    nsCOMPtr<nsITextContent> text(do_QueryInterface(child));
    if (text) {
      nsAutoString txtValue;
      rv = AppendFlatStringFromContentNode(text, &txtValue);
      if (NS_SUCCEEDED(rv)) {
        // Temp var (txtValue) needed until CompressWhitespace built for nsAString
        txtValue.CompressWhitespace();
        aName.Assign(txtValue);
        return NS_OK;
      }
    }
  }
  
  return NS_ERROR_FAILURE;
}

/**
  * Gets the previous accessible sibling of the mDOMNode and creates and returns
  *  a nsHTMLSelectOptionAccessible or nsHTMLSelectOptGroupAccessible.
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;

  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  nsCOMPtr<nsIAccessible> thisAcc, selectListAcc, nextSiblingAcc;  

  accService->GetAccessibleInWeakShell(mDOMNode, mWeakShell, getter_AddRefs(thisAcc));

  // The accessible parent of an <option> or <optgroup> 
  // is always the SelectListAcc - see GetAccessibleInShell()
  thisAcc->GetParent(getter_AddRefs(selectListAcc));

  if (!selectListAcc) {
    return NS_ERROR_FAILURE;  
  }
  nsCOMPtr<nsIDOMNode> siblingDOMNode;
  selectListAcc->GetFirstChild(_retval);

  // Go thru all the siblings until we find ourselves(mDOMNode) then use the 
  // sibling right before us.
  do {  
    (*_retval)->GetNextSibling(getter_AddRefs(nextSiblingAcc));
    if (!nextSiblingAcc) {
      *_retval = nsnull;
      return NS_ERROR_FAILURE;
    }
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(nextSiblingAcc));
    NS_ASSERTION(accessNode, "Unable to QI to nsIAccessNode");
    accessNode->GetDOMNode(getter_AddRefs(siblingDOMNode));
    if (siblingDOMNode == mDOMNode) {
      break;  // we found ourselves!
    }
    NS_RELEASE(*_retval);
    *_retval = nextSiblingAcc;
    NS_IF_ADDREF(*_retval);
  } while (nextSiblingAcc);
  
  return NS_OK;
} 

nsIFrame* nsHTMLSelectOptionAccessible::GetBoundsFrame()
{
  nsCOMPtr<nsIContent> selectContent(do_QueryInterface(mDOMNode));

  while (selectContent && selectContent->Tag() != nsAccessibilityAtoms::select) {
    selectContent = selectContent->GetParent();
  }

  nsCOMPtr<nsIDOMNode> selectNode(do_QueryInterface(selectContent));
  if (selectNode) {
    nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
    nsCOMPtr<nsIAccessible> selAcc;
    if (NS_SUCCEEDED(accService->GetAccessibleFor(selectNode, 
                                                  getter_AddRefs(selAcc)))) {
      PRUint32 state;
      selAcc->GetState(&state);
      if (state & STATE_COLLAPSED) {
        nsCOMPtr<nsIPresShell> presShell(GetPresShell());
        if (!presShell) {
          return nsnull;
        }
        nsIFrame *selectFrame = nsnull;
        presShell->GetPrimaryFrameFor(selectContent, &selectFrame);
        return selectFrame;
      }
    }
  }

  return nsAccessible::GetBoundsFrame();
}

/**
  * As a nsHTMLSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE -- not implemented yet
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetState(PRUint32 *_retval)
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
  GetFocusedOptionNode(parentNode, getter_AddRefs(focusedOptionNode));
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
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Select) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("select"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Select) {   // default action
    nsCOMPtr<nsIDOMHTMLOptionElement> newHTMLOption(do_QueryInterface(mDOMNode));
    if (!newHTMLOption) 
      return NS_ERROR_FAILURE;
    // Clear old selection
    nsCOMPtr<nsIDOMNode> oldHTMLOptionNode, selectNode;
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(mParent));
    NS_ASSERTION(accessNode, "Unable to QI to nsIAccessNode");
    accessNode->GetDOMNode(getter_AddRefs(selectNode));
    GetFocusedOptionNode(selectNode, getter_AddRefs(oldHTMLOptionNode));
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

    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
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
                                                            nsIDOMNode **aFocusedOptionNode)
{
  *aFocusedOptionNode = nsnull;
  NS_ASSERTION(aListNode, "Called GetFocusedOptionNode without a valid list node");

  nsCOMPtr<nsIContent> content(do_QueryInterface(aListNode));
  nsCOMPtr<nsIDocument> document = content->GetDocument();
  nsIPresShell *shell = nsnull;
  if (document)
    shell = document->GetShellAt(0);
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

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
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
    rv = options->Item(focusedOptionIndex, aFocusedOptionNode);
  else {  // If no options in list or focusedOptionIndex <0, then we are not focused on an item
    NS_ADDREF(*aFocusedOptionNode = aListNode);  // return normal target content
    rv = NS_OK;
  }

  return rv;
}

/** ----- nsHTMLSelectOptGroupAccessible ----- */

/** Default Constructor */
nsHTMLSelectOptGroupAccessible::nsHTMLSelectOptGroupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectOptionAccessible(aDOMNode, aShell)
{
}


/**
  * As a nsHTMLSelectOptGroupAccessible we can have the following states:
  *     STATE_SELECTABLE
  */
NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetState(PRUint32 *_retval)
{
  nsHTMLSelectOptionAccessible::GetState(_retval);
  *_retval &= ~(STATE_FOCUSABLE|STATE_SELECTABLE);
  
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetNumActions(PRUint8 *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
NS_IMETHODIMP nsHTMLComboboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_COMBOBOX;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxAccessible::Shutdown()
{
  nsHTMLSelectableAccessible::Shutdown();

  mComboboxTextFieldAccessible = nsnull;
  mComboboxButtonAccessible = nsnull;
  mComboboxListAccessible = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxAccessible::Init()
{
  // Hold references
  GetFirstChild(getter_AddRefs(mComboboxTextFieldAccessible));
  if (mComboboxTextFieldAccessible) {
    mComboboxTextFieldAccessible->GetNextSibling(getter_AddRefs(mComboboxButtonAccessible));
  }
  if (mComboboxButtonAccessible) {
    mComboboxButtonAccessible->GetNextSibling(getter_AddRefs(mComboboxListAccessible));
  }

  nsHTMLSelectableAccessible::Init();

  return NS_OK;
}

/**
  * We always have 3 children: TextField, Button, Window. In that order
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetChildCount(PRInt32 *_retval)
{
  *_retval = 3;
  return NS_OK;
}

/**
  * As a nsHTMLComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_READONLY
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetState(_retval);

  // we are open or closed
  PRBool isOpen = PR_FALSE;
  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (comboFrame)
    comboFrame->IsDroppedDown(&isOpen);

  if (isOpen)
    *_retval |= STATE_EXPANDED;
  else
    *_retval |= STATE_COLLAPSED;

  *_retval |= STATE_HASPOPUP | STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

/** 
  * Our last child is an nsHTMLComboboxListAccessible object, is also the third child
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetLastChild(nsIAccessible **aLastChild)
{
  // It goes: textfield, button, list. We're returning the list.

  return GetChildAt(2, aLastChild);
}

/** 
  * Our first child is an nsHTMLComboboxTextFieldAccessible object 
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetFirstChild(nsIAccessible **aFirstChild)
{
  if (mFirstChild) {
    *aFirstChild = mFirstChild;
  }
  else {
    nsHTMLComboboxTextFieldAccessible* accessible = 
      new nsHTMLComboboxTextFieldAccessible(this, mDOMNode, mWeakShell);
    *aFirstChild = accessible;
    if (! *aFirstChild)
      return NS_ERROR_FAILURE;
    accessible->Init();
    SetFirstChild(*aFirstChild);
  }
  NS_ADDREF(*aFirstChild);
  return NS_OK;
}

/**
  * MSAA/ATK accessible value != HTML value, especially not in combo boxes.
  * Our accessible value is the text label for of our ( first ) selected child.
  * The easiest way to get this is from the first child which is the readonly textfield.
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetValue(nsAString& aValue)
{
  nsCOMPtr<nsIAccessible> textFieldAccessible;
  nsresult rv = GetFirstChild(getter_AddRefs(textFieldAccessible));
  NS_ENSURE_SUCCESS(rv, rv);
  return textFieldAccessible->GetValue(aValue);
}

/** ----- nsHTMLComboboxTextFieldAccessible ----- */

/** Constructor */
nsHTMLComboboxTextFieldAccessible::nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, 
                                                                     nsIDOMNode* aDOMNode, 
                                                                     nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  // There is no cache entry for this item. 
  // It's generated and ref'd by  nsHTMLComboboxAccessible
  SetParent(aParent);
}

/**
  * Our next sibling is an nsHTMLComboboxButtonAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{ 
  if (mNextSibling) {
    *aNextSibling = mNextSibling;
  }
  else {
    nsHTMLComboboxButtonAccessible* accessible =
      new nsHTMLComboboxButtonAccessible(mParent, mDOMNode, mWeakShell);
    *aNextSibling = accessible;
    if (!*aNextSibling)
      return NS_ERROR_FAILURE;
    accessible->Init();
  }
  NS_ADDREF(*aNextSibling);
  return NS_OK;
} 

/**
  * Currently gets the text from the first option, needs to check for selection
  *     and then return that text.
  *     Walks the Frame tree and checks for proper frames.
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetValue(nsAString& _retval)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  if (!frame)
    return NS_ERROR_FAILURE;

  frame = frame->GetFirstChild(nsnull)->GetFirstChild(nsnull);
  nsIContent* content = frame->GetContent();

  if (!content) 
    return NS_ERROR_FAILURE;

  AppendFlatStringFromSubtree(content, &_retval);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = NS_STATIC_CAST(void*, this);
  return NS_OK;
}

/**
  * Gets the bounds for the BlockFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxTextFieldAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  // get our first child's frame
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  if (!frame)
    return;

  frame = frame->GetFirstChild(nsnull);
  *aBoundingFrame = frame;

  aBounds = frame->GetRect();
}

/** Return our cached parent */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/**
  * We are the first child of our parent, no previous sibling
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
} 

/**
  * Our role is currently only static text, but we should be able to have
  *     editable text here and we need to check that case.
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_STATICTEXT;
  return NS_OK;
}

/**
  * As a nsHTMLComboboxTextFieldAccessible we can have the following states:
  *     STATE_READONLY
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetState(_retval);

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}


/** -----ComboboxButtonAccessible ----- */

/** Constructor -- cache our parent */
nsHTMLComboboxButtonAccessible::nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  // There is no cache entry for this item. 
  // It's generated and ref'd by  nsHTMLComboboxAccessible
  SetParent(aParent);
}

/** Just one action ( click ). */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = eSingle_Action;
  return NS_OK;
}

/**
  * Programmaticaly click on the button, causing either the display or
  *     the hiding of the drop down box ( window ).
  *     Walks the Frame tree and checks for proper frames.
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::DoAction(PRUint8 aIndex)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsPresContext *context = GetPresContext();
  if (!frame || !context)
    return NS_ERROR_FAILURE;

  frame = frame->GetFirstChild(nsnull)->GetNextSibling();

  // We only have one action, click. Any other index is meaningless(wrong)
  if (aIndex == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement>
      element(do_QueryInterface(frame->GetContent()));
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
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& _retval)
{
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (isOpen)
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("close"), _retval); 
  else
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("open"), _retval); 

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = NS_STATIC_CAST(void*, this);
  return NS_OK;
}

/**
  * Gets the bounds for the gfxButtonControlFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxButtonAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  // get our second child's frame
  // bounding frame is the ComboboxControlFrame
  nsIFrame *frame = nsAccessible::GetBoundsFrame();
  *aBoundingFrame = frame;
  nsPresContext *context = GetPresContext();
  if (!frame || !context)
    return;

  aBounds = frame->GetFirstChild(nsnull)->GetNextSibling()->GetRect();
    // sibling frame is for the button
}

/** We are a button. */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetParent(nsIAccessible **aParent)
{   
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

/** 
  * Gets the name from GetActionName()
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetName(nsAString& _retval)
{
  return GetActionName(eAction_Click, _retval);
}

/**
  * As a nsHTMLComboboxButtonAccessible we can have the following states:
  *     STATE_PRESSED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetState(_retval);

  // we are open or closed --> pressed or not
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (isOpen)
  *_retval |= STATE_PRESSED;

  *_retval |= STATE_FOCUSABLE;

  return NS_OK;
}

/**
  * Our next sibling is an nsHTMLComboboxListAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetNextSibling(nsIAccessible **aNextSibling)
{ 
  if (mNextSibling) {
    *aNextSibling = mNextSibling;
  }
  else {
    nsHTMLComboboxListAccessible* accessible = 
      new nsHTMLComboboxListAccessible(mParent, mDOMNode, mWeakShell);
    *aNextSibling = accessible;
    if (!*aNextSibling)
      return NS_ERROR_OUT_OF_MEMORY;
    accessible->Init();
  }
  NS_ADDREF(*aNextSibling);
  return NS_OK;
} 

/**
  * Our prev sibling is an nsHTMLComboboxTextFieldAccessible object
  */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetPreviousSibling(nsIAccessible **aAccPrevSibling)
{ 
  return mParent->GetFirstChild(aAccPrevSibling);
} 


/** ----- nsHTMLComboboxListAccessible ----- */

nsHTMLComboboxListAccessible::nsHTMLComboboxListAccessible(nsIAccessible *aParent,
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsHTMLSelectListAccessible(aDOMNode, aShell)
{
  // There is no cache entry for this item. 
  // It's generated and ref'd by  nsHTMLComboboxAccessible
  SetParent(aParent);
}

/**
  * As a nsHTMLComboboxListAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE
  *     STATE_FLOATING
  */
NS_IMETHODIMP nsHTMLComboboxListAccessible::GetState(PRUint32 *aState)
{
  // Get focus status from base class
  nsAccessible::GetState(aState);

  // we are open or closed
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = nsnull;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (isOpen)
    *aState |= STATE_FLOATING | STATE_FOCUSABLE;
  else
    *aState |= STATE_INVISIBLE | STATE_FOCUSABLE;
  
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsHTMLComboboxListAccessible::GetParent(nsIAccessible **aParent)
{
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxListAccessible::GetPreviousSibling(nsIAccessible **aAccPrevSibling)
{ 
  return mParent->GetChildAt(1, aAccPrevSibling);
} 

NS_IMETHODIMP nsHTMLComboboxListAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same for all tree item, use |this| pointer as the unique Id
  *aUniqueID = NS_STATIC_CAST(void*, this);
  return NS_OK;
}

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxListAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
   // get our first option
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  // now get its frame
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mWeakShell));
  if (!shell) {
    *aBoundingFrame = nsnull;
    return;
  }

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(child));
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame) {
    *aBoundingFrame = nsnull;
    return;
  }

  *aBoundingFrame = frame->GetParent();
  aBounds = (*aBoundingFrame)->GetRect();
}
