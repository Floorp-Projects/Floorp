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
#include "nsIAccessibleEvent.h"
#include "nsIFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIListControlFrame.h"
#include "nsIServiceManager.h"
#include "nsIMutableArray.h"

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
  *        - nsHTMLComboboxTextFieldAccessible  (#ifdef COMBO_BOX_WITH_THREE_CHILDREN)
  *        - nsHTMLComboboxButtonAccessible     (#ifdef COMBO_BOX_WITH_THREE_CHILDREN)
  *        - nsHTMLComboboxListAccessible        [ inserted in accessible tree ]
  *           - nsHTMLSelectOptionAccessible(s)
  *
  * XXX COMBO_BOX_WITH_THREE_CHILDREN is not currently defined.
  *     If we start using it again, we should pass the correct frame into those accessibles.
  *     They share a DOM node with the parent combobox.
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

  nsCOMPtr<nsIMutableArray> selectedAccessibles =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_STATE(selectedAccessibles);
  
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
  *     nsIAccessibleStates::STATE_MULTISELECTABLE
  *     nsIAccessibleStates::STATE_EXTSELECTABLE
  */
NS_IMETHODIMP
nsHTMLSelectListAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHTMLSelectableAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if (select) {
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *aState |= nsIAccessibleStates::STATE_MULTISELECTABLE |
                 nsIAccessibleStates::STATE_EXTSELECTABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetRole(PRUint32 *aRole)
{
  if (mParent && Role(mParent) == nsIAccessibleRole::ROLE_COMBOBOX) {
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_LIST;
  }
  else {
    *aRole = nsIAccessibleRole::ROLE_LIST;
  }
  return NS_OK;
}

already_AddRefed<nsIAccessible>
nsHTMLSelectListAccessible::AccessibleForOption(nsIAccessibilityService *aAccService,
                                                nsIContent *aContent,
                                                nsIAccessible *aLastGoodAccessible,
                                                PRInt32 *aChildCount)
{
  nsCOMPtr<nsIDOMNode> domNode(do_QueryInterface(aContent));
  NS_ASSERTION(domNode, "DOM node is null");
  // Accessibility service will initialize & cache any accessibles created
  nsCOMPtr<nsIAccessible> accessible;
  aAccService->GetAccessibleInWeakShell(domNode, mWeakShell, getter_AddRefs(accessible));
  nsCOMPtr<nsPIAccessible> privateAccessible(do_QueryInterface(accessible));
  if (!privateAccessible) {
    return nsnull;
  }

  ++ *aChildCount;
  privateAccessible->SetParent(this);
  nsCOMPtr<nsPIAccessible> privatePrevAccessible(do_QueryInterface(aLastGoodAccessible));
  if (privatePrevAccessible) {
    privatePrevAccessible->SetNextSibling(accessible);
  }
  if (!mFirstChild) {
    mFirstChild = accessible;
  }
  nsIAccessible *returnAccessible = accessible;
  NS_ADDREF(returnAccessible);
  return returnAccessible;
}

already_AddRefed<nsIAccessible>
nsHTMLSelectListAccessible::CacheOptSiblings(nsIAccessibilityService *aAccService,
                                             nsIContent *aParentContent,
                                             nsIAccessible *aLastGoodAccessible,
                                             PRInt32 *aChildCount)
{
  // Recursive helper for CacheChildren()

  PRUint32 numChildren = aParentContent->GetChildCount();
  nsCOMPtr<nsIAccessible> lastGoodAccessible(aLastGoodAccessible);

  for (PRUint32 count = 0; count < numChildren; count ++) {
    nsIContent *childContent = aParentContent->GetChildAt(count);
    if (!childContent->IsNodeOfType(nsINode::eHTML)) {
      continue;
    }
    nsCOMPtr<nsIAtom> tag = childContent->Tag();
    if (tag == nsAccessibilityAtoms::option || tag == nsAccessibilityAtoms::optgroup) {
      lastGoodAccessible = AccessibleForOption(aAccService,
                                               childContent,
                                               lastGoodAccessible,
                                               aChildCount);
      if (tag == nsAccessibilityAtoms::optgroup) {
        lastGoodAccessible = CacheOptSiblings(aAccService, childContent,
                                              lastGoodAccessible,
                                              aChildCount);
      }
    }
  }
  if (lastGoodAccessible) {
    nsCOMPtr<nsPIAccessible> privateLastAcc =
      do_QueryInterface(lastGoodAccessible);
    privateLastAcc->SetNextSibling(nsnull);
    NS_ADDREF(aLastGoodAccessible = lastGoodAccessible);
  }
  return aLastGoodAccessible;
}

/**
  * Cache the children and child count of a Select List Accessible. We want to count 
  *  all the <optgroup>s and <option>s as children because we want a 
  *  flat tree under the Select List.
  */

void nsHTMLSelectListAccessible::CacheChildren()
{
  // Cache the number of <optgroup> and <option> DOM decendents,
  // as well as the accessibles for them. Avoid whitespace text nodes.

  nsCOMPtr<nsIContent> selectContent(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (!selectContent || !accService) {
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  PRInt32 childCount = 0;
  nsCOMPtr<nsIAccessible> lastGoodAccessible =
    CacheOptSiblings(accService, selectContent, nsnull, &childCount);
  mAccChildCount = childCount;
}

/** ----- nsHTMLSelectOptionAccessible ----- */

/** Default Constructor */
nsHTMLSelectOptionAccessible::nsHTMLSelectOptionAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aDOMNode, aShell)
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
      if (role == nsIAccessibleRole::ROLE_COMBOBOX) {
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
  if (mParent && Role(mParent) == nsIAccessibleRole::ROLE_COMBOBOX_LIST) {
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_LISTITEM;
  }
  else {
    *aRole = nsIAccessibleRole::ROLE_COMBOBOX_LISTITEM;
  }
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
    nsCOMPtr<nsIContent> text = do_QueryInterface(child);
    if (text && text->IsNodeOfType(nsINode::eTEXT)) {
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

nsresult
nsHTMLSelectOptionAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsresult rv = nsHyperTextAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsCOMPtr<nsIContent> parentContent = content->GetParent();
  NS_ENSURE_TRUE(parentContent, NS_ERROR_FAILURE);

  PRUint32 level =
    parentContent->NodeInfo()->Equals(nsAccessibilityAtoms::optgroup) ? 2 : 1;
  PRUint32 childCount = parentContent->GetChildCount();
  PRUint32 indexOf = parentContent->IndexOf(content);

  nsAccessibilityUtils::
    SetAccGroupAttrs(aAttributes, level, indexOf + 1, childCount);
  return  NS_OK;
}

nsIFrame* nsHTMLSelectOptionAccessible::GetBoundsFrame()
{
  PRUint32 state;
  nsCOMPtr<nsIContent> content = GetSelectState(&state);
  if (state & nsIAccessibleStates::STATE_COLLAPSED) {
    nsCOMPtr<nsIPresShell> presShell(GetPresShell());
    if (!presShell) {
      return nsnull;
    }
    return presShell->GetPrimaryFrameFor(content);
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
NS_IMETHODIMP
nsHTMLSelectOptionAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = 0;
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
    *aState |= nsIAccessibleStates::STATE_FOCUSED;

  // Are we selected?
  nsCOMPtr<nsIDOMHTMLOptionElement> option (do_QueryInterface(mDOMNode));
  if ( option ) {
    PRBool isSelected = PR_FALSE;
    option->GetSelected(&isSelected);
    if ( isSelected ) 
      *aState |= nsIAccessibleStates::STATE_SELECTED;
  }

  *aState |= nsIAccessibleStates::STATE_SELECTABLE |
             nsIAccessibleStates::STATE_FOCUSABLE;

  // remove STATE_SHOWING if parent is STATE_COLLAPSED
  PRUint32 state;
  GetSelectState(&state);
  if (state & nsIAccessibleStates::STATE_COLLAPSED) {
    *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
  }

  return NS_OK;
}

/** select us! close combo box if necessary*/
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Select) {
    aName.AssignLiteral("select"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
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
    nsCOMPtr<nsIAccessible> parent(GetParent());
    nsCOMPtr<nsIAccessNode> accessNode(do_QueryInterface(parent));
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

    nsIFrame *selectFrame = presShell->GetPrimaryFrameFor(selectContent);
    nsIComboboxControlFrame *comboBoxFrame = nsnull;
    CallQueryInterface(selectFrame, &comboBoxFrame);
    if (comboBoxFrame) {
      nsIFrame *listFrame = comboBoxFrame->GetDropDown();
      if (comboBoxFrame->IsDroppedDown() && listFrame) {
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

  nsIFrame *frame = shell->GetPrimaryFrameFor(content);
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
      focusedOptionIndex = listFrame->GetSelectedIndex();
    }
    else  // Combo boxes can only have 1 selected option, so they can use the dom interface for this
      rv = selectElement->GetSelectedIndex(&focusedOptionIndex);
  }

  // Either use options and focused index, or default return null
  if (NS_SUCCEEDED(rv) && options && focusedOptionIndex >= 0) {  // Something is focused
    rv = options->Item(focusedOptionIndex, aFocusedOptionNode);
  }

  return rv;
}

void nsHTMLSelectOptionAccessible::SelectionChangedIfOption(nsIContent *aPossibleOption)
{
  if (!aPossibleOption || aPossibleOption->Tag() != nsAccessibilityAtoms::option ||
      !aPossibleOption->IsNodeOfType(nsINode::eHTML)) {
    return;
  }

  nsCOMPtr<nsIDOMNode> optionNode(do_QueryInterface(aPossibleOption));
  NS_ASSERTION(optionNode, "No option node for nsIContent with option tag!");

  nsCOMPtr<nsIAccessible> multiSelect = GetMultiSelectFor(optionNode);
  nsCOMPtr<nsPIAccessible> privateMultiSelect = do_QueryInterface(multiSelect);
  if (!privateMultiSelect) {
    return;
  }

  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  nsCOMPtr<nsIAccessible> optionAccessible;
  accService->GetAccessibleFor(optionNode, getter_AddRefs(optionAccessible));
  if (!optionAccessible) {
    return;
  }

  privateMultiSelect->FireToolkitEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN,
                      multiSelect, nsnull);
  PRUint32 state = State(optionAccessible);
  PRUint32 eventType = (state & nsIAccessibleStates::STATE_SELECTED) ?
                       nsIAccessibleEvent::EVENT_SELECTION_ADD :
                       nsIAccessibleEvent::EVENT_SELECTION_REMOVE; 
  privateMultiSelect->FireToolkitEvent(eventType, optionAccessible, nsnull);
}

nsIContent* nsHTMLSelectOptionAccessible::GetSelectState(PRUint32* aState)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  while (content && content->Tag() != nsAccessibilityAtoms::select) {
    content = content->GetParent();
  }

  nsCOMPtr<nsIDOMNode> selectNode(do_QueryInterface(content));
  if (selectNode) {
    nsCOMPtr<nsIAccessibilityService> accService = GetAccService();
    if (accService) {
      nsCOMPtr<nsIAccessible> selAcc;
      accService->GetAccessibleFor(selectNode, getter_AddRefs(selAcc));
      if (selAcc) {
        PRUint32 dummy;
        selAcc->GetFinalState(aState, &dummy);
        return content;
      }
    }
  }
  return nsnull; 
}

/** ----- nsHTMLSelectOptGroupAccessible ----- */

/** Default Constructor */
nsHTMLSelectOptGroupAccessible::nsHTMLSelectOptGroupAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsHTMLSelectOptionAccessible(aDOMNode, aShell)
{
}


/**
  * As a nsHTMLSelectOptGroupAccessible we can have the following states:
  *     nsIAccessibleStates::STATE_SELECTABLE
  */
NS_IMETHODIMP
nsHTMLSelectOptGroupAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHTMLSelectOptionAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~(nsIAccessibleStates::STATE_FOCUSABLE |
               nsIAccessibleStates::STATE_SELECTABLE);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
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
nsAccessibleWrap(aDOMNode, aShell)
{
}

/** We are a combobox */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_COMBOBOX;
  return NS_OK;
}

void nsHTMLComboboxAccessible::CacheChildren()
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    mAccChildCount = 0;
#ifdef COMBO_BOX_WITH_THREE_CHILDREN
    // We no longer create textfield and button accessible, in order to have
    // harmonization between IAccessible2, ATK/AT-SPI and OS X
    nsHTMLComboboxTextFieldAccessible* textFieldAccessible = 
      new nsHTMLComboboxTextFieldAccessible(this, mDOMNode, mWeakShell);
    SetFirstChild(textFieldAccessible);
    if (!textFieldAccessible) {
      return;
    }
    textFieldAccessible->SetParent(this);
    textFieldAccessible->Init();
    mAccChildCount = 1;  // Textfield accessible child successfully added

    nsHTMLComboboxButtonAccessible* buttonAccessible =
      new nsHTMLComboboxButtonAccessible(mParent, mDOMNode, mWeakShell);
    textFieldAccessible->SetNextSibling(buttonAccessible);
    if (!buttonAccessible) {
      return;
    }

    buttonAccessible->SetParent(this);
    buttonAccessible->Init();
    mAccChildCount = 2; // Button accessible child successfully added
#endif

    nsIFrame *frame = GetFrame();
    if (!frame) {
      return;
    }
    nsIComboboxControlFrame *comboFrame = nsnull;
    frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
    if (!comboFrame) {
      return;
    }
    nsIFrame *listFrame = comboFrame->GetDropDown();
    if (!listFrame) {
      return;
    }

    nsHTMLComboboxListAccessible* listAccessible = 
      new nsHTMLComboboxListAccessible(mParent, mDOMNode, mWeakShell);
#ifdef COMBO_BOX_WITH_THREE_CHILDREN
    buttonAccessible->SetNextSibling(listAccessible);
#else
    SetFirstChild(listAccessible);
#endif
    if (!listAccessible) {
      return;
    }

    listAccessible->SetParent(this);
    listAccessible->SetNextSibling(nsnull);
    listAccessible->Init();

    ++ mAccChildCount;  // List accessible child successfully added
  }
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
NS_IMETHODIMP
nsHTMLComboboxAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);

  if (comboFrame && comboFrame->IsDroppedDown())
    *aState |= nsIAccessibleStates::STATE_EXPANDED;
  else
    *aState |= nsIAccessibleStates::STATE_COLLAPSED;

  *aState |= nsIAccessibleStates::STATE_HASPOPUP |
             nsIAccessibleStates::STATE_READONLY |
             nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxAccessible::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aaa:describedby
  nsAccessible::GetDescription(aDescription);
  if (!aDescription.IsEmpty()) {
    return NS_OK;
  }
  // Use description of currently focused option
  nsCOMPtr<nsIAccessible> optionAccessible = GetFocusedOptionAccessible();
  return optionAccessible ? optionAccessible->GetDescription(aDescription) : NS_OK;
}

