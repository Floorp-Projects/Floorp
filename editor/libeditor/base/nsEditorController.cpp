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
#include "nsIHTMLEditor.h"
#include "nsISupportsPrimitives.h"

#include "nsISelectionController.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"

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
  
  mUndoCmd            = getter_AddRefs(NS_NewAtom("cmd_undo"));
  mRedoCmd            = getter_AddRefs(NS_NewAtom("cmd_redo"));
  mCutCmd             = getter_AddRefs(NS_NewAtom("cmd_cut"));
  mCopyCmd            = getter_AddRefs(NS_NewAtom("cmd_copy"));
  mPasteCmd           = getter_AddRefs(NS_NewAtom("cmd_paste"));
  mDeleteCmd          = getter_AddRefs(NS_NewAtom("cmd_delete"));
  mSelectAllCmd       = getter_AddRefs(NS_NewAtom("cmd_selectAll"));

  mBeginLineCmd       = getter_AddRefs(NS_NewAtom("cmd_beginLine"));
  mEndLineCmd         = getter_AddRefs(NS_NewAtom("cmd_endLine"));
  mSelectBeginLineCmd = getter_AddRefs(NS_NewAtom("cmd_selectBeginLine"));
  mSelectEndLineCmd   = getter_AddRefs(NS_NewAtom("cmd_selectEndLine"));

  mScrollTopCmd       = getter_AddRefs(NS_NewAtom("cmd_scrollTop"));
  mScrollBottomCmd    = getter_AddRefs(NS_NewAtom("cmd_scrollBottom"));

  mMoveTopCmd         = getter_AddRefs(NS_NewAtom("cmd_moveTop"));
  mMoveBottomCmd      = getter_AddRefs(NS_NewAtom("cmd_moveBottom"));
  mSelectMoveTopCmd   = getter_AddRefs(NS_NewAtom("cmd_selectTop"));
  mSelectMoveBottomCmd = getter_AddRefs(NS_NewAtom("cmd_selectBottom"));

  mLineNextCmd        = getter_AddRefs(NS_NewAtom("cmd_lineNext"));
  mLinePreviousCmd    = getter_AddRefs(NS_NewAtom("cmd_linePrevious"));
  mSelectLineNextCmd  = getter_AddRefs(NS_NewAtom("cmd_selectLineNext"));
  mSelectLinePreviousCmd = getter_AddRefs(NS_NewAtom("cmd_selectLinePrevious"));

  mLeftCmd            = getter_AddRefs(NS_NewAtom("cmd_charPrevious"));
  mRightCmd           = getter_AddRefs(NS_NewAtom("cmd_charNext"));
  mSelectLeftCmd      = getter_AddRefs(NS_NewAtom("cmd_selectCharPrevious"));
  mSelectRightCmd     = getter_AddRefs(NS_NewAtom("cmd_selectCharNext"));

  mWordLeftCmd        = getter_AddRefs(NS_NewAtom("cmd_wordPrevious"));
  mWordRightCmd       = getter_AddRefs(NS_NewAtom("cmd_wordNext"));
  mSelectWordLeftCmd  = getter_AddRefs(NS_NewAtom("cmd_selectWordPrevious"));
  mSelectWordRightCmd = getter_AddRefs(NS_NewAtom("cmd_selectWordNext"));

  mScrollPageUpCmd    = getter_AddRefs(NS_NewAtom("cmd_scrollPageUp"));
  mScrollPageDownCmd  = getter_AddRefs(NS_NewAtom("cmd_scrollPageDown"));
  mScrollLineUpCmd    = getter_AddRefs(NS_NewAtom("cmd_scrollLineUp"));
  mScrollLineDownCmd  = getter_AddRefs(NS_NewAtom("cmd_scrollLineDown"));

  mMovePageUpCmd         = getter_AddRefs(NS_NewAtom("cmd_scrollPageUp"));
  mMovePageDownCmd       = getter_AddRefs(NS_NewAtom("cmd_scrollPageDown"));
  mSelectMovePageUpCmd   = getter_AddRefs(NS_NewAtom("cmd_selectPageUp"));
  mSelectMovePageDownCmd = getter_AddRefs(NS_NewAtom("cmd_selectPageDown"));

  mDeleteCharBackwardCmd = getter_AddRefs(NS_NewAtom("cmd_deleteCharBackward"));
  mDeleteCharForwardCmd  = getter_AddRefs(NS_NewAtom("cmd_deleteCharForward"));
  mDeleteWordBackwardCmd = getter_AddRefs(NS_NewAtom("cmd_deleteWordBackward"));
  mDeleteWordForwardCmd  = getter_AddRefs(NS_NewAtom("cmd_deleteWordForward"));

  mDeleteToBeginningOfLineCmd = getter_AddRefs(NS_NewAtom("cmd_deleteToBeginningOfLine"));
  mDeleteToEndOfLineCmd = getter_AddRefs(NS_NewAtom("cmd_deleteToEndOfLine"));
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

  nsCOMPtr<nsIAtom> cmdAtom = getter_AddRefs(NS_NewAtom(aCommand));
  
  nsCOMPtr<nsIEditor> editor;
  NS_ENSURE_SUCCESS(GetEditor(getter_AddRefs(editor)), NS_ERROR_FAILURE);
  // a null editor is a legal state

  if (cmdAtom == mUndoCmd)
  { // we can undo if the editor says we can undo
    if (editor)
    {
      PRBool isEnabled;
      NS_ENSURE_SUCCESS(editor->CanUndo(isEnabled, *aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mRedoCmd)
  { // we can redo if the editor says we can undo
    if (editor)
    {
      PRBool isEnabled;
      NS_ENSURE_SUCCESS(editor->CanRedo(isEnabled, *aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mCutCmd)
  { // we can cut if the editor has a non-collapsed selection and is not readonly
    if (editor)
    {
      NS_ENSURE_SUCCESS(editor->CanCut(*aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mCopyCmd)
  { // we can copy if the editor has a non-collapsed selection
    if (editor)
    {
      NS_ENSURE_SUCCESS(editor->CanCopy(*aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mPasteCmd)
  {
    if (editor)
    {
      NS_ENSURE_SUCCESS(editor->CanPaste(*aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mDeleteCmd)
  { // we can delete if the editor has a non-collapsed selection and is modifiable (same as cut)
    if (editor)
    {
      NS_ENSURE_SUCCESS(editor->CanCut(*aResult), NS_ERROR_FAILURE);
    }
  }
  else if (cmdAtom == mSelectAllCmd)    
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
  nsCOMPtr<nsIAtom> cmdAtom = getter_AddRefs(NS_NewAtom(aCommand));
  
  if (
    (cmdAtom == mUndoCmd) ||
    (cmdAtom == mRedoCmd) ||
    (cmdAtom == mCutCmd) ||
    (cmdAtom == mCopyCmd) ||
    (cmdAtom == mPasteCmd) ||
    (cmdAtom == mDeleteCmd) ||
    (cmdAtom == mSelectAllCmd) ||
    (cmdAtom == mBeginLineCmd) ||
    (cmdAtom == mEndLineCmd) ||
    (cmdAtom == mSelectBeginLineCmd) ||
    (cmdAtom == mSelectEndLineCmd) ||
    (cmdAtom == mScrollTopCmd) ||
    (cmdAtom == mScrollBottomCmd) ||
    (cmdAtom == mMoveTopCmd) ||
    (cmdAtom == mMoveBottomCmd) ||
    (cmdAtom == mSelectMoveTopCmd) ||
    (cmdAtom == mSelectMoveBottomCmd) ||
    (cmdAtom == mLineNextCmd) ||
    (cmdAtom == mLinePreviousCmd) ||
    (cmdAtom == mLeftCmd) ||
    (cmdAtom == mRightCmd) ||
    (cmdAtom == mSelectLineNextCmd) ||
    (cmdAtom == mSelectLinePreviousCmd) ||
    (cmdAtom == mSelectLeftCmd) ||
    (cmdAtom == mSelectRightCmd) ||
    (cmdAtom == mWordLeftCmd) ||
    (cmdAtom == mWordRightCmd) ||
    (cmdAtom == mSelectWordLeftCmd) ||
    (cmdAtom == mSelectWordRightCmd) ||
    (cmdAtom == mScrollPageUpCmd) ||
    (cmdAtom == mScrollPageDownCmd) ||
    (cmdAtom == mScrollLineUpCmd) ||
    (cmdAtom == mScrollLineDownCmd) ||
    (cmdAtom == mMovePageUpCmd) ||
    (cmdAtom == mMovePageDownCmd) ||
    (cmdAtom == mSelectMovePageUpCmd) ||
    (cmdAtom == mSelectMovePageDownCmd) ||
    (cmdAtom == mDeleteCharForwardCmd) ||
    (cmdAtom == mDeleteCharBackwardCmd) ||
    (cmdAtom == mDeleteWordForwardCmd) ||
    (cmdAtom == mDeleteWordBackwardCmd) ||
    (cmdAtom == mDeleteToBeginningOfLineCmd) ||
    (cmdAtom == mDeleteToEndOfLineCmd)
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

  nsCOMPtr<nsIAtom> cmdAtom = getter_AddRefs(NS_NewAtom(aCommand));

  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (cmdAtom == mUndoCmd)
  {
    rv = editor->Undo(1);
  }
  else if (cmdAtom == mRedoCmd)
  {
    rv = editor->Redo(1);
  }
  else if (cmdAtom == mCutCmd)
  { 
    rv = editor->Cut();
  }
  else if (cmdAtom == mCopyCmd)
  { 
    rv = editor->Copy();
  }
  else if (cmdAtom == mPasteCmd)
  { 
    rv = editor->Paste();
  }
  else if (cmdAtom == mDeleteCmd)
  { 
    rv = editor->DeleteSelection(nsIEditor::eNext);
  }
  else if (cmdAtom == mSelectAllCmd)    //SelectALL
  { 
    rv = editor->SelectAll();
  }

  else if (cmdAtom == mDeleteCharForwardCmd)
  {
    rv = editor->DeleteSelection(nsIEditor::eNext);
  }
  else if (cmdAtom == mDeleteCharBackwardCmd)
  { 
    rv = editor->DeleteSelection(nsIEditor::ePrevious);
  }
  else if (cmdAtom == mDeleteWordForwardCmd)
  { 
    rv = editor->DeleteSelection(nsIEditor::eNextWord);
  }
  else if (cmdAtom == mDeleteWordBackwardCmd)
  { 
    rv = editor->DeleteSelection(nsIEditor::ePreviousWord);
  }

  else if (cmdAtom == mDeleteToBeginningOfLineCmd)
  {
    rv = editor->DeleteSelection(nsIEditor::eToBeginningOfLine);
  }
  else if (cmdAtom == mDeleteToEndOfLineCmd)
  { 
    rv = editor->DeleteSelection(nsIEditor::eToEndOfLine);
  }

  else if (cmdAtom == mScrollTopCmd)    //ScrollTOP
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteScroll(PR_FALSE);
  }
  else if (cmdAtom == mScrollBottomCmd)    //ScrollBOTTOM
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteScroll(PR_TRUE);
  }
  else if (cmdAtom == mMoveTopCmd) //MoveTop
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mMoveBottomCmd) //MoveBottom
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mSelectMoveTopCmd) // SelectMoveTop
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mSelectMoveBottomCmd) //SelectMoveBottom
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CompleteMove(PR_TRUE,PR_TRUE);
  }
  else if (cmdAtom == mLineNextCmd)    //DOWN
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mLinePreviousCmd)    //UP
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mSelectLineNextCmd)    //SelectDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_TRUE,PR_TRUE);
  }
  else if (cmdAtom == mSelectLinePreviousCmd)    //SelectUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->LineMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mLeftCmd)    //LeftChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mRightCmd)    //Right char
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mSelectLeftCmd)    //SelectLeftChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mSelectRightCmd)    //SelectRightChar
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->CharacterMove(PR_TRUE,PR_TRUE);
  }
  else if (cmdAtom == mBeginLineCmd)  //BeginLine 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mEndLineCmd)    //EndLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mSelectBeginLineCmd)    //SelectBeginLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mSelectEndLineCmd)    //SelectEndLine
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->IntraLineMove(PR_TRUE,PR_TRUE);
  }
  else if (cmdAtom == mWordLeftCmd)  //LeftWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mWordRightCmd)  //RightWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mSelectWordLeftCmd)  //SelectLeftWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mSelectWordRightCmd)  //SelectRightWord 
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->WordMove(PR_TRUE,PR_TRUE);
  }
  else if (cmdAtom == mScrollPageUpCmd)  //ScrollPageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollPage(PR_FALSE);
  }
  else if (cmdAtom == mScrollPageDownCmd)  //ScrollPageDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollPage(PR_TRUE);
  }
  else if (cmdAtom == mScrollLineUpCmd)  //ScrollLineUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollLine(PR_FALSE);
  }
  else if (cmdAtom == mScrollLineDownCmd)  //ScrollLineDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->ScrollLine(PR_TRUE);
  }
  else if (cmdAtom == mMovePageUpCmd)  //MovePageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_FALSE,PR_FALSE);
  }
  else if (cmdAtom == mMovePageDownCmd)  //MovePageDown
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_TRUE,PR_FALSE);
  }
  else if (cmdAtom == mSelectMovePageUpCmd)  //SelectMovePageUp
  { 
    rv = GetSelectionController(getter_AddRefs(selCont));
    if (NS_SUCCEEDED(rv))
      rv = selCont->PageMove(PR_FALSE,PR_TRUE);
  }
  else if (cmdAtom == mSelectMovePageDownCmd)  //SelectMovePageDown
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
