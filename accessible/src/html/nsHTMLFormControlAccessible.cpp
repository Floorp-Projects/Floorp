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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Author: Eric Vaughan (evaughan@netscape.com)
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

// NOTE: alphabetically ordered
#include "nsAccessible.h"
#include "nsFormControlAccessible.h"
#include "nsHTMLAtoms.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsIClipboard.h"
#include "nsIDOMHTMLButtonElement.h"
#include "nsIDOMHTMLFormElement.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMNSHTMLButtonElement.h"
#include "nsIDOMHTMLLabelElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIDOMXULCheckboxElement.h"
#include "nsIDOMXULButtonElement.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIEditor.h"
#include "nsIEventStateManager.h"
#include "nsIFrame.h"
#include "nsITextControlFrame.h"
#include "nsIPlaintextEditor.h"
#include "nsISelectionController.h"
#include "nsReadableUtils.h"
#include "nsWeakReference.h"

// --- checkbox -----

nsHTMLCheckboxAccessible::nsHTMLCheckboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_CHECKBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {    // 0 is the magic value for default action
    // check or uncheck
    PRUint32 state;
    GetAccState(&state);

    if (state & STATE_CHECKED)
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("uncheck"), _retval); 
    else
      nsAccessible::GetTranslatedString(NS_LITERAL_STRING("check"), _retval); 

    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {   // 0 is the magic value for default action
    nsCOMPtr<nsIDOMHTMLInputElement> htmlCheckboxElement(do_QueryInterface(mDOMNode));
    if (htmlCheckboxElement) {
      htmlCheckboxElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLCheckboxAccessible::GetAccState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetAccState(_retval);
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

NS_IMETHODIMP nsHTMLRadioButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if (element) {
      element->Click();
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLRadioButtonAccessible::GetAccState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetAccState(_retval);
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
nsFormControlAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == eAction_Click) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(mDOMNode));
    if (element) {
      element->Click();
      return NS_OK;
    }
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetAccState(PRUint32 *_retval)
{
  nsFormControlAccessible::GetAccState(_retval);
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for button node!");

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.EqualsIgnoreCase("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLButtonAccessible::GetAccName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMHTMLInputElement> button(do_QueryInterface(mDOMNode));

  if (!button)
    return NS_ERROR_FAILURE;

  nsAutoString name;
  button->GetValue(name);
  name.CompressWhitespace();
  if (name.IsEmpty()) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
    elt->GetAttribute(NS_LITERAL_STRING("title"), name);
  }

  _retval.Assign(name);

  return NS_OK;
}


// ----- HTML 4 Button: can contain arbitrary HTML content -----

nsHTML4ButtonAccessible::nsHTML4ButtonAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsLeafAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  if (index == eAction_Click) {
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("press"), _retval); 
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIDOMNSHTMLButtonElement> buttonElement(do_QueryInterface(mDOMNode));
    if ( buttonElement )
    {
      buttonElement->Click();
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetAccState(PRUint32 *_retval)
{
  nsAccessible::GetAccState(_retval);
  *_retval |= STATE_FOCUSABLE;

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  NS_ASSERTION(element, "No nsIDOMElement for button node!");

  nsAutoString buttonType;
  element->GetAttribute(NS_LITERAL_STRING("type"), buttonType);
  if (buttonType.EqualsIgnoreCase("submit"))
    *_retval |= STATE_DEFAULT;

  return NS_OK;
}

NS_IMETHODIMP nsHTML4ButtonAccessible::GetAccName(nsAString& _retval)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  nsAutoString name;
  if (content)
    rv = AppendFlatStringFromSubtree(content, &name);

  if (NS_SUCCEEDED(rv)) {
    // Temp var needed until CompressWhitespace built for nsAString
    name.CompressWhitespace();
    _retval.Assign(name);
  }

  return rv;
}


// --- textfield -----

nsHTMLTextFieldAccessible::nsHTMLTextFieldAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsFormControlAccessible(aNode, aShell)
{ 
  // In nsHTMLTextFieldAccessible, mDOMNode is a nsHTMLInputElement. But we need 
  // a *true* text node(nsTextNode) for the text operation. It's the first child
  // of our editor's root element
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell)
    return;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  nsITextControlFrame *tframe = nsnull;
  frame->QueryInterface(NS_GET_IID(nsITextControlFrame), (void**)&tframe);
  if (!tframe)
    return;

  nsCOMPtr<nsIEditor> editor;
  tframe->GetEditor(getter_AddRefs(editor));
  if (!editor)
    return;

  nsCOMPtr<nsIDOMElement> rootElement;
  editor->GetRootElement(getter_AddRefs(rootElement));
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  if (rootNode) {
    nsCOMPtr<nsIDOMNode> domNode;
    rootNode->GetFirstChild(getter_AddRefs(domNode));
    SetTextNode(domNode);
  }
}

