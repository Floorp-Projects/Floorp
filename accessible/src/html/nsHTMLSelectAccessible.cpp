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
#include "States.h"

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

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLSelectListAccessible::
  nsHTMLSelectListAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
  mFlags |= eListControlAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: nsAccessible public

PRUint64
nsHTMLSelectListAccessible::NativeState()
{
  PRUint64 state = nsAccessibleWrap::NativeState();
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple))
    state |= states::MULTISELECTABLE | states::EXTSELECTABLE;

  return state;
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
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
           nsAccessibleWrap::SelectAll() : false;
}

bool
nsHTMLSelectListAccessible::UnselectAll()
{
  return mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::multiple) ?
           nsAccessibleWrap::UnselectAll() : false;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectListAccessible: Widgets

bool
nsHTMLSelectListAccessible::IsWidget() const
{
  return true;
}

bool
nsHTMLSelectListAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
nsHTMLSelectListAccessible::AreItemsOperable() const
{
  return true;
}

nsAccessible*
nsHTMLSelectListAccessible::CurrentItem()
{
  nsIListControlFrame* listControlFrame = do_QueryFrame(GetFrame());
  if (listControlFrame) {
    nsCOMPtr<nsIContent> activeOptionNode = listControlFrame->GetCurrentOption();
    if (activeOptionNode) {
      nsDocAccessible* document = GetDocAccessible();
      if (document)
        return document->GetAccessible(activeOptionNode);
    }
  }
  return nsnull;
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
    if (tag == nsGkAtoms::option ||
        tag == nsGkAtoms::optgroup) {

      // Get an accessible for option or optgroup and cache it.
      nsRefPtr<nsAccessible> accessible =
        GetAccService()->GetOrCreateAccessible(childContent, presShell,
                                               mWeakShell);
      if (accessible)
        AppendChild(accessible);

      // Deep down into optgroup element.
      if (tag == nsGkAtoms::optgroup)
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
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::label, aName);
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
  PRUint64 state = 0;
  nsIContent* content = GetSelectState(&state);
  if (state & states::COLLAPSED) {
    if (content) {
      return content->GetPrimaryFrame();
    }

    return nsnull;
  }

  return nsAccessible::GetBoundsFrame();
}

PRUint64
nsHTMLSelectOptionAccessible::NativeState()
{
  // As a nsHTMLSelectOptionAccessible we can have the following states:
  // SELECTABLE, SELECTED, FOCUSED, FOCUSABLE, OFFSCREEN
  // Upcall to nsAccessible, but skip nsHyperTextAccessible impl
  // because we don't want EDITABLE or SELECTABLE_TEXT
  PRUint64 state = nsAccessible::NativeState();

  PRUint64 selectState = 0;
  nsIContent* selectContent = GetSelectState(&selectState);
  if (!selectContent || selectState & states::INVISIBLE)
    return state;

  // Focusable and selectable
  if (!(state & states::UNAVAILABLE))
    state |= (states::FOCUSABLE | states::SELECTABLE);

  // Are we selected?
  bool isSelected = false;
  nsCOMPtr<nsIDOMHTMLOptionElement> option(do_QueryInterface(mContent));
  if (option) {
    option->GetSelected(&isSelected);
    if (isSelected)
      state |= states::SELECTED;
  }

  if (selectState & states::OFFSCREEN) {
    state |= states::OFFSCREEN;
  }
  else if (selectState & states::COLLAPSED) {
    // <select> is COLLAPSED: add OFFSCREEN, if not the currently
    // visible option
    if (!isSelected) {
      state |= states::OFFSCREEN;
    }
    else {
      // Clear offscreen and invisible for currently showing option
      state &= ~(states::OFFSCREEN | states::INVISIBLE);
      state |= selectState & states::OPAQUE1;
    }
  }
  else {
    // XXX list frames are weird, don't rely on nsAccessible's general
    // visibility implementation unless they get reimplemented in layout
    state &= ~states::OFFSCREEN;
    // <select> is not collapsed: compare bounds to calculate OFFSCREEN
    nsAccessible* listAcc = Parent();
    if (listAcc) {
      PRInt32 optionX, optionY, optionWidth, optionHeight;
      PRInt32 listX, listY, listWidth, listHeight;
      GetBounds(&optionX, &optionY, &optionWidth, &optionHeight);
      listAcc->GetBounds(&listX, &listY, &listWidth, &listHeight);
      if (optionY < listY || optionY + optionHeight > listY + listHeight) {
        state |= states::OFFSCREEN;
      }
    }
  }
 
  return state;
}

PRInt32
nsHTMLSelectOptionAccessible::GetLevelInternal()
{
  nsIContent *parentContent = mContent->GetParent();

  PRInt32 level =
    parentContent->NodeInfo()->Equals(nsGkAtoms::optgroup) ? 2 : 1;

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
  bool isContentFound = false;

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

PRUint8
nsHTMLSelectOptionAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsHTMLSelectOptionAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Select)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLSelectOptionAccessible::SetSelected(bool aSelect)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLOptionElement> optionElm(do_QueryInterface(mContent));
  return optionElm->SetSelected(aSelect);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: Widgets

