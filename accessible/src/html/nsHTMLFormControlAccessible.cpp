/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Author: Eric Vaughan (evaughan@netscape.com)
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

#include "nsHTMLFormControlAccessible.h"

#include "Relation.h"
#include "States.h"
#include "nsAccUtils.h"
#include "nsTextEquivUtils.h"

#include "nsIAccessibleRelation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMNodeList.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsISelectionController.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"
#include "nsIServiceManager.h"
#include "nsITextControlFrame.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLCheckboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLCheckboxAccessible::
  nsHTMLCheckboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsFormControlAccessible(aContent, aShell)
{
}

PRUint32
nsHTMLCheckboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_CHECKBUTTON;
}

PRUint8
nsHTMLCheckboxAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {    // 0 is the magic value for default action
    // cycle, check or uncheck
    PRUint64 state = NativeState();

    if (state & states::CHECKED)
      aName.AssignLiteral("uncheck"); 
    else if (state & states::MIXED)
      aName.AssignLiteral("cycle"); 
    else
      aName.AssignLiteral("check"); 

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsHTMLCheckboxAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRUint64
nsHTMLCheckboxAccessible::NativeState()
{
  PRUint64 state = nsFormControlAccessible::NativeState();

  state |= states::CHECKABLE;
  bool checkState = false;   // Radio buttons and check boxes can be checked or mixed

  nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement =
    do_QueryInterface(mContent);
           
  if (htmlCheckboxElement) {
    htmlCheckboxElement->GetIndeterminate(&checkState);

    if (checkState) {
      state |= states::MIXED;
    } else {   // indeterminate can't be checked at the same time.
      htmlCheckboxElement->GetChecked(&checkState);
    
      if (checkState)
        state |= states::CHECKED;
    }
  }
  return state;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLCheckboxAccessible: Widgets

bool
nsHTMLCheckboxAccessible::IsWidget() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLRadioButtonAccessible::
  nsHTMLRadioButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsRadioButtonAccessible(aContent, aShell)
{
}

PRUint64
nsHTMLRadioButtonAccessible::NativeState()
{
  PRUint64 state = nsAccessibleWrap::NativeState();

  state |= states::CHECKABLE;
  
  bool checked = false;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlRadioElement =
    do_QueryInterface(mContent);
  if (htmlRadioElement)
    htmlRadioElement->GetChecked(&checked);

  if (checked)
    state |= states::CHECKED;

  return state;
}

void
nsHTMLRadioButtonAccessible::GetPositionAndSizeInternal(PRInt32 *aPosInSet,
                                                        PRInt32 *aSetSize)
{
  nsAutoString nsURI;
  mContent->NodeInfo()->GetNamespaceURI(nsURI);
  nsAutoString tagName;
  mContent->NodeInfo()->GetName(tagName);

  nsAutoString type;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
  nsAutoString name;
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  nsCOMPtr<nsIDOMNodeList> inputs;

  nsCOMPtr<nsIDOMHTMLInputElement> radio(do_QueryInterface(mContent));
  nsCOMPtr<nsIDOMHTMLFormElement> form;
  radio->GetForm(getter_AddRefs(form));
  if (form) {
    form->GetElementsByTagNameNS(nsURI, tagName, getter_AddRefs(inputs));
  } else {
    nsIDocument* doc = mContent->GetOwnerDoc();
    nsCOMPtr<nsIDOMDocument> document(do_QueryInterface(doc));
    if (document)
      document->GetElementsByTagNameNS(nsURI, tagName, getter_AddRefs(inputs));
  }

  NS_ENSURE_TRUE(inputs, );

  PRUint32 inputsCount = 0;
  inputs->GetLength(&inputsCount);

  // Compute posinset and setsize.
  PRInt32 indexOf = 0;
  PRInt32 count = 0;

  for (PRUint32 index = 0; index < inputsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    inputs->Item(index, getter_AddRefs(itemNode));

    nsCOMPtr<nsIContent> item(do_QueryInterface(itemNode));
    if (item &&
        item->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                          type, eCaseMatters) &&
        item->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                          name, eCaseMatters)) {

      count++;

      if (item == mContent)
        indexOf = count;
    }
  }

  *aPosInSet = indexOf;
  *aSetSize = count;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLButtonAccessible::
  nsHTMLButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

PRUint8
nsHTMLButtonAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsHTMLButtonAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Click)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRUint64
nsHTMLButtonAccessible::NativeState()
{
  PRUint64 state = nsHyperTextAccessibleWrap::NativeState();

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::submit, eIgnoreCase))
    state |= states::DEFAULT;

  return state;
}

