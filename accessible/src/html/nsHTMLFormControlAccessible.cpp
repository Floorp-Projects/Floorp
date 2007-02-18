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
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSEditableElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLLegendElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsISelectionController.h"
#include "nsITextControlFrame.h"

// --- checkbox -----

nsHTMLCheckboxAccessible::nsHTMLCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_CHECKBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {    // 0 is the magic value for default action
    // check or uncheck
    PRUint32 state;
    GetState(&state);

    if (state & STATE_CHECKED)
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("uncheck"), _retval); 
    else
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("check"), _retval); 

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

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetState(_retval);
  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement(do_QueryInterface(mDOMNode));
  if (htmlCheckboxElement) 
    htmlCheckboxElement->GetChecked(&checked);

  if (checked) 
    *_retval |= STATE_CHECKED;
  
  return NS_OK;
}

//------ Radio button -------

nsHTMLRadioButtonAccessible::nsHTMLRadioButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsRadioButtonAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLRadioButtonAccessible::GetState(PRUint32 *_retval)
{
  nsAccessibleWrap::GetState(_retval);
  PRBool checked = PR_FALSE;   // Radio buttons and check boxes can be checked

  nsCOMPtr<nsIDOMHTMLInputElement> htmlRadioElement(do_QueryInterface(mDOMNode));
  if (htmlRadioElement) 
    htmlRadioElement->GetChecked(&checked);

  if (checked) 
    *_retval |= STATE_CHECKED;

  return NS_OK;
}

// ----- Button -----

nsHTMLButtonAccessible::nsHTMLButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
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

NS_IMETHODIMP nsHTMLButtonAccessible::GetState(PRUint32 *_retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (!element) {
    return NS_ERROR_FAILURE;  // Button accessible shut down
  }
  nsHyperTextAccessible::GetState(_retval);
  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetName(nsAString& aName)
{
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE; // Node shut down
  }

  nsAutoString name;
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value,
                        name) &&
      !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::alt,
                        name)) {
    if (mRoleMapEntry) {
      // Use HTML label or DHTML accessibility's labelledby attribute for name
      GetHTMLName(name, PR_FALSE);
    }
    if (name.IsEmpty()) {
      // Use the button's (default) label if nothing else works
      nsIFrame* frame = GetFrame();
      if (frame) {
        nsIFormControlFrame* fcFrame;
        frame->QueryInterface(NS_GET_IID(nsIFormControlFrame),
                              (void**) &fcFrame);
        if (fcFrame)
          fcFrame->GetFormProperty(nsAccessibilityAtoms::defaultLabel, name);
      }
    }
    if (name.IsEmpty() &&
        !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::title,
                          name) &&
        !content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::src,
                          name)) {
      content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::data, name);
    }
  }

  name.CompressWhitespace();
  aName = name;

  return NS_OK;
}


// ----- HTML 4 Button: can contain arbitrary HTML content -----