nsAccessible*
nsHTMLSelectOptionAccessible::ContainerWidget() const
{
  if (mParent && mParent->IsListControl()) {
    nsAccessible* grandParent = mParent->Parent();
    if (grandParent && grandParent->IsCombobox())
      return grandParent;

    return mParent;
  }
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLSelectOptionAccessible: static methods

void
nsHTMLSelectOptionAccessible::SelectionChangedIfOption(nsIContent *aPossibleOptionNode)
{
  if (!aPossibleOptionNode ||
      aPossibleOptionNode->Tag() != nsGkAtoms::option ||
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

  PRUint64 state = option->State();
  PRUint32 eventType;
  if (state & states::SELECTED) {
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

nsIContent*
nsHTMLSelectOptionAccessible::GetSelectState(PRUint64* aState)
{
  *aState = 0;

  nsIContent *content = mContent;
  while (content && content->Tag() != nsGkAtoms::select) {
    content = content->GetParent();
  }

  if (content) {
    nsAccessible* selAcc = GetAccService()->GetAccessible(content);
    if (selAcc) {
      *aState = selAcc->State();
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

PRUint64
nsHTMLSelectOptGroupAccessible::NativeState()
{
  PRUint64 state = nsHTMLSelectOptionAccessible::NativeState();

  state &= ~(states::FOCUSABLE | states::SELECTABLE);

  return state;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::DoAction(PRUint8 index)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLSelectOptGroupAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

PRUint8
nsHTMLSelectOptGroupAccessible::ActionCount()
{
  return 0;
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
  mFlags |= eComboboxAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible: nsAccessible

PRUint32
nsHTMLComboboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_COMBOBOX;
}

void
nsHTMLComboboxAccessible::InvalidateChildren()
{
  nsAccessibleWrap::InvalidateChildren();

  if (mListAccessible)
    mListAccessible->InvalidateChildren();
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
  */
PRUint64
nsHTMLComboboxAccessible::NativeState()
{
  // As a nsHTMLComboboxAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, HASPOPUP, EXPANDED, COLLAPSED
  // Get focus status from base class
  PRUint64 state = nsAccessible::NativeState();

  nsIFrame *frame = GetBoundsFrame();
  nsIComboboxControlFrame *comboFrame = do_QueryFrame(frame);
  if (comboFrame && comboFrame->IsDroppedDown())
    state |= states::EXPANDED;
  else
    state |= states::COLLAPSED;

  state |= states::HASPOPUP;
  return state;
}

void
nsHTMLComboboxAccessible::Description(nsString& aDescription)
{
  aDescription.Truncate();
  // First check to see if combo box itself has a description, perhaps through
  // tooltip (title attribute) or via aria-describedby
  nsAccessible::Description(aDescription);
  if (!aDescription.IsEmpty())
    return;

  // Otherwise use description of selected option.
  nsAccessible* option = SelectedOption();
  if (option)
    option->Description(aDescription);
}

NS_IMETHODIMP nsHTMLComboboxAccessible::GetValue(nsAString& aValue)
{
  // Use accessible name of selected option.
  nsAccessible* option = SelectedOption();
  return option ? option->GetName(aValue) : NS_OK;
}

PRUint8
nsHTMLComboboxAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
nsHTMLComboboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
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
// nsHTMLComboboxAccessible: Widgets

bool
nsHTMLComboboxAccessible::IsWidget() const
{
  return true;
}

bool
nsHTMLComboboxAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
nsHTMLComboboxAccessible::AreItemsOperable() const
{
  nsIComboboxControlFrame* comboboxFrame = do_QueryFrame(GetFrame());
  return comboboxFrame && comboboxFrame->IsDroppedDown();
}

nsAccessible*
nsHTMLComboboxAccessible::CurrentItem()
{
  // No current item for collapsed combobox.
  return SelectedOption(true);
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLComboboxAccessible: protected

nsAccessible*
nsHTMLComboboxAccessible::SelectedOption(bool aIgnoreIfCollapsed) const
{
  nsIFrame* frame = GetFrame();
  nsIComboboxControlFrame* comboboxFrame = do_QueryFrame(frame);
  if (comboboxFrame) {
    if (aIgnoreIfCollapsed && !comboboxFrame->IsDroppedDown())
      return nsnull;

    frame = comboboxFrame->GetDropDown();
  }

  nsIListControlFrame* listControlFrame = do_QueryFrame(frame);
  if (listControlFrame) {
    nsCOMPtr<nsIContent> activeOptionNode = listControlFrame->GetCurrentOption();
    if (activeOptionNode) {
      nsDocAccessible* document = GetDocAccessible();
      if (document)
        return document->GetAccessible(activeOptionNode);
    }
  }

  return nsnull;
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
nsHTMLComboboxListAccessible::GetFrame() const
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

PRUint64
nsHTMLComboboxListAccessible::NativeState()
{
  // As a nsHTMLComboboxListAccessible we can have the following states:
  // FOCUSED, FOCUSABLE, FLOATING, INVISIBLE
  // Get focus status from base class
  PRUint64 state = nsAccessible::NativeState();

  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = do_QueryFrame(boundsFrame);
  if (comboFrame && comboFrame->IsDroppedDown())
    state |= states::FLOATING;
  else
    state |= states::INVISIBLE;

  return state;
}

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsHTMLComboboxListAccessible::GetBoundsRect(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  *aBoundingFrame = nsnull;

  nsAccessible* comboAcc = Parent();
  if (!comboAcc)
    return;

  if (0 == (comboAcc->State() & states::COLLAPSED)) {
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