already_AddRefed<nsIAccessible>
nsHTMLComboboxAccessible::GetFocusedOptionAccessible()
{
  if (!mWeakShell) {
    return nsnull;  // Shut down
  }
  nsCOMPtr<nsIDOMNode> focusedOptionNode;
  nsHTMLSelectOptionAccessible::GetFocusedOptionNode(mDOMNode, getter_AddRefs(focusedOptionNode));
  nsIAccessibilityService *accService = GetAccService();
  if (!focusedOptionNode || !accService) {
    return nsnull;
  }

  nsIAccessible *optionAccessible;
  accService->GetAccessibleInWeakShell(focusedOptionNode, mWeakShell, 
                                       &optionAccessible);
  return optionAccessible;
}

/**
  * MSAA/ATK accessible value != HTML value, especially not in combo boxes.
  * Our accessible value is the text label for of our ( first ) selected child.
  * The easiest way to get this is from the first child which is the readonly textfield.
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetValue(nsAString& aValue)
{
  // Use label of currently focused option
  nsCOMPtr<nsIAccessible> optionAccessible = GetFocusedOptionAccessible();
  NS_ENSURE_TRUE(optionAccessible, NS_ERROR_FAILURE);
  return optionAccessible->GetName(aValue);
}

/** Just one action ( click ). */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 1;
  return NS_OK;
}