PRUint32
nsHTMLButtonAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

nsresult
nsHTMLButtonAccessible::GetNameInternal(nsAString& aName)
{
  nsAccessible::GetNameInternal(aName);
  if (!aName.IsEmpty())
    return NS_OK;

  // No name from HTML or ARIA
  nsAutoString name;
  if (!mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                         name) &&
      !mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::alt,
                         name)) {
    // Use the button's (default) label if nothing else works
    nsIFrame* frame = GetFrame();
    if (frame) {
      nsIFormControlFrame* fcFrame = do_QueryFrame(frame);
      if (fcFrame)
        fcFrame->GetFormProperty(nsGkAtoms::defaultLabel, name);
    }
  }

  if (name.IsEmpty() &&
      !mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::src, name)) {
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::data, name);
  }

  name.CompressWhitespace();
  aName = name;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLButtonAccessible: Widgets

bool
nsHTMLButtonAccessible::IsWidget() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTML4ButtonAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTML4ButtonAccessible::
  nsHTML4ButtonAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

PRUint8
nsHTML4ButtonAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsHTML4ButtonAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != 0)
    return NS_ERROR_INVALID_ARG;

  DoCommand();
  return NS_OK;
}

PRUint32
nsHTML4ButtonAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_PUSHBUTTON;
}

PRUint64
nsHTML4ButtonAccessible::NativeState()
{
  PRUint64 state = nsHyperTextAccessibleWrap::NativeState();

  state |= states::FOCUSABLE;

  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::submit, eIgnoreCase))
    state |= states::DEFAULT;

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTML4ButtonAccessible: Widgets

bool
nsHTML4ButtonAccessible::IsWidget() const
{
  return true;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLTextFieldAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLTextFieldAccessible::
  nsHTMLTextFieldAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsHTMLTextFieldAccessible, nsAccessible, nsHyperTextAccessible, nsIAccessibleText, nsIAccessibleEditableText)

PRUint32
nsHTMLTextFieldAccessible::NativeRole()
{
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::password, eIgnoreCase)) {
    return nsIAccessibleRole::ROLE_PASSWORD_TEXT;
  }
  return nsIAccessibleRole::ROLE_ENTRY;
}

nsresult
nsHTMLTextFieldAccessible::GetNameInternal(nsAString& aName)
{
  nsresult rv = nsAccessible::GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aName.IsEmpty())
    return NS_OK;

  if (mContent->GetBindingParent())
  {
    // XXX: bug 459640
    // There's a binding parent.
    // This means we're part of another control, so use parent accessible for name.
    // This ensures that a textbox inside of a XUL widget gets
    // an accessible name.
    nsAccessible* parent = Parent();
    if (parent)
      parent->GetName(aName);
  }

  if (!aName.IsEmpty())
    return NS_OK;

  // text inputs and textareas might have useful placeholder text
  mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder, aName);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetValue(nsAString& _retval)
{
  if (IsDefunct())
    return NS_ERROR_FAILURE;

  if (NativeState() & states::PROTECTED)    // Don't return password text!
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mContent));
  if (textArea) {
    return textArea->GetValue(_retval);
  }
  
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mContent));
  if (inputElement) {
    return inputElement->GetValue(_retval);
  }

  return NS_ERROR_FAILURE;
}

void
nsHTMLTextFieldAccessible::ApplyARIAState(PRUint64* aState)
{
  nsHyperTextAccessibleWrap::ApplyARIAState(aState);

  nsStateMapEntry::MapToStates(mContent, aState, eARIAAutoComplete);

}

