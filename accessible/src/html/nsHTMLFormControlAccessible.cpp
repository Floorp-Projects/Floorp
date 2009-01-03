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

// NOTE: alphabetically ordered
#include "nsAccessibleTreeWalker.h"
#include "nsAccessibilityAtoms.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIEditor.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsISelectionController.h"
#include "jsapi.h"
#include "nsIJSContextStack.h"
#include "nsIServiceManager.h"
#include "nsITextControlFrame.h"

// --- checkbox -----

nsHTMLCheckboxAccessible::nsHTMLCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_CHECKBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {    // 0 is the magic value for default action
    // check or uncheck
    PRUint32 state;
    nsresult rv = GetStateInternal(&state, nsnull);
    NS_ENSURE_SUCCESS(rv, rv);

    if (state & nsIAccessibleStates::STATE_CHECKED)
      aName.AssignLiteral("uncheck"); 
    else
      aName.AssignLiteral("check"); 

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {   // 0 is the magic value for default action
    return DoCommand();
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
nsHTMLCheckboxAccessible::GetStateInternal(PRUint32 *aState,
                                           PRUint32 *aExtraState)
{
  nsresult rv = nsFormControlAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_CHECKABLE;

  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement(do_QueryInterface(mDOMNode));
  if (htmlCheckboxElement)
    htmlCheckboxElement->GetChecked(&checked);

  if (checked)
    *aState |= nsIAccessibleStates::STATE_CHECKED;

  return NS_OK;
}

//------ Radio button -------

nsHTMLRadioButtonAccessible::nsHTMLRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsRadioButtonAccessible(aNode, aShell)
{ 
}

nsresult
nsHTMLRadioButtonAccessible::GetStateInternal(PRUint32 *aState,
                                              PRUint32 *aExtraState)
{
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_CHECKABLE;
  
  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlRadioElement(do_QueryInterface(mDOMNode));
  if (htmlRadioElement)
    htmlRadioElement->GetChecked(&checked);

  if (checked)
    *aState |= nsIAccessibleStates::STATE_CHECKED;

  return NS_OK;
}

nsresult
nsHTMLRadioButtonAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  NS_ENSURE_ARG_POINTER(aAttributes);
  NS_ENSURE_TRUE(mDOMNode, NS_ERROR_FAILURE);

  nsresult rv = nsRadioButtonAccessible::GetAttributesInternal(aAttributes);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString nsURI;
  mDOMNode->GetNamespaceURI(nsURI);
  nsAutoString tagName;
  mDOMNode->GetLocalName(tagName);

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ENSURE_STATE(content);

  nsAutoString type;
  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::type, type);
  nsAutoString name;
  content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::name, name);

  nsCOMPtr<nsIDOMNodeList> inputs;

  nsCOMPtr<nsIDOMHTMLInputElement> radio(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIDOMHTMLFormElement> form;
  radio->GetForm(getter_AddRefs(form));
  if (form) {
    form->GetElementsByTagNameNS(nsURI, tagName, getter_AddRefs(inputs));
  } else {
    nsCOMPtr<nsIDOMDocument> document;
    mDOMNode->GetOwnerDocument(getter_AddRefs(document));
    if (document)
      document->GetElementsByTagNameNS(nsURI, tagName, getter_AddRefs(inputs));
  }

  NS_ENSURE_TRUE(inputs, NS_OK);

  PRUint32 inputsCount = 0;
  inputs->GetLength(&inputsCount);

  // Get posinset and setsize.
  PRInt32 indexOf = 0;
  PRInt32 count = 0;

  for (PRUint32 index = 0; index < inputsCount; index++) {
    nsCOMPtr<nsIDOMNode> itemNode;
    inputs->Item(index, getter_AddRefs(itemNode));

    nsCOMPtr<nsIContent> item(do_QueryInterface(itemNode));
    if (item &&
        item->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                          type, eCaseMatters) &&
        item->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::name,
                          name, eCaseMatters)) {

      count++;

      if (itemNode == mDOMNode)
        indexOf = count;
    }
  }

  nsAccUtils::SetAccGroupAttrs(aAttributes, 0, indexOf, count);

  return  NS_OK;
}

// ----- Button -----

nsHTMLButtonAccessible::nsHTMLButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLButtonAccessible::DoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    return DoCommand();
  }
  return NS_ERROR_INVALID_ARG;
}

nsresult
nsHTMLButtonAccessible::GetStateInternal(PRUint32 *aState,
                                         PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(element, NS_ERROR_FAILURE);

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *aState |= nsIAccessibleStates::STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_PUSHBUTTON;
  return NS_OK;
}