/**
  * Programmaticaly toggle the combo box
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != nsHTMLComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIFrame *frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIComboboxControlFrame *comboFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  // Reverse whether it's dropped down or not
  comboFrame->ShowDropDown(!comboFrame->IsDroppedDown());

  return NS_OK;
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex != nsHTMLComboboxAccessible::eAction_Click) {
    return NS_ERROR_INVALID_ARG;
  }
  nsIFrame *frame = GetFrame();
  if (!frame) {
    return NS_ERROR_FAILURE;
  }
  nsIComboboxControlFrame *comboFrame = nsnull;
  frame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}


#ifdef COMBO_BOX_WITH_THREE_CHILDREN
/** ----- nsHTMLComboboxTextFieldAccessible ----- */

/** Constructor */
nsHTMLComboboxTextFieldAccessible::nsHTMLComboboxTextFieldAccessible(nsIAccessible* aParent, 
                                                                     nsIDOMNode* aDOMNode, 
                                                                     nsIWeakReference* aShell):
nsHTMLTextFieldAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsHTMLComboboxTextFieldAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same as for our parent, use |this| pointer as the unique Id
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

void nsHTMLComboboxTextFieldAccessible::CacheChildren()
{
  // Allow single text anonymous child, so that nsHyperTextAccessible can operate correctly
  // We must override this otherwise we get the dropdown button as a child of the textfield,
  // and at least for now we want to keep it as a sibling
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  // Allows only 1 child
  if (mAccChildCount == eChildCountUninitialized) {
    nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, PR_TRUE);
    // Seed the frame hint early while we're still on a container node.
    // This is better than doing the GetPrimaryFrameFor() later on
    // a text node, because text nodes aren't in the frame map.
    walker.mState.frame = GetFrame();

    walker.GetFirstChild();
    SetFirstChild(walker.mState.accessible);
    nsCOMPtr<nsPIAccessible> privateChild = 
      do_QueryInterface(walker.mState.accessible);
    privateChild->SetParent(this);
    privateChild->SetNextSibling(nsnull);
    mAccChildCount = 1;
  }
}