PRUint64
nsHTMLTextFieldAccessible::NativeState()
{
  PRUint64 state = nsHyperTextAccessibleWrap::NativeState();

  // can be focusable, focused, protected. readonly, unavailable, selected
  if (mContent->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                            nsGkAtoms::password, eIgnoreCase)) {
    state |= states::PROTECTED;
  }

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly)) {
    state |= states::READONLY;
  }

  // Is it an <input> or a <textarea> ?
  nsCOMPtr<nsIDOMHTMLInputElement> htmlInput(do_QueryInterface(mContent));
  state |= htmlInput ? states::SINGLE_LINE : states::MULTI_LINE;

  if (!(state & states::EDITABLE) ||
      (state & (states::PROTECTED | states::MULTI_LINE)))
    return state;

  // Expose autocomplete states if this input is part of autocomplete widget.
  nsAccessible* widget = ContainerWidget();
  if (widget && widget-IsAutoComplete()) {
    state |= states::HASPOPUP | states::SUPPORTS_AUTOCOMPLETION;
    return state;
  }

  // No parent can mean a fake widget created for XUL textbox. If accessible
  // is unattached from tree then we don't care.
  if (mParent && gIsFormFillEnabled) {
    // Check to see if autocompletion is allowed on this input. We don't expose
    // it for password fields even though the entire password can be remembered
    // for a page if the user asks it to be. However, the kind of autocomplete
    // we're talking here is based on what the user types, where a popup of
    // possible choices comes up.
    nsAutoString autocomplete;
    mContent->GetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete,
                      autocomplete);

    if (!autocomplete.LowerCaseEqualsLiteral("off")) {
      nsCOMPtr<nsIDOMHTMLFormElement> form;
      htmlInput->GetForm(getter_AddRefs(form));
      nsCOMPtr<nsIContent> formContent(do_QueryInterface(form));
      if (formContent) {
        formContent->GetAttr(kNameSpaceID_None,
                             nsGkAtoms::autocomplete, autocomplete);
      }

      if (!formContent || !autocomplete.LowerCaseEqualsLiteral("off"))
        state |= states::SUPPORTS_AUTOCOMPLETION;
    }
  }

  return state;
}

PRUint8
nsHTMLTextFieldAccessible::ActionCount()
{
  return 1;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("activate");
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMHTMLElement> element(do_QueryInterface(mContent));
    if ( element ) {
      return element->Focus();
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetAssociatedEditor(nsIEditor **aEditor)
{
  *aEditor = nsnull;
  nsCOMPtr<nsIDOMNSEditableElement> editableElt(do_QueryInterface(mContent));
  NS_ENSURE_TRUE(editableElt, NS_ERROR_FAILURE);

  // nsGenericHTMLElement::GetEditor has a security check.
  // Make sure we're not restricted by the permissions of
  // whatever script is currently running.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  bool pushed = stack && NS_SUCCEEDED(stack->Push(nsnull));

  nsCOMPtr<nsIEditor> editor;
  nsresult rv = editableElt->GetEditor(aEditor);

  if (pushed) {
    JSContext* cx;
    stack->Pop(&cx);
    NS_ASSERTION(!cx, "context should be null");
  }

  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLTextFieldAccessible: Widgets

bool
nsHTMLTextFieldAccessible::IsWidget() const
{
  return true;
}

nsAccessible*
nsHTMLTextFieldAccessible::ContainerWidget() const
{
  return mParent && mParent->Role() == nsIAccessibleRole::ROLE_AUTOCOMPLETE ?
    mParent : nsnull;
}


////////////////////////////////////////////////////////////////////////////////
// nsHTMLGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLGroupboxAccessible::
  nsHTMLGroupboxAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

PRUint32
nsHTMLGroupboxAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_GROUPING;
}

nsIContent* nsHTMLGroupboxAccessible::GetLegend()
{
  nsresult count = 0;
  nsIContent *legendContent = nsnull;
  while ((legendContent = mContent->GetChildAt(count++)) != nsnull) {
    if (legendContent->NodeInfo()->Equals(nsGkAtoms::legend,
                                          mContent->GetNameSpaceID())) {
      // Either XHTML namespace or no namespace
      return legendContent;
    }
  }

  return nsnull;
}

nsresult
nsHTMLGroupboxAccessible::GetNameInternal(nsAString& aName)
{
  nsresult rv = nsAccessible::GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aName.IsEmpty())
    return NS_OK;

  nsIContent *legendContent = GetLegend();
  if (legendContent) {
    return nsTextEquivUtils::
      AppendTextEquivFromContent(this, legendContent, &aName);
  }

  return NS_OK;
}

Relation
nsHTMLGroupboxAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsHyperTextAccessibleWrap::RelationByType(aType);
    // No override for label, so use <legend> for this <fieldset>
  if (aType == nsIAccessibleRelation::RELATION_LABELLED_BY)
    rel.AppendTarget(GetLegend());

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLegendAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLegendAccessible::
  nsHTMLLegendAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

Relation
nsHTMLLegendAccessible::RelationByType(PRUint32 aType)
{
  Relation rel = nsHyperTextAccessibleWrap::RelationByType(aType);
  if (aType != nsIAccessibleRelation::RELATION_LABEL_FOR)
    return rel;

  nsAccessible* groupbox = Parent();
  if (groupbox && groupbox->Role() == nsIAccessibleRole::ROLE_GROUPING)
    rel.AppendTarget(groupbox);

  return rel;
}

PRUint32
nsHTMLLegendAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LABEL;
}
