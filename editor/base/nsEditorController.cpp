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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsEditorController.h"
#include "nsIGfxTextControlFrame.h"
#include "nsIEditor.h"
#include "nsIFormControlFrame.h"
#include "nsIDOMSelection.h"

#include "nsISelectionController.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

//USED FOR DEPRICATED CALL...
//#include "nsGenericHTMLElement.h"

NS_IMPL_ADDREF(nsEditorController)
NS_IMPL_RELEASE(nsEditorController)

NS_INTERFACE_MAP_BEGIN(nsEditorController)
	NS_INTERFACE_MAP_ENTRY(nsIController)
	NS_INTERFACE_MAP_ENTRY(nsIEditorController)
	NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIEditorController)
NS_INTERFACE_MAP_END

nsEditorController::nsEditorController()
{
  NS_INIT_REFCNT();
  mContent = nsnull;
  mEditor = nsnull;
  mUndoString   = "cmd_undo";
  mRedoString   = "cmd_redo";
  mCutString    = "cmd_cut";
  mCopyString   = "cmd_copy";
  mPasteString  = "cmd_paste";

  mDeleteString = "cmd_delete";
  mSelectAllString = "cmd_selectAll";
  mBeginLineString = "cmd_beginLine";
  mEndLineString   = "cmd_endLine";
  mSelectBeginLineString = "cmd_selectBeginLine";
  mSelectEndLineString   = "cmd_selectEndLine";

  mScrollTopString = "cmd_scrollTop";
  mScrollBottomString = "cmd_scrollBottom";

  mMoveTopString   = "cmd_moveTop";
  mMoveBottomString= "cmd_moveBottom";
  mSelectMoveTopString   = "cmd_selectTop";
  mSelectMoveBottomString= "cmd_selectBottom";

  mLineNextString      = "cmd_lineNext";
  mLinePreviousString        = "cmd_linePrevious";
  mSelectLineNextString = "cmd_selectLineNext";
  mSelectLinePreviousString   = "cmd_selectLinePrevious";

  mLeftString      = "cmd_charPrevious";
  mRightString     = "cmd_charNext";
  mSelectLeftString = "cmd_selectCharPrevious";
  mSelectRightString= "cmd_selectCharNext";


  mWordLeftString      = "cmd_wordPrevious";
  mWordRightString     = "cmd_wordNext";
  mSelectWordLeftString = "cmd_selectWordPrevious";
  mSelectWordRightString= "cmd_selectWordNext";

  mScrollPageUp    = "cmd_scrollPageUp";
  mScrollPageDown  = "cmd_scrollPageDown";
  mScrollLineUp    = "cmd_scrollLineUp";
  mScrollLineDown  = "cmd_scrollLineDown";

  mMovePageUp    = "cmd_scrollPageUp";
  mMovePageDown  = "cmd_scrollPageDown";
  mSelectMovePageUp     = "cmd_selectPageUp";
  mSelectMovePageDown   = "cmd_selectPageDown";

  mDeleteCharBackward = "cmd_deleteCharBackward";
  mDeleteCharForward = "cmd_deleteCharForward";
  mDeleteWordBackward = "cmd_deleteWordBackward";
  mDeleteWordForward = "cmd_deleteWordForward";

  mDeleteToBeginningOfLine = "cmd_deleteToBeginningOfLine";
  mDeleteToEndOfLine = "cmd_deleteToEndOfLine";
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

NS_IMETHODIMP nsEditorController::SetEditor(nsIEditor *aEditor)
{
  // null editors are allowed
  mEditor = aEditor;
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
      (PR_TRUE==mDeleteCharForward.Equals(aCommand)) ||
      (PR_TRUE==mDeleteCharBackward.Equals(aCommand)) ||
      (PR_TRUE==mDeleteWordForward.Equals(aCommand)) ||
      (PR_TRUE==mDeleteWordBackward.Equals(aCommand)) ||
      (PR_TRUE==mDeleteToBeginningOfLine.Equals(aCommand)) ||
      (PR_TRUE==mDeleteToEndOfLine.Equals(aCommand)) ||
      (PR_TRUE==mSelectAllString.Equals(aCommand)) ||
      (PR_TRUE==mBeginLineString.Equals(aCommand)) ||
      (PR_TRUE==mEndLineString.Equals(aCommand)) ||
      (PR_TRUE==mSelectBeginLineString.Equals(aCommand)) ||
      (PR_TRUE==mSelectEndLineString.Equals(aCommand)) ||
      (PR_TRUE==mScrollTopString.Equals(aCommand)) ||
      (PR_TRUE==mScrollBottomString.Equals(aCommand)) ||
      (PR_TRUE==mMoveTopString.Equals(aCommand)) ||
      (PR_TRUE==mMoveBottomString.Equals(aCommand)) ||
      (PR_TRUE==mSelectMoveTopString.Equals(aCommand)) ||
      (PR_TRUE==mSelectMoveBottomString.Equals(aCommand)) ||
      (PR_TRUE==mLineNextString.Equals(aCommand)) ||
      (PR_TRUE==mLinePreviousString.Equals(aCommand)) ||
      (PR_TRUE==mLeftString.Equals(aCommand)) ||
      (PR_TRUE==mRightString.Equals(aCommand)) ||
      (PR_TRUE==mSelectLineNextString.Equals(aCommand)) ||
      (PR_TRUE==mSelectLinePreviousString.Equals(aCommand)) ||
      (PR_TRUE==mSelectLeftString.Equals(aCommand)) ||
      (PR_TRUE==mSelectRightString.Equals(aCommand)) ||
      (PR_TRUE==mWordLeftString.Equals(aCommand)) ||
      (PR_TRUE==mWordRightString.Equals(aCommand)) ||
      (PR_TRUE==mSelectWordLeftString.Equals(aCommand)) ||
      (PR_TRUE==mSelectWordRightString.Equals(aCommand)) ||
      (PR_TRUE==mScrollPageUp.Equals(aCommand)) ||
      (PR_TRUE==mScrollPageDown.Equals(aCommand)) ||
      (PR_TRUE==mScrollLineUp.Equals(aCommand)) ||
      (PR_TRUE==mScrollLineDown.Equals(aCommand)) ||
      (PR_TRUE==mMovePageUp.Equals(aCommand)) ||
      (PR_TRUE==mMovePageDown.Equals(aCommand)) ||
      (PR_TRUE==mSelectMovePageUp.Equals(aCommand)) ||
      (PR_TRUE==mSelectMovePageDown.Equals(aCommand))
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
  nsCOMPtr<nsISelectionController> selCont;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  if (!editor)
  { // Q: What does it mean if there is no editor?  
    // A: It means we've never had focus, so we can't do anything
    return NS_OK;
  }

  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (PR_TRUE==mUndoString.Equals(aCommand))
  {
    rv = editor->Undo(1);
  }
  else if (PR_TRUE==mRedoString.Equals(aCommand))
  {
    rv = editor->Redo(1);
  }
  else if (PR_TRUE==mCutString.Equals(aCommand))
  { 
    rv = editor->Cut();
  }
  else if (PR_TRUE==mCopyString.Equals(aCommand))
  { 
    rv = editor->Copy();
  }
  else if (PR_TRUE==mPasteString.Equals(aCommand))
  { 
    rv = editor->Paste();
  }
  else if (PR_TRUE==mDeleteString.Equals(aCommand))
  { 
    rv = editor->DeleteSelection(nsIEditor::eNext);
  }
  else if (PR_TRUE==mSelectAllString.Equals(aCommand))    //SelectALL
  { 
    rv = editor->SelectAll();
  }

  else if (PR_TRUE==mDeleteCharForward.Equals(aCommand))
  {
    rv = editor->DeleteSelection(nsIEditor::eNext);
  }
  else if (PR_TRUE==mDeleteCharBackward.Equals(aCommand))
  { 
    rv = editor->DeleteSelection(nsIEditor::ePrevious);
  }
  else if (PR_TRUE==mDeleteWordForward.Equals(aCommand))
  { 
    rv = editor->DeleteSelection(nsIEditor::eNextWord);
  }
  else if (PR_TRUE==mDeleteWordBackward.Equals(aCommand))
  { 
    rv = editor->DeleteSelection(nsIEditor::ePreviousWord);
  }

  else if (PR_TRUE==mDeleteToBeginningOfLine.Equals(aCommand))
  {
    rv = editor->DeleteSelection(nsIEditor::eToBeginningOfLine);
  }
  else if (PR_TRUE==mDeleteToEndOfLine.Equals(aCommand))
  { 
    rv = editor->DeleteSelection(nsIEditor::eToEndOfLine);
  }

  else if (PR_TRUE==mScrollTopString.Equals(aCommand))    //ScrollTOP
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteScroll(PR_FALSE);
  }
  else if (PR_TRUE==mScrollBottomString.Equals(aCommand))    //ScrollBOTTOM
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteScroll(PR_TRUE);
  }
  else if (PR_TRUE==mMoveTopString.Equals(aCommand)) //MoveTop
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mMoveBottomString.Equals(aCommand)) //MoveBottom
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectMoveTopString.Equals(aCommand)) // SelectMoveTop
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectMoveBottomString.Equals(aCommand)) //SelectMoveBottom
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_TRUE,PR_TRUE);
  }
  else if (PR_TRUE==mLineNextString.Equals(aCommand))    //DOWN
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mLinePreviousString.Equals(aCommand))    //UP
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectLineNextString.Equals(aCommand))    //SelectDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_TRUE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectLinePreviousString.Equals(aCommand))    //SelectUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mLeftString.Equals(aCommand))    //LeftChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mRightString.Equals(aCommand))    //Right char
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectLeftString.Equals(aCommand))    //SelectLeftChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectRightString.Equals(aCommand))    //SelectRightChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_TRUE,PR_TRUE);
  }
  else if (PR_TRUE==mBeginLineString.Equals(aCommand))  //BeginLine 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mEndLineString.Equals(aCommand))    //EndLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectBeginLineString.Equals(aCommand))    //SelectBeginLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectEndLineString.Equals(aCommand))    //SelectEndLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_TRUE,PR_TRUE);
  }
  else if (PR_TRUE==mWordLeftString.Equals(aCommand))  //LeftWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mWordRightString.Equals(aCommand))  //RightWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectWordLeftString.Equals(aCommand))  //SelectLeftWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectWordRightString.Equals(aCommand))  //SelectRightWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_TRUE,PR_TRUE);
  }
  else if (PR_TRUE==mScrollPageUp.Equals(aCommand))  //ScrollPageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollPage(PR_FALSE);
  }
  else if (PR_TRUE==mScrollPageDown.Equals(aCommand))  //ScrollPageDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollPage(PR_TRUE);
  }
  else if (PR_TRUE==mScrollLineUp.Equals(aCommand))  //ScrollLineUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollLine(PR_FALSE);
  }
  else if (PR_TRUE==mScrollLineDown.Equals(aCommand))  //ScrollLineDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollLine(PR_TRUE);
  }
  else if (PR_TRUE==mMovePageUp.Equals(aCommand))  //MovePageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_FALSE,PR_FALSE);
  }
  else if (PR_TRUE==mMovePageDown.Equals(aCommand))  //MovePageDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_TRUE,PR_FALSE);
  }
  else if (PR_TRUE==mSelectMovePageUp.Equals(aCommand))  //SelectMovePageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_FALSE,PR_TRUE);
  }
  else if (PR_TRUE==mSelectMovePageDown.Equals(aCommand))  //SelectMovePageDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_TRUE,PR_TRUE);
  }

  // For debug builds, we might want to print or return rv here.
  // For release builds, though, we don't want to be spewing JS errors
  // for things like "right arrow when already at end of line".
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
  if (mEditor)
  {
    *aEditor = mEditor;
    NS_ADDREF(*aEditor);
    return NS_OK; 
  }