nsresult
nsHTMLButtonAccessible::GetNameInternal(nsAString& aName)
{
  nsresult rv = nsAccessible::GetNameInternal(aName);
  if (!aName.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  // No name from HTML or ARIA
  nsAutoString name;
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value,
                        name) &&
      !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::alt,
                        name)) {
    // Use the button's (default) label if nothing else works
    nsIFrame* frame = GetFrame();
    if (frame) {
      nsIFormControlFrame* fcFrame = nsnull;
      CallQueryInterface(frame, &fcFrame);
      if (fcFrame)
        fcFrame->GetFormProperty(nsAccessibilityAtoms::defaultLabel, name);
    }
  }

  if (name.IsEmpty() &&
      !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::src,
                        name)) {
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::data, name);
  }

  name.CompressWhitespace();
  aName = name;

  return NS_OK;
}


// ----- HTML 4 Button: can contain arbitrary HTML content -----

nsHTML4ButtonAccessible::nsHTML4ButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  if (aIndex == eAction_Click) {
    aName.AssignLiteral("press"); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    return DoCommand();
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_PUSHBUTTON;
  return NS_OK;
}

nsresult
nsHTML4ButtonAccessible::GetStateInternal(PRUint32 *aState,
                                          PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No element for button's dom node!");

  *aState |= nsIAccessibleStates::STATE_FOCUSABLE;

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *aState |= nsIAccessibleStates::STATE_DEFAULT;

  return NS_OK;
}

// --- textfield -----

nsHTMLTextFieldAccessible::nsHTMLTextFieldAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED3(nsHTMLTextFieldAccessible, nsAccessible, nsHyperTextAccessible, nsIAccessibleText, nsIAccessibleEditableText)

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_ENTRY;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content &&
      content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                           nsAccessibilityAtoms::password, eIgnoreCase)) {
    *aRole = nsIAccessibleRole::ROLE_PASSWORD_TEXT;
  }
  return NS_OK;
}

nsresult
nsHTMLTextFieldAccessible::GetNameInternal(nsAString& aName)
{
  nsresult rv = nsAccessible::GetNameInternal(aName);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aName.IsEmpty())
    return NS_OK;

  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  if (!content->GetBindingParent())
    return NS_OK;

  // XXX: bug 459640
  // There's a binding parent.
  // This means we're part of another control, so use parent accessible for name.
  // This ensures that a textbox inside of a XUL widget gets
  // an accessible name.
  nsCOMPtr<nsIAccessible> parent;
  rv = GetParent(getter_AddRefs(parent));
  return parent ? parent->GetName(aName) : rv;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetValue(nsAString& _retval)
{
  PRUint32 state;
  nsresult rv = GetStateInternal(&state, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (state & nsIAccessibleStates::STATE_PROTECTED)    // Don't return password text!
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  if (textArea) {
    return textArea->GetValue(_retval);
  }
  
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));
  if (inputElement) {
    return inputElement->GetValue(_retval);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsHTMLTextFieldAccessible::GetStateInternal(PRUint32 *aState,
                                            PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  // can be focusable, focused, protected. readonly, unavailable, selected
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "Should not have gotten here if upcalled GetExtState() succeeded");

  if (content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                           nsAccessibilityAtoms::password, eIgnoreCase)) {
    *aState |= nsIAccessibleStates::STATE_PROTECTED;
  }
  else {
    nsCOMPtr<nsIAccessible> parent;
    GetParent(getter_AddRefs(parent));
    if (nsAccUtils::Role(parent) == nsIAccessibleRole::ROLE_AUTOCOMPLETE)
      *aState |= nsIAccessibleStates::STATE_HASPOPUP;
  }

  if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::readonly)) {
    *aState |= nsIAccessibleStates::STATE_READONLY;
  }

  if (!aExtraState)
    return NS_OK;

  nsCOMPtr<nsIDOMHTMLInputElement> htmlInput(do_QueryInterface(mDOMNode));
  // Is it an <input> or a <textarea> ?
  if (htmlInput) {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_SINGLE_LINE;
  }
  else {
    *aExtraState |= nsIAccessibleStates::EXT_STATE_MULTI_LINE;
  }

  if (!(*aExtraState & nsIAccessibleStates::EXT_STATE_EDITABLE))
    return NS_OK;

  nsCOMPtr<nsIContent> bindingContent = content->GetBindingParent();
  if (bindingContent &&
      bindingContent->NodeInfo()->Equals(nsAccessibilityAtoms::textbox,
                                         kNameSpaceID_XUL) &&
      bindingContent->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                                  nsAccessibilityAtoms::autocomplete,
                                  eIgnoreCase)) {
    // If parent is XUL textbox and value of @type attribute is "autocomplete",
    // then this accessible supports autocompletion.
    *aExtraState |= nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION;
  } else if (gIsFormFillEnabled && htmlInput &&
             !(*aState & nsIAccessibleStates::STATE_PROTECTED)) {
    // Check to see if autocompletion is allowed on this input. We don't expose
    // it for password fields even though the entire password can be remembered
    // for a page if the user asks it to be. However, the kind of autocomplete
    // we're talking here is based on what the user types, where a popup of
    // possible choices comes up.
    nsAutoString autocomplete;
    content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::autocomplete,
                     autocomplete);

    if (!autocomplete.LowerCaseEqualsLiteral("off")) {
      nsCOMPtr<nsIDOMHTMLFormElement> form;
      htmlInput->GetForm(getter_AddRefs(form));
      nsCOMPtr<nsIContent> formContent(do_QueryInterface(form));
      if (formContent) {
        formContent->GetAttr(kNameSpaceID_None,
                             nsAccessibilityAtoms::autocomplete, autocomplete);
      }

      if (!formContent || !autocomplete.LowerCaseEqualsLiteral("off"))
        *aExtraState |= nsIAccessibleStates::EXT_STATE_SUPPORTS_AUTOCOMPLETION;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
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
    nsCOMPtr<nsIDOMNSHTMLElement> element(do_QueryInterface(mDOMNode));
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
  nsCOMPtr<nsIDOMNSEditableElement> editableElt(do_QueryInterface(mDOMNode));
  NS_ENSURE_TRUE(editableElt, NS_ERROR_FAILURE);

  // nsGenericHTMLElement::GetEditor has a security check.
  // Make sure we're not restricted by the permissions of
  // whatever script is currently running.
  nsCOMPtr<nsIJSContextStack> stack =
    do_GetService("@mozilla.org/js/xpc/ContextStack;1");
  PRBool pushed = stack && NS_SUCCEEDED(stack->Push(nsnull));

  nsCOMPtr<nsIEditor> editor;
  nsresult rv = editableElt->GetEditor(aEditor);

  if (pushed) {
    JSContext* cx;
    stack->Pop(&cx);
    NS_ASSERTION(!cx, "context should be null");
  }

  return rv;
}