/** -----ComboboxButtonAccessible ----- */

/** Constructor -- cache our parent */
nsHTMLComboboxButtonAccessible::nsHTMLComboboxButtonAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
}

/** Just one action ( click ). */
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetNumActions(PRUint8 *aNumActions)
{
  *aNumActions = 1;
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
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;

  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open");

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
  *_retval = nsIAccessibleRole::ROLE_PUSHBUTTON;
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
NS_IMETHODIMP nsHTMLComboboxButtonAccessible::GetName(nsAString& aName)
{
  return GetActionName(eAction_Click, aName);
}

/**
  * As a nsHTMLComboboxButtonAccessible we can have the following states:
  *     STATE_PRESSED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP
nsHTMLComboboxButtonAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;

  if (comboFrame->IsDroppedDown())
    *aState |= nsIAccessibleStates::STATE_PRESSED;

  *aState |= nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}
#endif

/** ----- nsHTMLComboboxListAccessible ----- */

nsHTMLComboboxListAccessible::nsHTMLComboboxListAccessible(nsIAccessible *aParent,
                                                           nsIDOMNode* aDOMNode,
                                                           nsIWeakReference* aShell):
nsHTMLSelectListAccessible(aDOMNode, aShell)
{
}

nsIFrame *nsHTMLComboboxListAccessible::GetFrame()
{
  nsIFrame* frame = nsHTMLSelectListAccessible::GetFrame();

  if (frame) {
    nsIComboboxControlFrame* comboBox;
    CallQueryInterface(frame, &comboBox);
    if (comboBox) {
      return comboBox->GetDropDown();
    }
  }

  return nsnull;
}

/**
  * As a nsHTMLComboboxListAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE
  *     STATE_FLOATING
  */
NS_IMETHODIMP
nsHTMLComboboxListAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = nsnull;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;

  if (comboFrame->IsDroppedDown())
    *aState |= nsIAccessibleStates::STATE_FLOATING |
               nsIAccessibleStates::STATE_FOCUSABLE;
  else
    *aState |= nsIAccessibleStates::STATE_INVISIBLE |
               nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsHTMLComboboxListAccessible::GetParent(nsIAccessible **aParent)
{
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
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

  nsCOMPtr<nsIContent> content(do_QueryInterface(child));
  nsIFrame* frame = shell->GetPrimaryFrameFor(content);
  if (!frame) {
    *aBoundingFrame = nsnull;
    return;
  }

  *aBoundingFrame = frame->GetParent();
  aBounds = (*aBoundingFrame)->GetRect();
}
