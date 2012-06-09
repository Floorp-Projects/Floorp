/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XULFormControlAccessible.h"

#include "Accessible-inl.h"
#include "HTMLFormControlAccessible.h"
#include "nsAccUtils.h"
#include "nsAccTreeWalker.h"
#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "nsIAccessibleRelation.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"
#include "XULMenuAccessible.h"

#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULMenuListElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULTextboxElement.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsITextControlFrame.h"
#include "nsMenuPopupFrame.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULButtonAccessible::
  XULButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  if (ContainsMenu())
    mFlags |= eMenuButtonAccessible;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsISupports

NS_IMPL_ISUPPORTS_INHERITED0(XULButtonAccessible, Accessible)

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: nsIAccessible

PRUint8
XULButtonAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP
XULButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
XULButtonAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Accessible

role
XULButtonAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

PRUint64
XULButtonAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled).

  // get focus and disable status from base class
  PRUint64 state = Accessible::NativeState();

  // Buttons can be checked -- they simply appear pressed in rather than checked
  nsCOMPtr<nsIDOMXULButtonElement> xulButtonElement(do_QueryInterface(mContent));
  if (xulButtonElement) {
    nsAutoString type;
    xulButtonElement->GetType(type);
    if (type.EqualsLiteral("checkbox") || type.EqualsLiteral("radio")) {
      state |= states::CHECKABLE;
      bool checked = false;
      PRInt32 checkState = 0;
      xulButtonElement->GetChecked(&checked);
      if (checked) {
        state |= states::PRESSED;
        xulButtonElement->GetCheckState(&checkState);
        if (checkState == nsIDOMXULButtonElement::CHECKSTATE_MIXED) { 
          state |= states::MIXED;
        }
      }
    }
  }

  if (ContainsMenu())
    state |= states::HASPOPUP;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::_default))
    state |= states::DEFAULT;

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Widgets

bool
XULButtonAccessible::IsWidget() const
{
  return true;
}

bool
XULButtonAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULButtonAccessible::AreItemsOperable() const
{
  if (IsMenuButton()) {
    Accessible* menuPopup = mChildren.SafeElementAt(0, nsnull);
    if (menuPopup) {
      nsMenuPopupFrame* menuPopupFrame = do_QueryFrame(menuPopup->GetFrame());
      return menuPopupFrame->IsOpen();
    }
  }
  return false; // no items
}

Accessible*
XULButtonAccessible::ContainerWidget() const
{
  if (IsMenuButton() && mParent && mParent->IsAutoComplete())
    return mParent;
  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible: Accessible protected

void
XULButtonAccessible::CacheChildren()
{
  // In general XUL button has not accessible children. Nevertheless menu
  // buttons can have button (@type="menu-button") and popup accessibles
  // (@type="menu-button" or @type="menu").

  // XXX: no children until the button is menu button. Probably it's not
  // totally correct but in general AT wants to have leaf buttons.
  bool isMenu = mContent->AttrValueIs(kNameSpaceID_None,
                                       nsGkAtoms::type,
                                       nsGkAtoms::menu,
                                       eCaseMatters);

  bool isMenuButton = isMenu ?
    false :
    mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                          nsGkAtoms::menuButton, eCaseMatters);

  NS_ENSURE_TRUE(mDoc,);
  if (!isMenu && !isMenuButton)
    return;

  Accessible* menupopup = nsnull;
  Accessible* button = nsnull;

  nsAccTreeWalker walker(mDoc, mContent, true);

  Accessible* child = nsnull;
  while ((child = walker.NextChild())) {
    roles::Role role = child->Role();

    if (role == roles::MENUPOPUP) {
      // Get an accessible for menupopup or panel elements.
      menupopup = child;

    } else if (isMenuButton && role == roles::PUSHBUTTON) {
      // Button type="menu-button" contains a real button. Get an accessible
      // for it. Ignore dropmarker button which is placed as a last child.
      button = child;
      break;

    } else {
      // Unbind rejected accessible from document.
      Document()->UnbindFromDocument(child);
    }
  }

  if (!menupopup)
    return;

  AppendChild(menupopup);
  if (button)
    AppendChild(button);
}

////////////////////////////////////////////////////////////////////////////////
// XULButtonAccessible protected

