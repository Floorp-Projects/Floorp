/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsEditorController.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIEditor.h"
#include "nsIFormControlFrame.h"
#include "nsGenericHTMLElement.h"
#include "nsIDOMSelection.h"

NS_IMPL_ADDREF(nsEditorController)
NS_IMPL_RELEASE(nsEditorController)

NS_IMPL_QUERY_INTERFACE1(nsEditorController, nsIController)

nsEditorController::nsEditorController()
{
  NS_INIT_REFCNT();
  mContent = nsnull;
  mUndoString   = "cmd_undo";
  mRedoString   = "cmd_redo";
  mCutString    = "cmd_cut";
  mCopyString   = "cmd_copy";
  mPasteString  = "cmd_paste";
  mDeleteString = "cmd_delete";
  mSelectAllString = "cmd_selectAll";
}

nsEditorController::~nsEditorController()
{
}

NS_IMETHODIMP nsEditorController::SetContent(nsIHTMLContent *aContent)
{
  // indiscriminately sets mContent, no ref counting here
  mContent = aContent;
  return NS_OK;
}


/* =======================================================================
 * nsIController
 * ======================================================================= */

NS_IMETHODIMP nsEditorController::IsCommandEnabled(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = PR_FALSE;

  nsCOMPtr<nsIEditor> editor;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  // a null editor is a legal state

  if (PR_TRUE==mUndoString.Equals(aCommand))
  { // we can undo if the editor says we can undo
    if (editor)
    {
      PRBool isEnabled;
      NS_ENSURE_SUCCESS(editor->CanUndo(isEnabled, *aResult), NS_ERROR_FAILURE);
    }
  }
  else if (PR_TRUE==mRedoString.Equals(aCommand))
  { // we can redo if the editor says we can undo
    if (editor)
    {
      PRBool isEnabled;
      NS_ENSURE_SUCCESS(editor->CanRedo(isEnabled, *aResult), NS_ERROR_FAILURE);
    }
  }
  else if (PR_TRUE==mCutString.Equals(aCommand))
  { // we can cut if the editor has a non-collapsed selection and is not readonly
    if (editor)
    {
      nsCOMPtr<nsIDOMSelection> selection;
      NS_ENSURE_SUCCESS(editor->GetSelection(getter_AddRefs(selection)), NS_ERROR_FAILURE);
      if (selection)
      {
        PRBool collapsed;
        NS_ENSURE_SUCCESS(selection->GetIsCollapsed(&collapsed), NS_ERROR_FAILURE);
        if ((PR_FALSE==collapsed) && (PR_TRUE==IsEnabled())) {
          *aResult = PR_TRUE;
        }
      }
    }
  }
  else if (PR_TRUE==mCopyString.Equals(aCommand))
  { // we can copy if the editor has a non-collapsed selection
    if (editor)
    {
      nsCOMPtr<nsIDOMSelection> selection;
      NS_ENSURE_SUCCESS(editor->GetSelection(getter_AddRefs(selection)), NS_ERROR_FAILURE);
      if (selection)
      {
        PRBool collapsed;
        NS_ENSURE_SUCCESS(selection->GetIsCollapsed(&collapsed), NS_ERROR_FAILURE);
        if (PR_FALSE==collapsed) {
          *aResult = PR_TRUE;
        }
      }
    }
  }
  else if (PR_TRUE==mPasteString.Equals(aCommand))
  { // paste is enabled if there is text on the clipbard and if the editor is not readonly or disabled
    // XXX: implement me
    *aResult = PR_TRUE;
  }
  else if (PR_TRUE==mDeleteString.Equals(aCommand))
  { // delete is enabled if there is any content and if the editor is not readonly or disabled
    *aResult = PR_TRUE;
  }
  else if (PR_TRUE==mSelectAllString.Equals(aCommand))    
  { // selectAll is enabled if there is any content and if the editor is not disabled (readonly is ok)
    *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditorController::SupportsCommand(const PRUnichar *aCommand, PRBool *aResult)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  NS_ENSURE_ARG_POINTER(aResult);

  // XXX: need to check the readonly and disabled states

  *aResult = PR_FALSE;
  if ((PR_TRUE==mUndoString.Equals(aCommand)) ||
      (PR_TRUE==mRedoString.Equals(aCommand)) ||
      (PR_TRUE==mCutString.Equals(aCommand)) ||
      (PR_TRUE==mCopyString.Equals(aCommand)) ||
      (PR_TRUE==mPasteString.Equals(aCommand)) ||
      (PR_TRUE==mDeleteString.Equals(aCommand)) ||
      (PR_TRUE==mSelectAllString.Equals(aCommand))    
    )
  {
    *aResult = PR_TRUE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditorController::DoCommand(const PRUnichar *aCommand)
{
  NS_ENSURE_ARG_POINTER(aCommand);
  nsCOMPtr<nsIEditor> editor;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  if (!editor)
  { // Q: What does it mean if there is no editor?  
    // A: It means we've never had focus, so we can't do anything
    return NS_OK;
  }

  if (PR_TRUE==mUndoString.Equals(aCommand))
  {
    NS_ENSURE_SUCCESS(editor->Undo(1), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mRedoString.Equals(aCommand))
  {
    NS_ENSURE_SUCCESS(editor->Redo(1), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mCutString.Equals(aCommand))
  { 
    NS_ENSURE_SUCCESS(editor->Cut(), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mCopyString.Equals(aCommand))
  { 
    NS_ENSURE_SUCCESS(editor->Copy(), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mPasteString.Equals(aCommand))
  { 
    NS_ENSURE_SUCCESS(editor->Paste(), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mDeleteString.Equals(aCommand))
  { 
    NS_ENSURE_SUCCESS(editor->DeleteSelection(nsIEditor::eDeleteNext), NS_ERROR_FAILURE);
  }
  else if (PR_TRUE==mSelectAllString.Equals(aCommand))    
  { 
    NS_ENSURE_SUCCESS(editor->SelectAll(), NS_ERROR_FAILURE);
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditorController::OnEvent(const PRUnichar *aEventName)
{
  NS_ENSURE_ARG_POINTER(aEventName);

  return NS_OK;
}




NS_IMETHODIMP nsEditorController::GetEditor(nsIEditor ** aEditor)
{
  NS_ENSURE_ARG_POINTER(aEditor);

  nsIGfxTextControlFrame *frame;
  NS_ENSURE_SUCCESS(GetFrame(&frame), NS_ERROR_FAILURE);
  if (!frame) { return NS_ERROR_FAILURE; }

  NS_ENSURE_SUCCESS(frame->GetEditor(aEditor), NS_ERROR_FAILURE);
  
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::GetFrame(nsIGfxTextControlFrame **aFrame)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  *aFrame = nsnull;
  NS_ENSURE_STATE(mContent);

  nsIFormControlFrame *frame = nsnull;
  NS_ENSURE_SUCCESS( 
    nsGenericHTMLElement::GetPrimaryFrame(mContent, frame), 
    NS_ERROR_FAILURE
  );
  if (!frame) { return NS_ERROR_FAILURE; }

  NS_ENSURE_SUCCESS(
    frame->QueryInterface(nsIGfxTextControlFrame::GetIID(), (void**)aFrame), 
    NS_ERROR_FAILURE
  );
  
  return NS_OK;
}

PRBool nsEditorController::IsEnabled()
{
  return PR_TRUE; // XXX: need to implement this
  /*
      PRUint32 flags=0;
      NS_ENSURE_SUCCESS(mEditor->GetFlags(&flags), NS_ERROR_FAILURE);
      check eEditorReadonlyBit and eEditorDisabledBit
   */
}
