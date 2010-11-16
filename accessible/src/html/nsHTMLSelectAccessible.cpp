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

#include "nsHTMLSelectAccessible.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsDocAccessible.h"
#include "nsEventShell.h"
#include "nsIAccessibleEvent.h"
#include "nsTextEquivUtils.h"

#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIListControlFrame.h"
#include "nsIServiceManager.h"
#include "nsIMutableArray.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLSelectListAccessible::
  nsHTMLSelectListAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: nsAccessible public

nsresult
nsHTMLSelectListAccessible::GetStateInternal(PRUint32 *aState,
                                             PRUint32 *aExtraState)
{
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // As a nsHTMLSelectListAccessible we can have the following states:
  //   nsIAccessibleStates::STATE_MULTISELECTABLE
  //   nsIAccessibleStates::STATE_EXTSELECTABLE

  if (*aState & nsIAccessibleStates::STATE_FOCUSED) {
    // Treat first focusable option node as actual focus, in order
    // to avoid confusing JAWS, which needs focus on the option
    nsCOMPtr<nsIContent> focusedOption =
      nsHTMLSelectOptionAccessible::GetFocusedOption(mContent);
    if (focusedOption) { // Clear focused state since it is on option
      *aState &= ~nsIAccessibleStates::STATE_FOCUSED;
    }
  }
  if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::multiple))
    *aState |= nsIAccessibleStates::STATE_MULTISELECTABLE |
               nsIAccessibleStates::STATE_EXTSELECTABLE;

  return NS_OK;
}

PRUint32
nsHTMLSelectListAccessible::NativeRole()
{
  if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX)
    return nsIAccessibleRole::ROLE_COMBOBOX_LIST;

  return nsIAccessibleRole::ROLE_LISTBOX;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: SelectAccessible

bool
nsHTMLSelectListAccessible::IsSelect()
{
  return true;
}

bool
nsHTMLSelectListAccessible::SelectAll()
{
  return mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::multiple) ?
           nsAccessibleWrap::SelectAll() : false;
}

bool
nsHTMLSelectListAccessible::UnselectAll()
{
  return mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::multiple) ?
           nsAccessibleWrap::UnselectAll() : false;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: nsAccessible protected