bool
XULButtonAccessible::ContainsMenu()
{
  static nsIContent::AttrValuesArray strings[] =
    {&nsGkAtoms::menu, &nsGkAtoms::menuButton, nsnull};

  return mContent->FindAttrValueIn(kNameSpaceID_None,
                                   nsGkAtoms::type,
                                   strings, eCaseMatters) >= 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULDropmarkerAccessible
////////////////////////////////////////////////////////////////////////////////

XULDropmarkerAccessible::
  XULDropmarkerAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

PRUint8
XULDropmarkerAccessible::ActionCount()
{
  return 1;
}

bool
XULDropmarkerAccessible::DropmarkerOpen(bool aToggleOpen)
{
  bool isOpen = false;

  nsCOMPtr<nsIDOMXULButtonElement> parentButtonElement =
    do_QueryInterface(mContent->GetParent());

  if (parentButtonElement) {
    parentButtonElement->GetOpen(&isOpen);
    if (aToggleOpen)
      parentButtonElement->SetOpen(!isOpen);
  }
  else {
    nsCOMPtr<nsIDOMXULMenuListElement> parentMenuListElement =
      do_QueryInterface(parentButtonElement);
    if (parentMenuListElement) {
      parentMenuListElement->GetOpen(&isOpen);
      if (aToggleOpen)
        parentMenuListElement->SetOpen(!isOpen);
    }
  }

  return isOpen;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP
XULDropmarkerAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    if (DropmarkerOpen(false))
      aName.AssignLiteral("close");
    else
      aName.AssignLiteral("open");
    return NS_OK;
  }

  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the Dropmarker to do its action
  */
NS_IMETHODIMP
XULDropmarkerAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    DropmarkerOpen(true); // Reverse the open attribute
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

role
XULDropmarkerAccessible::NativeRole()
{
  return roles::PUSHBUTTON;
}

PRUint64
XULDropmarkerAccessible::NativeState()
{
  return DropmarkerOpen(false) ? states::PRESSED : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULCheckboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULCheckboxAccessible::
  XULCheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULCheckboxAccessible::NativeRole()
{
  return roles::CHECKBUTTON;
}

PRUint8
XULCheckboxAccessible::ActionCount()
{
  return 1;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP
XULCheckboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    // check or uncheck

    if (NativeState() & states::CHECKED)
      aName.AssignLiteral("uncheck");
    else
      aName.AssignLiteral("check");

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the checkbox to do its only action -- check( or uncheck) itself
  */
NS_IMETHODIMP
XULCheckboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRUint64
XULCheckboxAccessible::NativeState()
{
  // Possible states: focused, focusable, unavailable(disabled), checked
  // Get focus and disable status from base class
  PRUint64 state = LeafAccessible::NativeState();
  
  state |= states::CHECKABLE;
  
  // Determine Checked state
  nsCOMPtr<nsIDOMXULCheckboxElement> xulCheckboxElement =
    do_QueryInterface(mContent);
  if (xulCheckboxElement) {
    bool checked = false;
    xulCheckboxElement->GetChecked(&checked);
    if (checked) {
      state |= states::CHECKED;
      PRInt32 checkState = 0;
      xulCheckboxElement->GetCheckState(&checkState);
      if (checkState == nsIDOMXULCheckboxElement::CHECKSTATE_MIXED)
        state |= states::MIXED;
    }
  }

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// XULGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

XULGroupboxAccessible::
  XULGroupboxAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULGroupboxAccessible::NativeRole()
{
  return roles::GROUPING;
}

nsresult
XULGroupboxAccessible::GetNameInternal(nsAString& aName)
{
  // XXX: we use the first related accessible only.
  Accessible* label =
    RelationByType(nsIAccessibleRelation::RELATION_LABELLED_BY).Next();
  if (label)
    return label->GetName(aName);

  return NS_OK;
}

Relation
XULGroupboxAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = AccessibleWrap::RelationByType(aType);
  if (aType != nsIAccessibleRelation::RELATION_LABELLED_BY)
    return rel;

  // The label for xul:groupbox is generated from xul:label that is
  // inside the anonymous content of the xul:caption.
  // The xul:label has an accessible object but the xul:caption does not
  PRUint32 childCount = ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* childAcc = GetChildAt(childIdx);
    if (childAcc->Role() == roles::LABEL) {
      // Ensure that it's our label
      Relation reverseRel =
        childAcc->RelationByType(nsIAccessibleRelation::RELATION_LABEL_FOR);
      Accessible* testGroupbox = nsnull;
      while ((testGroupbox = reverseRel.Next()))
        if (testGroupbox == this) {
          // The <label> points back to this groupbox
          rel.AppendTarget(childAcc);
        }
    }
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULRadioButtonAccessible::
  XULRadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  RadioButtonAccessible(aContent, aDoc)
{
}

PRUint64
XULRadioButtonAccessible::NativeState()
{
  PRUint64 state = LeafAccessible::NativeState();
  state |= states::CHECKABLE;

  nsCOMPtr<nsIDOMXULSelectControlItemElement> radioButton =
    do_QueryInterface(mContent);
  if (radioButton) {
    bool selected = false;   // Radio buttons can be selected
    radioButton->GetSelected(&selected);
    if (selected) {
      state |= states::CHECKED;
    }
  }

  return state;
}

PRUint64
XULRadioButtonAccessible::NativeInteractiveState() const
{
  return NativelyUnavailable() ? states::UNAVAILABLE : states::FOCUSABLE;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioButtonAccessible: Widgets

Accessible*
XULRadioButtonAccessible::ContainerWidget() const
{
  return mParent;
}


////////////////////////////////////////////////////////////////////////////////
// XULRadioGroupAccessible
////////////////////////////////////////////////////////////////////////////////

/**
  * XUL Radio Group
  *   The Radio Group proxies for the Radio Buttons themselves. The Group gets
  *   focus whereas the Buttons do not. So we only have an accessible object for
  *   this for the purpose of getting the proper RadioButton. Need this here to 
  *   avoid circular reference problems when navigating the accessible tree and
  *   for getting to the radiobuttons.
  */

XULRadioGroupAccessible::
  XULRadioGroupAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULSelectControlAccessible(aContent, aDoc)
{ 
}

role
XULRadioGroupAccessible::NativeRole()
{
  return roles::GROUPING;
}

PRUint64
XULRadioGroupAccessible::NativeInteractiveState() const
{
  // The radio group is not focusable. Sometimes the focus controller will
  // report that it is focused. That means that the actual selected radio button
  // should be considered focused.
  return NativelyUnavailable() ? states::UNAVAILABLE : 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULRadioGroupAccessible: Widgets

bool
XULRadioGroupAccessible::IsWidget() const
{
  return true;
}

bool
XULRadioGroupAccessible::IsActiveWidget() const
{
  return FocusMgr()->HasDOMFocus(mContent);
}

bool
XULRadioGroupAccessible::AreItemsOperable() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// XULStatusBarAccessible
////////////////////////////////////////////////////////////////////////////////

XULStatusBarAccessible::
  XULStatusBarAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULStatusBarAccessible::NativeRole()
{
  return roles::STATUSBAR;
}


////////////////////////////////////////////////////////////////////////////////
// XULToolbarButtonAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarButtonAccessible::
  XULToolbarButtonAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  XULButtonAccessible(aContent, aDoc)
{
}

void
XULToolbarButtonAccessible::GetPositionAndSizeInternal(PRInt32* aPosInSet,
                                                       PRInt32* aSetSize)
{
  PRInt32 setSize = 0;
  PRInt32 posInSet = 0;

  Accessible* parent = Parent();
  if (!parent)
    return;

  PRUint32 childCount = parent->ChildCount();
  for (PRUint32 childIdx = 0; childIdx < childCount; childIdx++) {
    Accessible* child = parent->GetChildAt(childIdx);
    if (IsSeparator(child)) { // end of a group of buttons
      if (posInSet)
        break; // we've found our group, so we're done

      setSize = 0; // not our group, so start a new group

    } else {
      setSize++; // another button in the group

      if (child == this)
        posInSet = setSize; // we've found our button
    }
  }

  *aPosInSet = posInSet;
  *aSetSize = setSize;
}

bool
XULToolbarButtonAccessible::IsSeparator(Accessible* aAccessible)
{
  nsIContent* content = aAccessible->GetContent();
  return content && ((content->Tag() == nsGkAtoms::toolbarseparator) ||
                     (content->Tag() == nsGkAtoms::toolbarspacer) ||
                     (content->Tag() == nsGkAtoms::toolbarspring)); }


////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarAccessible::
  XULToolbarAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
}

role
XULToolbarAccessible::NativeRole()
{
  return roles::TOOLBAR;
}

nsresult
XULToolbarAccessible::GetNameInternal(nsAString& aName)
{
  nsAutoString name;
  if (mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::toolbarname, name)) {
    name.CompressWhitespace();
    aName = name;
  }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////////////
// XULToolbarAccessible
////////////////////////////////////////////////////////////////////////////////

XULToolbarSeparatorAccessible::
  XULToolbarSeparatorAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  LeafAccessible(aContent, aDoc)
{
}

role
XULToolbarSeparatorAccessible::NativeRole()
{
  return roles::SEPARATOR;
}

PRUint64
XULToolbarSeparatorAccessible::NativeState()
{
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// XULTextFieldAccessible
////////////////////////////////////////////////////////////////////////////////

XULTextFieldAccessible::
 XULTextFieldAccessible(nsIContent* aContent, DocAccessible* aDoc) :
 HyperTextAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(XULTextFieldAccessible,
                             Accessible,
                             HyperTextAccessible,
                             nsIAccessibleText,
                             nsIAccessibleEditableText)

////////////////////////////////////////////////////////////////////////////////
// XULTextFieldAccessible: nsIAccessible

void
XULTextFieldAccessible::Value(nsString& aValue)
{
  aValue.Truncate();
  if (NativeRole() == roles::PASSWORD_TEXT) // Don't return password text!
    return;

  nsCOMPtr<nsIDOMXULTextBoxElement> textBox(do_QueryInterface(mContent));
  if (textBox) {
    textBox->GetValue(aValue);
    return;
  }

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList)
    menuList->GetLabel(aValue);
}

void
XULTextFieldAccessible::ApplyARIAState(PRUint64* aState) const
{
  HyperTextAccessibleWrap::ApplyARIAState(aState);

  aria::MapToState(aria::eARIAAutoComplete, mContent->AsElement(), aState);
}

PRUint64
XULTextFieldAccessible::NativeState()
{
  PRUint64 state = HyperTextAccessibleWrap::NativeState();

  nsCOMPtr<nsIContent> inputField(GetInputField());
  NS_ENSURE_TRUE(inputField, state);

  // Create a temporary accessible from the HTML text field to get
  // the accessible state from. Doesn't add to cache into document cache.
  nsRefPtr<HTMLTextFieldAccessible> tempAccessible =
    new HTMLTextFieldAccessible(inputField, mDoc);
  if (!tempAccessible)
    return state;

  state |= tempAccessible->NativeState();

  nsCOMPtr<nsIDOMXULMenuListElement> menuList(do_QueryInterface(mContent));
  if (menuList) {
    // <xul:menulist droppable="false">
    if (!mContent->AttrValueIs(kNameSpaceID_None,
                               nsGkAtoms::editable,
                               nsGkAtoms::_true, eIgnoreCase)) {
      state |= states::READONLY;
    }
  }

  return state;
}

role
XULTextFieldAccessible::NativeRole()
{
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::password, eIgnoreCase))
    return roles::PASSWORD_TEXT;
  
  return roles::ENTRY;
}

/**
  * Only one actions available
  */
PRUint8
XULTextFieldAccessible::ActionCount()
{
  return 1;
}

/**
  * Return the name of our only action
  */
NS_IMETHODIMP
XULTextFieldAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("activate"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

/**
  * Tell the button to do its action
  */
NS_IMETHODIMP
XULTextFieldAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMXULElement> element(do_QueryInterface(mContent));
    if (element)
    {
      element->Focus();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

bool
XULTextFieldAccessible::CanHaveAnonChildren()
{
  return false;
}

already_AddRefed<nsIEditor>
XULTextFieldAccessible::GetEditor() const
{
  nsCOMPtr<nsIContent> inputField = GetInputField();
  nsCOMPtr<nsIDOMNSEditableElement> editableElt(do_QueryInterface(inputField));
  if (!editableElt)
    return nsnull;

  nsCOMPtr<nsIEditor> editor;
  editableElt->GetEditor(getter_AddRefs(editor));
  return editor.forget();
}

////////////////////////////////////////////////////////////////////////////////
// XULTextFieldAccessible: Accessible protected

void
XULTextFieldAccessible::CacheChildren()
{
  NS_ENSURE_TRUE(mDoc,);
  // Create child accessibles for native anonymous content of underlying HTML
  // input element.
  nsCOMPtr<nsIContent> inputContent(GetInputField());
  if (!inputContent)
    return;

  nsAccTreeWalker walker(mDoc, inputContent, false);

  Accessible* child = nsnull;
  while ((child = walker.NextChild()) && AppendChild(child));
}

////////////////////////////////////////////////////////////////////////////////
// XULTextFieldAccessible: HyperTextAccessible protected

already_AddRefed<nsFrameSelection>
XULTextFieldAccessible::FrameSelection()
{
  nsCOMPtr<nsIContent> inputContent(GetInputField());
  nsIFrame* frame = inputContent->GetPrimaryFrame();
  return frame ? frame->GetFrameSelection() : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// XULTextFieldAccessible protected

already_AddRefed<nsIContent>
XULTextFieldAccessible::GetInputField() const
{
  nsCOMPtr<nsIDOMNode> inputFieldDOMNode;
  nsCOMPtr<nsIDOMXULTextBoxElement> textBox = do_QueryInterface(mContent);
  if (textBox) {
    textBox->GetInputField(getter_AddRefs(inputFieldDOMNode));

  } else {
    // <xul:menulist droppable="false">
    nsCOMPtr<nsIDOMXULMenuListElement> menuList = do_QueryInterface(mContent);
    if (menuList)
      menuList->GetInputField(getter_AddRefs(inputFieldDOMNode));
  }

  NS_ASSERTION(inputFieldDOMNode, "No input field for XULTextFieldAccessible");

  nsIContent* inputField = nsnull;
  if (inputFieldDOMNode)
    CallQueryInterface(inputFieldDOMNode, &inputField);

  return inputField;
}