nsHTML4ButtonAccessible::nsHTML4ButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
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
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetState(PRUint32 *_retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (!element) {
    return NS_ERROR_FAILURE;  // Button accessible shut down
  }
  nsHyperTextAccessible::GetState(_retval);
  *_retval |= STATE_FOCUSABLE;

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.LowerCaseEqualsLiteral("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

// --- textfield -----

nsHTMLTextFieldAccessible::nsHTMLTextFieldAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED2(nsHTMLTextFieldAccessible, nsAccessible, nsIAccessibleText, nsIAccessibleEditableText)

NS_IMETHODIMP nsHTMLTextFieldAccessible::Init()
{
  CheckForEditor();
  return nsHyperTextAccessible::Init();
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::Shutdown()
{
  if (mEditor) {
    mEditor->RemoveEditActionListener(this);
    mEditor = nsnull;
  }
  return nsHyperTextAccessible::Shutdown();
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_ENTRY;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (content &&
      content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                           nsAccessibilityAtoms::password, eIgnoreCase)) {
    *aRole = ROLE_PASSWORD_TEXT;
  }
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetValue(nsAString& _retval)
{
  PRUint32 state;
  GetState(&state);
  if (state & STATE_PROTECTED)    // Don't return password text!
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

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetState(PRUint32 *aState)
{
  nsresult rv = nsHyperTextAccessible::GetState(aState);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // can be focusable, focused, protected. readonly, unavailable, selected
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "Should not have gotten here if upcalled GetExtState() succeeded");

  if (content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                           nsAccessibilityAtoms::password, eIgnoreCase)) {
    *aState |= STATE_PROTECTED;
  }
  else {
    nsCOMPtr<nsIAccessible> parent;
    GetParent(getter_AddRefs(parent));
    if (parent && Role(parent) == ROLE_AUTOCOMPLETE) {
      *aState |= STATE_HASPOPUP;
    }
  }

  if (content->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::readonly)) {
    *aState |= STATE_READONLY;
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetExtState(PRUint32 *aExtState)
{
  nsresult rv = nsHyperTextAccessible::GetExtState(aExtState);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIDOMHTMLInputElement> htmlInput(do_QueryInterface(mDOMNode, &rv));
  // Is it an <input> or a <textarea> ?
  *aExtState |= htmlInput ? EXT_STATE_SINGLE_LINE : EXT_STATE_MULTI_LINE;

  PRUint32 state;
  GetState(&state);
  const PRUint32 kNonEditableStates = STATE_READONLY | STATE_UNAVAILABLE;
  if (0 == (state & kNonEditableStates)) {
    *aExtState |= EXT_STATE_EDITABLE;
    nsCOMPtr<nsIContent> content = do_QueryInterface(mDOMNode);
    if (content && (content = content->GetBindingParent()) != nsnull &&
        content->NodeInfo()->Equals(nsAccessibilityAtoms::textbox, kNameSpaceID_XUL)) {
      // If parent is XUL textbox, then it supports autocompletion if type="autocomplete"
      if (content->AttrValueIs(kNameSpaceID_None, nsAccessibilityAtoms::type,
                               NS_LITERAL_STRING("autocomplete"), eIgnoreCase)) {
        *aExtState |= EXT_STATE_SUPPORTS_AUTOCOMPLETION;
      }
    }
    else if (gIsFormFillEnabled && htmlInput && !(state & STATE_PROTECTED)) {
      // Check to see if autocompletion is allowed on this input
      // We don't expose it for password fields even though the entire password can
      // be remembered for a page if the user asks it to be.
      // However, the kind of autocomplete we're talking here is based on what
      // the user types, where a popup of possible choices comes up.
      nsAutoString autocomplete;
      htmlInput->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
      if (!autocomplete.LowerCaseEqualsLiteral("off")) {
        nsCOMPtr<nsIDOMHTMLFormElement> form;
        htmlInput->GetForm(getter_AddRefs(form));
        if (form)
          form->GetAttribute(NS_LITERAL_STRING("autocomplete"), autocomplete);
        if (!form || !autocomplete.LowerCaseEqualsLiteral("off")) {
          *aExtState |= EXT_STATE_SUPPORTS_AUTOCOMPLETION;
        }
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("activate"), _retval);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::DoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if ( element )
    {
      element->Focus();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

void nsHTMLTextFieldAccessible::SetEditor(nsIEditor* aEditor)
{
  mEditor = aEditor;
  if (mEditor)
    mEditor->AddEditActionListener(this);
}

void nsHTMLTextFieldAccessible::CheckForEditor()
{
  nsCOMPtr<nsIDOMNSEditableElement> editableElt(do_QueryInterface(mDOMNode));
  if (!editableElt) {
    return;
  }

  nsCOMPtr<nsIEditor> editor;
  nsresult rv = editableElt->GetEditor(getter_AddRefs(editor));
  if (NS_SUCCEEDED(rv)) {
    SetEditor(editor);
  }
}

// --- groupbox  -----

/*
 * The HTML for this is <fieldset> <legend>box-title</legend> form elements </fieldset> 
 */

nsHTMLGroupboxAccessible::nsHTMLGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessibleWrap(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetState(PRUint32 *_retval)
{
  // Groupbox doesn't support any states!
  *_retval = 0;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetName(nsAString& aName)
{
  if (mRoleMapEntry) {
    nsAccessible::GetName(aName);
    if (!aName.IsEmpty()) {
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (element) {
    nsCOMPtr<nsIDOMNodeList> legends;
    nsAutoString nameSpaceURI;
    element->GetNamespaceURI(nameSpaceURI);
    element->GetElementsByTagNameNS(nameSpaceURI, NS_LITERAL_STRING("legend"),
                                  getter_AddRefs(legends));
    if (legends) {
      nsCOMPtr<nsIDOMNode> legendNode;
      legends->Item(0, getter_AddRefs(legendNode));
      nsCOMPtr<nsIContent> legendContent(do_QueryInterface(legendNode));
      if (legendContent) {
        aName.Truncate();  // Default name is blank 
        return AppendFlatStringFromSubtree(legendContent, &aName);
      }
    }
  }
  return NS_OK;
}

void nsHTMLGroupboxAccessible::CacheChildren()
{
  if (!mWeakShell) {
    // This node has been shut down
    mAccChildCount = eChildCountUninitialized;
    return;
  }

  if (mAccChildCount == eChildCountUninitialized) {
    PRBool allowsAnonChildren = PR_FALSE;
    GetAllowsAnonChildAccessibles(&allowsAnonChildren);
    nsAccessibleTreeWalker walker(mWeakShell, mDOMNode, allowsAnonChildren);
    walker.mState.frame = GetFrame();
    PRInt32 childCount = 0;
    walker.GetFirstChild();
    // Check for <legend> and skip it if it's there
    if (walker.mState.accessible && walker.mState.domNode) {
      nsCOMPtr<nsIDOMNode> mightBeLegendNode;
      walker.mState.domNode->GetParentNode(getter_AddRefs(mightBeLegendNode));
      nsCOMPtr<nsIDOMHTMLLegendElement> legend(do_QueryInterface(mightBeLegendNode));
      if (legend) {
        walker.GetNextSibling();      // Skip the legend
      }
    }
    SetFirstChild(walker.mState.accessible);
    nsCOMPtr<nsPIAccessible> privatePrevAccessible;
    while (walker.mState.accessible) {
      ++ childCount;
      privatePrevAccessible = do_QueryInterface(walker.mState.accessible);
      privatePrevAccessible->SetParent(this);
      walker.GetNextSibling();
      privatePrevAccessible->SetNextSibling(walker.mState.accessible);
    }
    mAccChildCount = childCount;
  }
}