void
nsHTMLSelectListAccessible::CacheChildren()
{
  // Cache accessibles for <optgroup> and <option> DOM decendents as children,
  // as well as the accessibles for them. Avoid whitespace text nodes. We want
  // to count all the <optgroup>s and <option>s as children because we want
  // a flat tree under the Select List.
  CacheOptSiblings(mContent);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible protected

void
nsHTMLSelectListAccessible::CacheOptSiblings(nsIContent *aParentContent)
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  PRUint32 numChildren = aParentContent->GetChildCount();
  for (PRUint32 count = 0; count < numChildren; count ++) {
    nsIContent *childContent = aParentContent->GetChildAt(count);
    if (!childContent->IsHTML()) {
      continue;
    }

    nsCOMPtr<nsIAtom> tag = childContent->Tag();
    if (tag == nsAccessibilityAtoms::option ||
        tag == nsAccessibilityAtoms::optgroup) {

      // Get an accessible for option or optgroup and cache it.
      nsRefPtr<nsAccessible> accessible =
        GetAccService()->GetOrCreateAccessible(childContent, presShell,
                                               mWeakShell);
      if (accessible)
        AppendChild(accessible);

      // Deep down into optgroup element.
      if (tag == nsAccessibilityAtoms::optgroup)
        CacheOptSiblings(childContent);
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLSelectOptionAccessible::
  nsHTMLSelectOptionAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: nsAccessible public

PRUint32
nsHTMLSelectOptionAccessible::NativeRole()
{
  if (mParent && mParent->Role() == nsIAccessibleRole::ROLE_COMBOBOX_LIST)
    return nsIAccessibleRole::ROLE_COMBOBOX_OPTION;

  return nsIAccessibleRole::ROLE_OPTION;
}

nsresult
nsHTMLSelectOptionAccessible::GetNameInternal(nsAString& aName)
{
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  mContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::label, aName);
  if (!aName.IsEmpty())
    return NS_OK;

  // CASE #2 -- no label parameter, get the first child, 
  // use it if it is a text node
  nsIContent *text = mContent->GetChildAt(0);
  if (!text)
    return NS_OK;

  if (text->IsNodeOfType(nsINode::eTEXT)) {
    nsAutoString txtValue;
    nsresult rv = nsTextEquivUtils::
      AppendTextEquivFromTextContent(text, &txtValue);
    NS_ENSURE_SUCCESS(rv, rv);

    // Temp var (txtValue) needed until CompressWhitespace built for nsAString
    txtValue.CompressWhitespace();
    aName.Assign(txtValue);
    return NS_OK;
  }

  return NS_OK;
}

// nsAccessible protected
nsIFrame* nsHTMLSelectOptionAccessible::GetBoundsFrame()
{
  PRUint32 state = 0;
  nsCOMPtr<nsIContent> content = GetSelectState(&state);
  if (state & nsIAccessibleStates::STATE_COLLAPSED) {
    if (content) {
      return content->GetPrimaryFrame();
    }

    return nsnull;
  }

  return nsAccessible::GetBoundsFrame();
}

/**
  * As a nsHTMLSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_OFFSCREEN
  */
nsresult
nsHTMLSelectOptionAccessible::GetStateInternal(PRUint32 *aState,
                                               PRUint32 *aExtraState)
{
  // Upcall to nsAccessible, but skip nsHyperTextAccessible impl
  // because we don't want EXT_STATE_EDITABLE or EXT_STATE_SELECTABLE_TEXT
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  PRUint32 selectState = 0, selectExtState = 0;
  nsCOMPtr<nsIContent> selectContent = GetSelectState(&selectState,
                                                      &selectExtState);
  if (selectState & nsIAccessibleStates::STATE_INVISIBLE) {
    return NS_OK;
  }

  NS_ENSURE_TRUE(selectContent, NS_ERROR_FAILURE);

  // Is disabled?
  if (0 == (*aState & nsIAccessibleStates::STATE_UNAVAILABLE)) {
    *aState |= (nsIAccessibleStates::STATE_FOCUSABLE |
                nsIAccessibleStates::STATE_SELECTABLE);
    // When the list is focused but no option is actually focused,
    // Firefox draws a focus ring around the first non-disabled option.
    // We need to indicated STATE_FOCUSED in that case, because it
    // prevents JAWS from ignoring the list
    // GetFocusedOption() ensures that an option node is
    // returned in this case, as long as some focusable option exists
    // in the listbox
    nsCOMPtr<nsIContent> focusedOption = GetFocusedOption(selectContent);
    if (focusedOption == mContent)
      *aState |= nsIAccessibleStates::STATE_FOCUSED;
  }

  // Are we selected?
  PRBool isSelected = PR_FALSE;
  nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(mContent));
  if (option) {
    option->GetSelected(&isSelected);
    if ( isSelected ) 
      *aState |= nsIAccessibleStates::STATE_SELECTED;
  }

  if (selectState & nsIAccessibleStates::STATE_OFFSCREEN) {
    *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
  }
  else if (selectState & nsIAccessibleStates::STATE_COLLAPSED) {
    // <select> is COLLAPSED: add STATE_OFFSCREEN, if not the currently
    // visible option
    if (!isSelected) {
      *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
    }
    else {
      // Clear offscreen and invisible for currently showing option
      *aState &= ~nsIAccessibleStates::STATE_OFFSCREEN;
      *aState &= ~nsIAccessibleStates::STATE_INVISIBLE;
       if (aExtraState) {
         *aExtraState |= selectExtState & nsIAccessibleStates::EXT_STATE_OPAQUE;
       }
    }
  }
  else {
    // XXX list frames are weird, don't rely on nsAccessible's general
    // visibility implementation unless they get reimplemented in layout
    *aState &= ~nsIAccessibleStates::STATE_OFFSCREEN;
    // <select> is not collapsed: compare bounds to calculate STATE_OFFSCREEN
    nsAccessible* listAcc = GetParent();
    if (listAcc) {
      PRInt32 optionX, optionY, optionWidth, optionHeight;
      PRInt32 listX, listY, listWidth, listHeight;
      GetBounds(&optionX, &optionY, &optionWidth, &optionHeight);
      listAcc->GetBounds(&listX, &listY, &listWidth, &listHeight);
      if (optionY < listY || optionY + optionHeight > listY + listHeight) {
        *aState |= nsIAccessibleStates::STATE_OFFSCREEN;
      }
    }
  }
 
  return NS_OK;
}

PRInt32
nsHTMLSelectOptionAccessible::GetLevelInternal()
{
  nsIContent *parentContent = mContent->GetParent();

  PRInt32 level =
    parentContent->NodeInfo()->Equals(nsAccessibilityAtoms::optgroup) ? 2 : 1;

  if (level == 1 && Role() != nsIAccessibleRole::ROLE_HEADING)
    level = 0; // In a single level list, the level is irrelevant

  return level;
}

void
nsHTMLSelectOptionAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                         PRInt32 *aSetSize)
{
  nsIContent *parentContent = mContent->GetParent();

  PRInt32 posInSet = 0, setSize = 0;
  PRBool isContentFound = PR_FALSE;

  PRUint32 childCount = parentContent->GetChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    nsIContent *childContent = parentContent->GetChildAt(childIdx);
    if (childContent->NodeInfo()->Equals(mContent->NodeInfo())) {
      if (!isContentFound) {
        if (childContent == mContent)
          isContentFound = PR_TRUE;

        posInSet++;
      }
      setSize++;
    }
  }

  *aSetSize = setSize;
  *aPosInSet = posInSet;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: nsIAccessible

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
    nsCOMPtr<nsIDOMHTMLOptionElement> newHTMLOption(do_QueryInterface(mContent));
    if (!newHTMLOption) 
      return NS_ERROR_FAILURE;
    // Clear old selection
    nsAccessible* parent = GetParent();
    NS_ASSERTION(parent, "No parent!");

    nsCOMPtr<nsIContent> oldHTMLOptionContent =
      GetFocusedOption(parent->GetContent());
    nsCOMPtr<nsIDOMHTMLOptionElement> oldHTMLOption =
      do_QueryInterface(oldHTMLOptionContent);
    if (oldHTMLOption)
      oldHTMLOption->SetSelected(PR_FALSE);
    // Set new selection
    newHTMLOption->SetSelected(PR_TRUE);

    // If combo box, and open, close it
    // First, get the <select> widgets list control frame
    nsIContent *selectContent = mContent;
    do {
      selectContent = selectContent->GetParent();
      nsCOMPtr<nsIDOMHTMLSelectElement> selectControl =
        do_QueryInterface(selectContent);
      if (selectControl)
        break;

    } while (selectContent);

    nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
    nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(mContent));

    if (!selectContent || !presShell || !option)
      return NS_ERROR_FAILURE;

    nsIFrame *selectFrame = selectContent->GetPrimaryFrame();
    nsIComboboxControlFrame *comboBoxFrame = do_QueryFrame(selectFrame);
    if (comboBoxFrame) {
      nsIFrame *listFrame = comboBoxFrame->GetDropDown();
      if (comboBoxFrame->IsDroppedDown() && listFrame) {
        // use this list control frame to roll up the list
        nsIListControlFrame *listControlFrame = do_QueryFrame(listFrame);
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

NS_IMETHODIMP
nsHTMLSelectOptionAccessible::SetSelected(PRBool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLOptionElement> optionElm(do_QueryInterface(mContent));
  return optionElm->SetSelected(aSelect);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: static methods

/**
  * Helper method for getting the focused DOM Node from our parent(list) node. We
  *  need to use the frame to get the focused option because for some reason we
  *  weren't getting the proper notification when the focus changed using the DOM
  */
already_AddRefed<nsIContent>
nsHTMLSelectOptionAccessible::GetFocusedOption(nsIContent *aListNode)
{
  NS_ASSERTION(aListNode, "Called GetFocusedOptionNode without a valid list node");

  nsIFrame *frame = aListNode->GetPrimaryFrame();
  if (!frame)
    return nsnull;

  PRInt32 focusedOptionIndex = 0;

  // Get options
  nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(aListNode));
  NS_ASSERTION(selectElement, "No select element where it should be");

  nsCOMPtr<nsIDOMHTMLOptionsCollection> options;
  nsresult rv = selectElement->GetOptions(getter_AddRefs(options));
  
  if (NS_SUCCEEDED(rv)) {
    nsIListControlFrame *listFrame = do_QueryFrame(frame);
    if (listFrame) {
      // Get what's focused in listbox by asking frame for "selected item". 
      // Can't use dom interface for this, because it will always return the first selected item
      // when there is more than 1 item selected. We need the focused item, not
      // the first selected item.
      focusedOptionIndex = listFrame->GetSelectedIndex();
      if (focusedOptionIndex == -1) {
        nsCOMPtr<nsIDOMNode> nextOption;
        while (PR_TRUE) {
          ++ focusedOptionIndex;
          options->Item(focusedOptionIndex, getter_AddRefs(nextOption));
          nsCOMPtr<nsIDOMHTMLOptionElement> optionElement = do_QueryInterface(nextOption);
          if (!optionElement) {
            break;
          }
          PRBool disabled;
          optionElement->GetDisabled(&disabled);
          if (!disabled) {
            break;
          }
        }
      }
    }
    else  // Combo boxes can only have 1 selected option, so they can use the dom interface for this
      rv = selectElement->GetSelectedIndex(&focusedOptionIndex);
  }

  // Either use options and focused index, or default return null
  if (NS_SUCCEEDED(rv) && options && focusedOptionIndex >= 0) {  // Something is focused
    nsCOMPtr<nsIDOMNode> focusedOptionNode;
    options->Item(focusedOptionIndex, getter_AddRefs(focusedOptionNode));
    nsIContent *focusedOption = nsnull;
    if (focusedOptionNode)
      CallQueryInterface(focusedOptionNode, &focusedOption);
    return focusedOption;
  }

  return nsnull;
}

void
nsHTMLSelectOptionAccessible::SelectionChangedIfOption(nsIContent *aPossibleOptionNode)
{
  if (!aPossibleOptionNode ||
      aPossibleOptionNode->Tag() != nsAccessibilityAtoms::option ||
      !aPossibleOptionNode->IsHTML()) {
    return;
  }

  nsAccessible *multiSelect =
    nsAccUtils::GetMultiSelectableContainer(aPossibleOptionNode);
  if (!multiSelect)
    return;

  nsAccessible *option = GetAccService()->GetAccessible(aPossibleOptionNode);
  if (!option)
    return;


  nsRefPtr<AccEvent> selWithinEvent =
    new AccEvent(nsIAccessibleEvent::EVENT_SELECTION_WITHIN, multiSelect);

  if (!selWithinEvent)
    return;

  option->GetDocAccessible()->FireDelayedAccessibleEvent(selWithinEvent);

  PRUint32 state = nsAccUtils::State(option);
  PRUint32 eventType;
  if (state & nsIAccessibleStates::STATE_SELECTED) {
    eventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
  }
  else {
    eventType = nsIAccessibleEvent::EVENT_SELECTION_REMOVE;
  }

  nsRefPtr<AccEvent> selAddRemoveEvent = new AccEvent(eventType, option);

  if (selAddRemoveEvent)
    option->GetDocAccessible()->FireDelayedAccessibleEvent(selAddRemoveEvent);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: private methods

nsIContent* nsHTMLSelectOptionAccessible::GetSelectState(PRUint32* aState,
                                                         PRUint32* aExtraState)
{
  *aState = 0;

  if (aExtraState)
    *aExtraState = 0;

  nsIContent *content = mContent;
  while (content && content->Tag() != nsAccessibilityAtoms::select) {
    content = content->GetParent();
  }

  if (content) {
    nsAccessible* selAcc = GetAccService()->GetAccessible(content);
    if (selAcc) {
      selAcc->GetState(aState, aExtraState);
      return content;
    }
  }
  return nsnull; 
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptGroupAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLSelectOptGroupAccessible::
  nsHTMLSelectOptGroupAccessible(nsIContent *aContent,
                                 nsIWeakReference *aShell) :
  nsHTMLSelectOptionAccessible(aContent, aShell)
{
}

PRUint32
nsHTMLSelectOptGroupAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_HEADING;
}

nsresult
nsHTMLSelectOptGroupAccessible::GetStateInternal(PRUint32 *aState,
                                                 PRUint32 *aExtraState)
{
  nsresult rv = nsHTMLSelectOptionAccessible::GetStateInternal(aState,
                                                               aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

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

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptGroupAccessible: nsAccessible protected

void
nsHTMLSelectOptGroupAccessible::CacheChildren()
{
  // XXX To do (bug 378612) - create text child for the anonymous attribute
  // content, so that nsIAccessibleText is supported for the <optgroup> as it is
  // for an <option>. Attribute content is what layout creates for
  // the label="foo" on the <optgroup>. See eStyleContentType_Attr and
  // CreateAttributeContent() in nsCSSFrameConstructor
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLComboboxAccessible::
  nsHTMLComboboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible: nsAccessible

PRUint32
nsHTMLComboboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_COMBOBOX;
}

void
nsHTMLComboboxAccessible::CacheChildren()
{
  nsIFrame* frame = GetFrame();
  if (!frame)
    return;

  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (!comboFrame)
    return;

  nsIFrame *listFrame = comboFrame->GetDropDown();
  if (!listFrame)
    return;

  if (!mListAccessible) {
    mListAccessible = 
      new nsHTMLComboboxListAccessible(mParent, mContent, mWeakShell);

    // Initialize and put into cache.
    if (!GetDocAccessible()->BindToDocument(mListAccessible, nsnull))
      return;
  }

  if (AppendChild(mListAccessible)) {
    // Cache combobox option accessibles so that we build complete accessible
    // tree for combobox.
    mListAccessible->EnsureChildren();
  }
}

void
nsHTMLComboboxAccessible::Shutdown()
{
  nsAccessibleWrap::Shutdown();

  if (mListAccessible) {
    mListAccessible->Shutdown();
    mListAccessible = nsnull;
  }
}

/**
  * As a nsHTMLComboboxAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_HASPOPUP
  *     STATE_EXPANDED
  *     STATE_COLLAPSED
  */
nsresult
nsHTMLComboboxAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (comboFrame && comboFrame->IsDroppedDown()) {
    *aState |= nsIAccessibleStates::STATE_EXPANDED;
  }
  else {
    *aState &= ~nsIAccessibleStates::STATE_FOCUSED; // Focus is on an option
    *aState |= nsIAccessibleStates::STATE_COLLAPSED;
  }

  *aState |= nsIAccessibleStates::STATE_HASPOPUP |
             nsIAccessibleStates::STATE_FOCUSABLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLComboboxAccessible::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aria-describedby
  nsAccessible::GetDescription(aDescription);
  if (!aDescription.IsEmpty()) {
    return NS_OK;
  }
  // Use description of currently focused option
  nsAccessible *option = GetFocusedOptionAccessible();
  return option ? option->GetDescription(aDescription) : NS_OK;
}

nsAccessible *
nsHTMLComboboxAccessible::GetFocusedOptionAccessible()
{
  if (IsDefunct())
    return nsnull;

  nsCOMPtr<nsIContent> focusedOption =
    nsHTMLSelectOptionAccessible::GetFocusedOption(mContent);
  if (!focusedOption) {
    return nsnull;
  }

  return GetAccService()->GetAccessibleInWeakShell(focusedOption,
                                                   mWeakShell);
}

/**
  * MSAA/ATK accessible value != HTML value, especially not in combo boxes.
  * Our accessible value is the text label for of our ( first ) selected child.
  * The easiest way to get this is from the first child which is the readonly textfield.
  */
NS_IMETHODIMP nsHTMLComboboxAccessible::GetValue(nsAString& aValue)
{
  // Use accessible name of currently focused option.
  nsAccessible *option = GetFocusedOptionAccessible();
  return option ? option->GetName(aValue) : NS_OK;
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
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
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
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (!comboFrame) {
    return NS_ERROR_FAILURE;
  }
  if (comboFrame->IsDroppedDown())
    aName.AssignLiteral("close"); 
  else
    aName.AssignLiteral("open"); 

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxListAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLComboboxListAccessible::
  nsHTMLComboboxListAccessible(nsIAccessible *aParent, nsIContent *aContent,
                               nsIWeakReference *aShell) :
  nsHTMLSelectListAccessible(aContent, aShell)
{
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible: nsAccessNode

nsIFrame*
nsHTMLComboboxListAccessible::GetFrame()
{
  nsIFrame* frame = nsHTMLSelectListAccessible::GetFrame();

  if (frame) {
    nsIComboboxControlFrame* comboBox = do_QueryFrame(frame);
    if (comboBox) {
      return comboBox->GetDropDown();
    }
  }

  return nsnull;
}

bool
nsHTMLComboboxListAccessible::IsPrimaryForNode() const
{
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible: nsAccessible

/**
  * As a nsHTMLComboboxListAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE
  *     STATE_FLOATING
  */
nsresult
nsHTMLComboboxListAccessible::GetStateInternal(PRUint32 *aState,
                                               PRUint32 *aExtraState)
{
  // Get focus status from base class
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(boundsFrame);
  if (comboFrame && comboFrame->IsDroppedDown())
    *aState |= nsIAccessibleStates::STATE_FLOATING;
  else
    *aState |= nsIAccessibleStates::STATE_INVISIBLE;

  return NS_OK;
}

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxListAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  *aBoundingFrame = nsnull;

  nsAccessible* comboAcc = GetParent();
  if (!comboAcc)
    return;

  if (0 == (nsAccUtils::State(comboAcc) & nsIAccessibleStates::STATE_COLLAPSED)) {
    nsHTMLSelectListAccessible::GetBoundsRect(aBounds, aBoundingFrame);
    return;
  }

  // Get the first option.
  nsIContent* content = mContent->GetChildAt(0);
  if (!content) {
    return;
  }
  nsIFrame* frame = content->GetPrimaryFrame();
  if (!frame) {
    *aBoundingFrame = nsnull;
    return;
  }

  *aBoundingFrame = frame->GetParent();
  aBounds = (*aBoundingFrame)->GetRect();
}