// --- groupbox  -----

/*
 * The HTML for this is <fieldset> <legend>box-title</legend> form elements </fieldset> 
 */

nsHTMLGroupboxAccessible::nsHTMLGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_GROUPING;
  return NS_OK;
}

nsIContent* nsHTMLGroupboxAccessible::GetLegend()
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
  NS_ENSURE_TRUE(content, nsnull);

  nsresult count = 0;
  nsIContent *testLegendContent;
  while ((testLegendContent = content->GetChildAt(count ++ )) != nsnull) {
    if (testLegendContent->NodeInfo()->Equals(nsAccessibilityAtoms::legend,
                                              content->GetNameSpaceID())) {
      // Either XHTML namespace or no namespace
      return testLegendContent;
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
    return AppendFlatStringFromSubtree(legendContent, &aName);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLGroupboxAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                               nsIAccessible **aRelated)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  NS_ENSURE_ARG_POINTER(aRelated);

  *aRelated = nsnull;

  nsresult rv = nsHyperTextAccessibleWrap::GetAccessibleRelated(aRelationType, aRelated);
  if (NS_FAILED(rv) || *aRelated) {
    // Either the node is shut down, or another relation mechanism has been used
    return rv;
  }

  if (aRelationType == nsIAccessibleRelation::RELATION_LABELLED_BY) {
    // No override for label, so use <legend> for this <fieldset>
    nsCOMPtr<nsIDOMNode> legendNode = do_QueryInterface(GetLegend());
    if (legendNode) {
      GetAccService()->GetAccessibleInWeakShell(legendNode, mWeakShell, aRelated);
    }
  }

  return NS_OK;
}

nsHTMLLegendAccessible::nsHTMLLegendAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP
nsHTMLLegendAccessible::GetAccessibleRelated(PRUint32 aRelationType,
                                             nsIAccessible **aRelated)
{
  *aRelated = nsnull;

  nsresult rv = nsHyperTextAccessibleWrap::GetAccessibleRelated(aRelationType, aRelated);
  if (NS_FAILED(rv) || *aRelated) {
    // Either the node is shut down, or another relation mechanism has been used
    return rv;
  }

  if (aRelationType == nsIAccessibleRelation::RELATION_LABEL_FOR) {
    // Look for groupbox parent
    nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
    if (!content) {
      return NS_ERROR_FAILURE;  // Node already shut down
    }
    nsCOMPtr<nsIAccessible> groupboxAccessible = GetParent();
    if (nsAccUtils::Role(groupboxAccessible) == nsIAccessibleRole::ROLE_GROUPING) {
      nsCOMPtr<nsIAccessible> testLabelAccessible;
      groupboxAccessible->GetAccessibleRelated(nsIAccessibleRelation::RELATION_LABELLED_BY,
                                               getter_AddRefs(testLabelAccessible));
      if (testLabelAccessible == this) {
        // We're the first child of the parent groupbox
        NS_ADDREF(*aRelated = groupboxAccessible);
      }
    }
  }

  return NS_OK;
}