NS_IMPL_ISUPPORTS_INHERITED2(nsHTMLTextFieldAccessible, nsFormControlAccessible, nsIAccessibleEditableText, nsAccessibleText)

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_TEXT;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetAccValue(nsAString& _retval)
{
  PRUint32 state;
  GetAccState(&state);
  if (state & STATE_PROTECTED)    // Don't return password text!
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  if (textArea) {
    textArea->GetValue(_retval);
    return NS_OK;
  }
  
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));
  if (inputElement) {
    inputElement->GetValue(_retval);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::GetAccState(PRUint32 *_retval)
{
  // can be
  // focusable, focused, protected. readonly, unavailable, selected

  nsAccessible::GetAccState(_retval);
  *_retval |= STATE_FOCUSABLE;

  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));

  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  PRBool isReadOnly = PR_FALSE;
  elt->HasAttribute(NS_LITERAL_STRING("readonly"), &isReadOnly);
  if (isReadOnly)
    *_retval |= STATE_READONLY;

  // Get current selection and find out if current node is in it
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIFrame *frame = nsnull;
  if (content && NS_SUCCEEDED(shell->GetPrimaryFrameFor(content, &frame)) && frame) {
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(context,getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel) {
        PRBool isCollapsed = PR_TRUE;
        domSel->GetIsCollapsed(&isCollapsed);
        if (!isCollapsed)
          *_retval |=STATE_SELECTED;
      }
    }
  }


  if (!textArea) {
    if (inputElement) {
      /////// ====== Must be a password field, so it uses nsIDOMHTMLFormControl ==== ///////
      PRUint32 moreStates = 0;
      nsresult rv = nsFormControlAccessible::GetAccState(&moreStates);
      *_retval |= moreStates;
      return rv;
    }
    return NS_ERROR_FAILURE;
  }

  PRBool disabled = PR_FALSE;
  textArea->GetDisabled(&disabled);
  if (disabled)
    *_retval |= STATE_UNAVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::SetAttributes(PRInt32 aStartPos, PRInt32 aEndPos, nsISupports *aAttributes)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::SetTextContents(const nsAString &aText)
{
  nsCOMPtr<nsIDOMHTMLTextAreaElement> textArea(do_QueryInterface(mDOMNode));
  if (textArea)
    return textArea->SetValue(aText);
  
  nsCOMPtr<nsIDOMHTMLInputElement> inputElement(do_QueryInterface(mDOMNode));
  if (inputElement)
    return inputElement->SetValue(aText);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::MakeSelection(PRInt32 aStartPos, PRInt32 aEndPos, nsIEditor **aEditor)
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell)
    return NS_ERROR_FAILURE;  

  AccTakeFocus();

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIFrame *frame = nsnull;
  shell->GetPrimaryFrameFor(content, &frame);
  nsITextControlFrame *tframe = nsnull;
  frame->QueryInterface(NS_GET_IID(nsITextControlFrame), (void**)&tframe);
  if (!tframe)
    return NS_ERROR_FAILURE;  

  nsCOMPtr<nsIEditor> editor;
  tframe->SetSelectionRange(aStartPos, aEndPos);
  tframe->GetEditor(getter_AddRefs(editor));
  if (!editor)
    return NS_ERROR_FAILURE;  

  *aEditor = editor;
  NS_ADDREF(*aEditor);

  return NS_OK;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::InsertText(const nsAString &aText, PRInt32 aPosition)
{
  nsCOMPtr<nsIEditor> editor;
  if (NS_SUCCEEDED(MakeSelection(aPosition, aPosition, getter_AddRefs(editor)))) {
    nsCOMPtr<nsIPlaintextEditor> peditor(do_QueryInterface(editor));
    peditor->InsertText(aText);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::CopyText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  if (NS_SUCCEEDED(MakeSelection(aStartPos, aEndPos, getter_AddRefs(editor)))) {
    editor->Copy();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::CutText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  if (NS_SUCCEEDED(MakeSelection(aStartPos, aEndPos, getter_AddRefs(editor)))) {
    editor->Cut();
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::DeleteText(PRInt32 aStartPos, PRInt32 aEndPos)
{
  nsCOMPtr<nsIEditor> editor;
  if (NS_SUCCEEDED(MakeSelection(aStartPos, aEndPos, getter_AddRefs(editor)))) {
    editor->DeleteSelection(nsIEditor::eNone);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsHTMLTextFieldAccessible::PasteText(PRInt32 aPosition)
{
  nsCOMPtr<nsIEditor> editor;
  if (NS_SUCCEEDED(MakeSelection(aPosition, aPosition, getter_AddRefs(editor)))) {
    editor->Paste(nsIClipboard::kGlobalClipboard);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

// --- groupbox  -----

/*
 * The HTML for this is <fieldset> <legend>box-title</legend> form elements </fieldset> 
 */

nsHTMLGroupboxAccessible::nsHTMLGroupboxAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_GROUPING;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetAccState(PRUint32 *_retval)
{
  // Groupbox doesn't support any states!
  *_retval = 0;

  return NS_OK;
}

NS_IMETHODIMP nsHTMLGroupboxAccessible::GetAccName(nsAString& _retval)
{
  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (element) {
    nsCOMPtr<nsIDOMNodeList> legends;
    element->GetElementsByTagName(NS_LITERAL_STRING("legend"), getter_AddRefs(legends));
    if (legends) {
      nsCOMPtr<nsIDOMNode> legendNode;
      legends->Item(0, getter_AddRefs(legendNode));
      nsCOMPtr<nsIContent> legendContent(do_QueryInterface(legendNode));
      if (legendContent) {
        _retval.Assign(NS_LITERAL_STRING(""));  // Default name is blank 
        return AppendFlatStringFromSubtree(legendContent, &_retval);
      }
    }
  }
  return NS_OK;
}