/*  nsIGfxTextControlFrame *frame;
  NS_ENSURE_SUCCESS(GetFrame(&frame), NS_ERROR_FAILURE);
  if (!frame) { return NS_ERROR_FAILURE; }

  NS_ENSURE_SUCCESS(frame->GetEditor(aEditor), NS_ERROR_FAILURE);
 */
  return NS_OK;
}

NS_IMETHODIMP nsEditorController::GetSelectionController(nsISelectionController ** aSelCon)
{
  nsCOMPtr<nsIEditor>editor;
  nsresult result = GetEditor(getter_AddRefs(editor));
  if (NS_FAILED(result) || !editor)
    return result ? result : NS_ERROR_FAILURE;

  nsCOMPtr<nsIPresShell> presShell;
  result = editor->GetPresShell(getter_AddRefs(presShell)); 
  if (NS_FAILED(result) || !presShell)
    return result ? result : NS_ERROR_FAILURE;
  
  nsCOMPtr<nsISelectionController> selController = do_QueryInterface(presShell); 
  if (selController)
  {
    *aSelCon = selController;
    NS_ADDREF(*aSelCon);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
/*
  NS_ENSURE_ARG_POINTER(aSelCon);
  nsCOMPtr<nsIDocument> doc; 
  mContent->GetDocument(*getter_AddRefs(doc)); 

  *aSelCon = nsnull;
  if (doc)
  {
    PRInt32 i = doc->GetNumberOfShells(); 
    if (i == 0) 
      return NS_ERROR_FAILURE; 

    nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(doc->GetShellAt(0)); 
    nsCOMPtr<nsISelectionController> selController = do_QueryInterface(presShell); 
    if (selController)
    {
      *aSelCon = selController;
      (*aSelCon)->AddRef();
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
  */
}

NS_IMETHODIMP nsEditorController::GetFrame(nsIGfxTextControlFrame **aFrame)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  //DEPRICATED. WILL REMOVE LATER
/*
  *aFrame = nsnull;
  NS_ENSURE_STATE(mContent);

  nsIFormControlFrame *frame = nsnull;
  NS_ENSURE_SUCCESS( 
    nsGenericHTMLElement::GetPrimaryFrame(mContent, frame), 
    NS_ERROR_FAILURE
  );
  if (!frame) { return NS_ERROR_FAILURE; }

  NS_ENSURE_SUCCESS(
    frame->QueryInterface(NS_GET_IID(nsIGfxTextControlFrame), (void**)aFrame), 
    NS_ERROR_FAILURE
  );
  */
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
